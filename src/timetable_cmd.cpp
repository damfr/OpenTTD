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
