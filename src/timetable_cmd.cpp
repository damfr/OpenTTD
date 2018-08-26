/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timetable_cmd.cpp Commands related to time tabling. */

#include "stdafx.h"
#include "command_func.h"
#include "company_func.h"
#include "date_func.h"
#include "window_func.h"
#include "string_func.h"
#include "vehicle_base.h"
#include "cmd_helper.h"

#include "table/strings.h"

#include "safeguards.h"

/**
 * Set the start date of the timetable.  This indirectly adjusts the start date
 * of all corresponding vehicles (as their start date = timetable start date + vehicle offset).
 * @param tile Not used.
 * @param flags Operation to perform.
 * @param p1 vehicle id
 * @param p2 new start date
 * @param text Not used.
 * @return The error or cost of the operation.
 */
CommandCost CmdSetTimetableStart(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Vehicle *v = Vehicle::GetIfValid(p1);
	if (v == NULL || !v->IsPrimaryVehicle()) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	/* Don't let a timetable start more than 5 years into the future or into the  past. */
	Date start_date = (Date)p2;
	if (start_date < 0 || start_date > MAX_DAY) return CMD_ERROR;
	if (start_date - _date > 5 * DAYS_IN_LEAP_YEAR) return CMD_ERROR;
	if (_date - start_date > 5 * DAYS_IN_LEAP_YEAR) return CMD_ERROR;
	if (v->orders.list == NULL && !OrderList::CanAllocateItem()) return CMD_ERROR;

	if (flags & DC_EXEC) {
		// If there is not yet an orderlist, create it
		if (v->orders.list == NULL) {
			v->orders.list = new OrderList(NULL, v);
		}

		v->orders.list->SetStartTime(start_date);

		SetWindowClassesDirty(WC_VEHICLE_TIMETABLE);
	}

	return CommandCost();
}

/**
 * Set the offset of a vehicle relative to a timetable.
 * @param tile Not used.
 * @param flags Operation to perform.
 * @param p1 Bits 0..15: Vehicle id.
 *           Bits 16..23: Unit of timetable offset
 * @param p2 Length of the new timetable offset
 * @param text Not used.
 * @return The error or cost of the operation.
 */
CommandCost CmdSetTimetableOffset(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	VehicleID vehicle_id = (VehicleID)(p1 & 0x0000FFFF);
	Vehicle *v = Vehicle::GetIfValid(vehicle_id);
	if (v == NULL || !v->IsPrimaryVehicle()) return CMD_ERROR;

	DurationUnit duration_unit = (DurationUnit)((p1 >> 16) & 0x000000FF);
	if (duration_unit != DU_DAYS && duration_unit != DU_MONTHS && duration_unit != DU_YEARS && duration_unit != DU_INVALID) {
		return CMD_ERROR;
	}

	int32 offset = (int32)p2;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	if (v->orders.list == NULL && !OrderList::CanAllocateItem()) return CMD_ERROR;

	if (flags & DC_EXEC) {
		/* If there is not yet an orderlist, create it */
		if (v->orders.list == NULL) {
			v->orders.list = new OrderList(NULL, v);
		}

		v->SetTimetableOffset(Duration(offset, duration_unit));

		SetWindowDirty(WC_VEHICLE_TIMETABLE, v->index);
	}

	return CommandCost();
}

/**
 * Set the length of a timetable.
 * @param tile Not used.
 * @param flags Operation to perform.
 * @param p1 Bits 0..15: Vehicle id.
 *           Bits 16..23: Unit of timetable length
 * @param p2 Length of the new timetable length
 * @param text Not used.
 * @return The error or cost of the operation.
 */
CommandCost CmdSetTimetableLength(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	VehicleID vehicle_id = (VehicleID)(p1 & 0x0000FFFF);
	Vehicle *v = Vehicle::GetIfValid(vehicle_id);
	if (v == NULL || !v->IsPrimaryVehicle()) return CMD_ERROR;

	DurationUnit duration_unit = (DurationUnit)((p1 >> 16) & 0x000000FF);
	if (duration_unit != DU_DAYS && duration_unit != DU_MONTHS && duration_unit != DU_YEARS && duration_unit != DU_INVALID) {
		return CMD_ERROR;
	}

	int32 length = (int32)p2;
	if (length <= 0) {
		return CMD_ERROR;
	}

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	if (v->orders.list == NULL && !OrderList::CanAllocateItem()) return CMD_ERROR;

	if (flags & DC_EXEC) {
		/* If there is not yet an orderlist, create it */
		if (v->orders.list == NULL) {
			v->orders.list = new OrderList(NULL, v);
		}

		OrderList* order_list = v->orders.list;
		Duration new_length = Duration(length, duration_unit);
		order_list->SetTimetableDuration(new_length);

		SetWindowClassesDirty(WC_VEHICLE_TIMETABLE);
	}

	return CommandCost();
}

/** Updates the (cached) start times of all vehicles that share the timetable of the given vehicle, including the vehicle itself.
 *  Has to be called after the start time of the timetable changes.
 */
void UpdateSharedVehiclesTimetableData(OrderList *order_list)
{
	if (order_list->HasStartTime()) {
		Date timetable_start_date = order_list->GetStartTime();
		Duration timetable_length = order_list->GetTimetableDuration();
		for (Vehicle *u = order_list->GetFirstSharedVehicle(); u != NULL; u = u->NextShared()) {
			u->timetable_start = AddToDate(timetable_start_date, u->timetable_offset);
			u->timetable_end = AddToDate(u->timetable_start, timetable_length);
		}
	}
}

void UpdateVehicleStartTimes(Vehicle *vehicle)
{
	Duration timetable_length = vehicle->orders.list->GetTimetableDuration();

	/* Update the local timetable start/end time of the vehicle */
	Date global_timetable_start_date = vehicle->orders.list->GetStartTime();
	vehicle->timetable_start = AddToDate(global_timetable_start_date, vehicle->timetable_offset);
	vehicle->timetable_end = AddToDate(vehicle->timetable_start, timetable_length);
}

static bool IsUniqueTimetableName(const char *name)
{
	for (const OrderList *order_list : OrderList::Iterate()) {
		if (order_list->GetName() != NULL && strcmp(order_list->GetName(), name) == 0) return false;
	}

	return true;
}

/**
 * Rename a timetable.
 * @param tile unused
 * @param flags type of operation
 * @param p1   unused
 * @param p2   unused
 * @param text new name of the timetable
 * @return the cost of this operation or an error
 */
CommandCost CmdRenameTimetable(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Vehicle *vehicle = Vehicle::GetIfValid(p1);

	if (vehicle == NULL) {
		return CMD_ERROR;
	}

	if (vehicle->orders.list == NULL && !OrderList::CanAllocateItem()) {
		return CMD_ERROR;
	}

	bool reset = StrEmpty(text);

	if (!reset) {
		if (Utf8StringLength(text) >= MAX_LENGTH_TIMETABLE_NAME_CHARS) {
			return CMD_ERROR;
		}
		if (!IsUniqueTimetableName(text)) return_cmd_error(STR_ERROR_NAME_MUST_BE_UNIQUE);
	}

	if (flags & DC_EXEC) {
		if (vehicle->orders.list == NULL) {
			vehicle->orders.list = new OrderList(NULL, vehicle);
		}

		/* Delete the old name */
		free(vehicle->orders.list->GetName());
		/* Assign the new one */
		vehicle->orders.list->SetName(reset ? NULL : stredup(text));
	}

	return CommandCost();
}

/**
 * Set the departure time for an order.  Only departure time within the range
 * timetable_start (inclusive) .. timetable_start plus length of the timetable
 * (exclusive) are allowed.
 * @param tile Not used.
 * @param flags Operation to perform.
 * @param p1 Bits 0..15: Order ID
 *           Bits 16..23: ID of the Vehicle the order is for
 * @param p2 New departure date, may be the invalid date.
 * @param text Not used.
 * @return The error or cost of the operation.
 */
CommandCost CmdSetOrderDeparture(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	OrderID order_id = (p1 & 0x0000FFFF);
	VehicleID vehicle_id = (p1 >> 16) & 0x0000FFFF;

	Order* order = Order::GetIfValid(order_id);
	Vehicle *vehicle = Vehicle::GetIfValid(vehicle_id);

	if (order == NULL || vehicle == NULL) {
		return CMD_ERROR;
	}

	Date timetable_start = vehicle->orders.list->GetStartTime();
	if (timetable_start == INVALID_DATE) {
		return_cmd_error(STR_ERROR_TIMETABLE_NO_TIMETABLE_START_GIVEN);
	}

	Duration timetable_length = vehicle->orders.list->GetTimetableDuration();
	if (timetable_length.IsInvalid()) {
		return_cmd_error(STR_ERROR_TIMETABLE_NO_TIMETABLE_LENGTH_GIVEN);
	}

	Date timetable_end = AddToDate(timetable_start, timetable_length);

	Date new_departure_date = (Date)p2;
	if (new_departure_date != INVALID_DATE && (new_departure_date < timetable_start || new_departure_date >= timetable_end)) {
		return_cmd_error(STR_ERROR_TIMETABLE_DATE_NOT_IN_TIMETABLE);
	}

	if (flags & DC_EXEC) {
		order->SetDeparture(new_departure_date);

		SetWindowDirty(WC_VEHICLE_TIMETABLE, vehicle->index);
	}

	return CommandCost();
}

/**
 * Set the speed limit of an order.
 * @param tile Not used.
 * @param flags Operation to perform.
 * @param p1 Bits 0..15: Order ID
 *           Bits 16..31: ID of the Vehicle the order is for
 * @param p2 Bits 0..15: New speed limit
 * @param text Not used.
 * @return The error or cost of the operation.
 */
CommandCost CmdSetOrderSpeedLimit(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	OrderID order_id = (p1 & 0x0000FFFF);
	VehicleID vehicle_id = (p1 >> 16) & 0x0000FFFF;

	Order* order = Order::GetIfValid(order_id);
	Vehicle *vehicle = Vehicle::GetIfValid(vehicle_id);
	if (order == NULL || vehicle == NULL) {
		return CMD_ERROR;
	}

	uint16 speed_limit = (p2 & 0x0000FFFF);

	if (flags & DC_EXEC) {
		order->SetMaxSpeed(speed_limit);

		SetWindowDirty(WC_VEHICLE_TIMETABLE, vehicle->index);
	}

	return CommandCost();
}

/**
 * Set the arrival date for an order.  Only arrival dates within the range
 * start date of the vehicles timetable (inclusive) .. start date plus vehicle offset
 * (exclusive) are allowed.
 * @param tile Not used.
 * @param flags Operation to perform.
 * @param p1 Bits 0..15: Order ID
 *           Bits 16..31: ID of the Vehicle the order is for
 * @param p2 New arrival date, may be the invalid date
 * @param text Not used.
 * @return The error or cost of the operation.
 */
CommandCost CmdSetOrderArrival(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	OrderID order_id = (p1 & 0x0000FFFF);
	VehicleID vehicle_id = (p1 >> 16) & 0x0000FFFF;

	Order* order = Order::GetIfValid(order_id);
	Vehicle *vehicle = Vehicle::GetIfValid(vehicle_id);

	if (order == NULL || vehicle == NULL) {
		return CMD_ERROR;
	}

	Date timetable_start = vehicle->orders.list->GetStartTime();
	if (timetable_start == INVALID_DATE) {
		return_cmd_error(STR_ERROR_TIMETABLE_NO_TIMETABLE_START_GIVEN);
	}

	Duration timetable_length = vehicle->orders.list->GetTimetableDuration();
	if (timetable_length.IsInvalid()) {
		return_cmd_error(STR_ERROR_TIMETABLE_NO_TIMETABLE_LENGTH_GIVEN);
	}

	Date timetable_end = AddToDate(timetable_start, timetable_length);

	Date new_arrival_date = (Date)p2;
	if (new_arrival_date != INVALID_DATE && (new_arrival_date < timetable_start || new_arrival_date >= timetable_end)) {
		return_cmd_error(STR_ERROR_TIMETABLE_DATE_NOT_IN_TIMETABLE);
	}

	if (flags & DC_EXEC) {
		order->SetArrival(new_arrival_date);

		SetWindowDirty(WC_VEHICLE_TIMETABLE, vehicle->index);
	}

	return CommandCost();
}

/**
 * Start or stop filling the timetable automatically from the time the vehicle
 * actually takes to complete it. When starting to autofill the current times
 * are cleared and the timetable will start again from scratch.
 * @param tile Not used.
 * @param flags Operation to perform.
 * @param p1 Vehicle index.
 * @param p2 Various bitstuffed elements
 * - p2 = (bit 0) - Set to 1 to enable, 0 to disable autofill.
 * - p2 = (bit 1) - Set to 1 to preserve waiting times in non-destructive mode
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdAutofillTimetable(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	VehicleID veh = GB(p1, 0, 20);

	Vehicle *v = Vehicle::GetIfValid(veh);
	if (v == nullptr || !v->IsPrimaryVehicle() || v->orders.list == nullptr) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	if (flags & DC_EXEC) {
		if (HasBit(p2, 0)) {
			/* Start autofilling the timetable, which clears the
			 * "timetable has started" bit. Times are not cleared anymore, but are
			 * overwritten when the order is reached now. */
			SetBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE);
			ClrBit(v->vehicle_flags, VF_TIMETABLE_STARTED);

			/* Overwrite waiting times only if they got longer */
			if (HasBit(p2, 1)) SetBit(v->vehicle_flags, VF_AUTOFILL_PRES_WAIT_TIME);

			v->timetable_start = 0;
			v->lateness_counter = 0;
		} else {
			ClrBit(v->vehicle_flags, VF_AUTOFILL_TIMETABLE);
			ClrBit(v->vehicle_flags, VF_AUTOFILL_PRES_WAIT_TIME);
		}

		for (Vehicle *v2 = v->FirstShared(); v2 != nullptr; v2 = v2->NextShared()) {
			if (v2 != v) {
				/* Stop autofilling; only one vehicle at a time can perform autofill */
				ClrBit(v2->vehicle_flags, VF_AUTOFILL_TIMETABLE);
				ClrBit(v2->vehicle_flags, VF_AUTOFILL_PRES_WAIT_TIME);
			}
			SetWindowDirty(WC_VEHICLE_TIMETABLE, v2->index);
		}
	}

	return CommandCost();
}

bool IsOrderTimetableValid(const Vehicle *vehicle, const Order *order)
{
	Date timetable_start = vehicle->orders.list->GetStartTime();
	Date timetable_end = AddToDate(timetable_start, vehicle->orders.list->GetTimetableDuration());

	if (order->HasArrival()) {
		Date arrival = order->GetArrival();
		if (arrival < timetable_start || arrival >= timetable_end) {
			return false;
		}
	}
	if (order->HasDeparture()) {
		Date departure = order->GetDeparture();
		if (departure < timetable_start || departure >= timetable_end) {
			return false;
		} else if (order->next != NULL && order->next->HasArrival() && departure > order->next->GetArrival()) {
			return false;
		}
	}

 	if (order->HasArrival() && order->HasDeparture() && order->GetArrival() > order->GetDeparture()) {
 		return false;
 	} else {
 		return true;
 	}
}
