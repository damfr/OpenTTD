/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timetable_gui.cpp GUI for time tabling. */

#include "stdafx.h"
#include "command_func.h"
#include "engine_base.h"
#include "gui.h"
#include "window_gui.h"
#include "window_func.h"
#include "textbuf_gui.h"
#include "strings_func.h"
#include "vehicle_base.h"
#include "string_func.h"
#include "gfx_func.h"
#include "company_func.h"
#include "core/geometry_func.hpp"
#include "date_func.h"
#include "date_gui.h"
#include "duration_gui.h"
#include "vehicle_gui.h"
#include "network/network.h"
#include "settings_type.h"
#include "timetable.h"
#include "tilehighlight_func.h"
#include "tilehighlight_type.h"
#include "viewport_func.h"

#include "widget_type.h"
#include "widgets/dropdown_func.h"
#include "widgets/dropdown_type.h"
#include "widgets/timetable_widget.h"

#include "table/sprites.h"
#include "table/strings.h"

#include "order_func.h"
#include "order_gui.h"

#include "safeguards.h"

extern uint ConvertSpeedToDisplaySpeed(uint speed);
extern uint ConvertDisplaySpeedToSpeed(uint speed);

const int _timetable_setdate_offsets[] = {
	-10, -5, -1, 1, 5, 10
};

const StringID _timetable_setdate_strings[] = {
	STR_TIMETABLE_DATE_MINUS_THREE,
	STR_TIMETABLE_DATE_MINUS_TWO,
	STR_TIMETABLE_DATE_MINUS_ONE,
	STR_TIMETABLE_DATE_PLUS_ONE,
	STR_TIMETABLE_DATE_PLUS_TWO,
	STR_TIMETABLE_DATE_PLUS_THREE
};

enum TimetableFilterMode {
	TFM_SHOW_ALL = 0,
	TFM_SHOW_DESTINATION_LINES = 1,
	TFM_SHOW_TIMETABLE_LINES = 2,
};

/**
 * Callback for when a time has been chosen to start the time table
 * @param window the window related to the setting of the date
 * @param date the actually chosen date
 */
static void ChangeTimetableStartCallback(const Window *w, Date date)
{
	VehicleID vehicle_id = w->window_number;
	DoCommandP(0, vehicle_id, date, CMD_SET_TIMETABLE_START | CMD_MSG(STR_ERROR_CAN_T_TIMETABLE_VEHICLE));
}

/** Callback for when an offset of the vehicle has been chosen.
 */
static void SetOffsetCallback(const Window *w, Duration duration)
{
	uint32 p1 = (VehicleID)w->window_number | duration.GetUnit() << 16;
	uint32 p2 = duration.GetLength();
	DoCommandP(0, p1, p2, CMD_SET_TIMETABLE_OFFSET | CMD_MSG(STR_ERROR_CAN_T_TIMETABLE_VEHICLE));
}

/** Callback for when the length of the timetable has been chosen.
 */
static void SetLengthCallback(const Window *w, Duration duration)
{
	uint32 p1 = (VehicleID)w->window_number | duration.GetUnit() << 16;
	uint32 p2 = duration.GetLength();
	DoCommandP(0, p1, p2, CMD_SET_TIMETABLE_LENGTH | CMD_MSG(STR_ERROR_CAN_T_TIMETABLE_VEHICLE));
}

/** Constructs the caption to be used for the datefast_gui, when choosing an arrival for the given order,
 *  and writes it into the given buffer.
 */
static void GetArrivalQueryCaption(const Vehicle *vehicle, char *buffer, const char* last, const Order *order)
{
	if (order->IsWaypointOrder()) {
		GetString(buffer, STR_TIMETABLE_SET_ARRIVAL_WAYPOINT_CAPTION, last);
	} else if (order->IsDepotOrder()) {
		SetDParam(0, vehicle->type);
		SetDParam(1, order->GetDestination());
		GetString(buffer, STR_TIMETABLE_SET_ARRIVAL_DEPOT_CAPTION, last);
	} else {
		SetDParam(0, order->GetDestination());
		GetString(buffer, STR_TIMETABLE_SET_ARRIVAL_STATION_CAPTION, last);
	}
}

/** Constructs the caption to be used for the datefast_gui, when choosing a departure for the given order,
 *  and writes it into the given buffer.
 */
static void GetDepartureQueryCaption(const Vehicle *vehicle, char *buffer, const char* last, const Order *order)
{
	if (order->IsWaypointOrder()) {
		SetDParam(0, order->GetDestination());
		GetString(buffer, STR_TIMETABLE_SET_DEPARTURE_WAYPOINT_CAPTION, last);
	} else if (order->IsDepotOrder()) {
		SetDParam(0, vehicle->type);
		SetDParam(1, order->GetDestination());
		GetString(buffer, STR_TIMETABLE_SET_DEPARTURE_DEPOT_CAPTION, last);
	} else {
		SetDParam(0, order->GetDestination());
		GetString(buffer, STR_TIMETABLE_SET_DEPARTURE_STATION_CAPTION, last);
	}
}

struct TimetableWindow : Window {

private:
	/** The currently selected line (not order index!) in the order/timetable panel.  E.g. if the first order occupies two lines
      * (one destination, one timetable), then its timetable line corresponds to selected_timetable_line == 1 */
	int selected_timetable_line;

	/** Indicates wether the property line (start, length and name of timetable, offset of vehicle) is currently selected. */
	bool property_line_selected;

	/** Indicates wether the status line (name, autofill) is currently selected. */
	bool vehicle_interval_line_selected;

	/** Constant for marking the fact that currently, no timetable line is selected (e.g. since one of the meta data lines on top are selected) */
	static const int INVALID_SELECTION = -1;

	/** Indicates, which filter mode is currently active.  Options are show destination information plus timetable information,
	 *  show only destination information (as in the traditional order view), or show only timetable information in a tabular manner. */
	TimetableFilterMode filter_mode;

	/** The default value assumed for staying in a station (when setting arrivals / departures using Choose & Next in the datefast_gui) */
	static const int DEFAULT_STATION_TIME = 5;

	/** The default value assumed for traveling between two stations (when setting arrivals / departures using Choose & Next in the datefast_gui */
	static const int DEFAULT_TRAVEL_TIME = 10;

	const Vehicle *vehicle; ///< Vehicle monitored by the window.

	Scrollbar *vscroll;

	VehicleOrderID order_over;         ///< Order over which another order is dragged, \c INVALID_VEH_ORDER_ID if none.

	bool can_do_autorefit; ///< Vehicle chain can be auto-refitted.

	enum TimetableQueryType {
		TQT_NAME,
		TQT_SPEED,
		TQT_COND
	};

	TimetableQueryType query_type;

  	/** Under what reason are we using the PlaceObject functionality? */
	enum TimetablePlaceObjectState {
		TIMETABLE_POS_GOTO,
		TIMETABLE_POS_CONDITIONAL,
		TIMETABLE_POS_SHARE,
	};

	enum DisplayPlane {
			// WID_VT_SELECTION_TOP
			DP_PROPERTY_LINE = 0,
			DP_VEHICLE_INTERVAL_LINE = 1,
			DP_DEST_COND_LINE = 2,
			DP_DEST_STATION_LINE = 3,
			DP_DEST_WAYPOINT_LINE = 4,
			DP_DEST_DEPOT_LINE = 5,
			DP_TIMETABLE_LINE = 6,
			DP_EMPTY_LINE = 7,

			// WID_VT_AUTOFILL_SELECTION
			DP_AUTOFILL_START_DROPDOWN = 0,
			DP_AUTOFILL_STOP_BUTTON = 1,

			// WID_VT_SELECTION_BOTTOM_2
	        DP_DELETE_ORDER_BUTTON = 0,
	        DP_STOP_SHARING_BUTTON = 1,
	};

	TimetablePlaceObjectState place_object_type;

	/**********************************************************************************************************************/
	/******************************* Management of order indices and line indices in the order list ***********************/
	/**********************************************************************************************************************/

	/** Given the y position where the user has clicked in the timetable panel, this function returns the corresponding
      * line number in terms of selected_timetable_line.
      * @param y coordinate the user has clicked to
      * @return corresponding line number as described
      */
	int GetLineFromPt(int y) const
	{
		/* Selected line, but without considering a potential offset due to the scrollbar */
		int raw_selected_line = (y - this->GetWidget<NWidgetBase>(WID_VT_TIMETABLE_PANEL)->pos_y - WD_FRAMERECT_TOP) / FONT_HEIGHT_NORMAL;

		/* User clicked below end of list */
		if ((uint)raw_selected_line >= this->vscroll->GetCapacity()) {
			return INVALID_SELECTION;
		}

		/* Consider scroll bar offset */
	    int selected_line_before_list_bounds = raw_selected_line + this->vscroll->GetPosition();

		/* Only consider the destination/timetable lines corresponding to the orders, and the destination line; not potentially
		 * left free space below */
		if (selected_line_before_list_bounds < this->vehicle->GetNumOrders() * GetLineMultiplier() + 1 && selected_line_before_list_bounds >= 0) {
			return selected_line_before_list_bounds;
		} else {
			return INVALID_SELECTION;
		}
	}

	/** Set the selected timetable line.
	 *  @param line the new selected_timetable_line */
	void UpdateSelectedTimetableLine(int line)
	{
		this->selected_timetable_line = line;
		if (this->selected_timetable_line != INVALID_SELECTION) {
			this->property_line_selected = false;
			this->vehicle_interval_line_selected = false;
		}
	}

	/** Returns wether any line in the whole timetable window is selected.  This can be (1) the property line in the top panel,
	 *  (2) the name / (autofill) status line also in the top panel, (3) any destination / timetable line in the timetable panel, or
	 *  (4) the end of orders line also in the timetable panel.
	 */
	bool IsAnyLineSelected() const
	{
		return IsContentLine(this->selected_timetable_line);
	}

	/** Returns wether the given line contains any kind of content, i.e. is one of the lines enumerated in the comment of IsAnyLineSelected. */
	bool IsContentLine(int line) const
	{
		return line != INVALID_SELECTION;
	}

	/** Makes the property line at the top the selected line. */
	void SelectPropertyLine()
	{
		this->property_line_selected = true;
		this->vehicle_interval_line_selected = false;
		this->selected_timetable_line = INVALID_SELECTION;
	}

	/** Returns wether the property line at the top is currently selected. */
	bool IsPropertyLineSelected() const
	{
		return this->property_line_selected;
	}

	/** Makes the name / autofill status line the selected line. */
	void SelectVehicleIntervalLine()
	{
		this->property_line_selected = false;
		this->vehicle_interval_line_selected = true;
		this->selected_timetable_line = INVALID_SELECTION;
	}

	/** Returns wether the name / autofill line is the currently selected line. */
	bool IsVehicleIntervalLineSelected() const
	{
		return this->vehicle_interval_line_selected;
	}

	/** Returns wether any destination line is selected, i.e. the first of the (potentially two) lines of an order. */
	bool IsDestinationLineSelected() const
	{
		return IsDestinationLine(this->selected_timetable_line);
	}

	/** Returns wether the given line is a destination line. */
	bool IsDestinationLine(int line) const
	{
		return IsContentLine(line) && vehicle->GetNumOrders() != INVALID_VEH_ORDER_ID && line < vehicle->GetNumOrders() * GetLineMultiplier() && (IsInShowDestinationsMode() || (IsInShowAllMode() && (line % 2 == 0)));
	}

	/** Returns wether any timetable line (i.e. a line containing Arrival, Departure, Speed Limit) is currently selected. */
	bool IsTimetableLineSelected() const
	{
		return IsTimetableLine(this->selected_timetable_line);
	}

	/** Returns wether the given line is a timetable line. */
	bool IsTimetableLine(int line) const
	{
		return IsContentLine(line) && vehicle->GetNumOrders() != INVALID_VEH_ORDER_ID && line < vehicle->GetNumOrders() * 2 && (IsInShowTimetableMode() || (IsInShowAllMode() &&  (line % 2 == 1)));
	}

	/** Returns wether the end of orders line is selected. */
	bool IsEndOfOrdersLineSelected() const
	{
		return IsAnyLineSelected() && this->selected_timetable_line == GetEndOfOrdersIndex();
	}

	/** Returns the index of the "End of orders" line. */
	int GetEndOfOrdersIndex() const
	{
		return vehicle->GetNumOrders() * GetLineMultiplier();
	}

	/** Returns the VehicleOrderID corresponding to the currently selected line, or INVALID_VEH_ORDER_ID if that line doesn´t correspond to an Order. */
	VehicleOrderID GetSelectedOrderID() const
	{
		return GetOrderID(this->selected_timetable_line);
	}

	/** Returns the VehicleOrderId corresponding to the given line, or INVALID_VEH_ORDER_ID if that line doesn´t correspond to an Order. */
	VehicleOrderID GetOrderID(int line) const
	{
		return (IsDestinationLine(line) || IsTimetableLine(line)) ? line / GetLineMultiplier() : INVALID_VEH_ORDER_ID;
 	}

	void AdjustSelectionByNumberOfOrders(int n)
	{
		assert (selected_timetable_line != INVALID_SELECTION);
		selected_timetable_line += n * GetLineMultiplier();
	}

	/** Returns the next destination line relative to the currently selected line, or INVALID_SELECTION if no such line exists. */
	int GetNextDestinationLine() const
	{
		assert (IsDestinationLineSelected() || IsTimetableLineSelected());
		if (IsDestinationLineSelected()) {
			return selected_timetable_line + (IsInShowAllMode() ? 2 : 1);
		} else if (IsTimetableLineSelected()) {
			return selected_timetable_line + 1;
		} else {
			return INVALID_SELECTION;
		}
	}

	/** Returns the number of lines a order currently occupies in the list. */
	int GetLineMultiplier() const { return (this->IsInShowAllMode() ? 2 : 1); }

	/**********************************************************************************************************************/
	/********************************* Keeping track about which information is shown *************************************/
	/**********************************************************************************************************************/

	bool IsInShowAllMode() const { return this->filter_mode == TFM_SHOW_ALL; }

	bool IsInShowDestinationsMode() const { return this->filter_mode == TFM_SHOW_DESTINATION_LINES; }

	bool IsInShowTimetableMode() const { return this->filter_mode == TFM_SHOW_TIMETABLE_LINES; }

	/** This method adjusts the selected line, if the player changes the filter setting.
     *  E.g. if previously all lines were shown, and now only the timetable lines are
	 *  to be shown, one has to divide the selected line index by two (if a line was selected).
	 */
	void AdjustShowModeAfterFilterChange(TimetableFilterMode old_mode, TimetableFilterMode new_mode)
	{
		if (!IsEndOfOrdersLineSelected()) {
			bool line_selected = IsTimetableLineSelected() || IsDestinationLineSelected();
			if (old_mode == TFM_SHOW_ALL) {
				if (new_mode != TFM_SHOW_ALL) {
					this->selected_timetable_line = (line_selected ? this->selected_timetable_line / 2 : INVALID_SELECTION);
				}
			} else if (old_mode == TFM_SHOW_DESTINATION_LINES) {
				if (new_mode == TFM_SHOW_ALL) {
					this->selected_timetable_line = (line_selected ? this->selected_timetable_line * 2 : INVALID_SELECTION);
				}
			} else if (old_mode == TFM_SHOW_TIMETABLE_LINES) {
				if (new_mode == TFM_SHOW_ALL) {
					this->selected_timetable_line = (line_selected ? this->selected_timetable_line * 2 + 1 : INVALID_SELECTION);
				}
			}
		} else {
			if (new_mode == TFM_SHOW_ALL) {
				this->selected_timetable_line = vehicle->GetNumOrders() * 2;
			} else {
				this->selected_timetable_line = vehicle->GetNumOrders();
			}
		}
	}

	/** This function calculates the space needed for the largest of the given dropdown items,
     *  and enlarges the given size to that space if necessary.
     *  @param dropdown_items pointer to an array of dropdown items, MUST be finished by INVALID_STRING_ID
     *  @param size initial size
     *  @param padding padding
     */
	void EnlargeSizeForDropdownIfNeeded(const StringID* dropdown_items, Dimension *size, const Dimension &padding)
	{
		Dimension d = {0, 0};
		for (int i = 0; dropdown_items[i] != INVALID_STRING_ID; i++) {
			d = maxdim(d, GetStringBoundingBox(dropdown_items[i]));
		}
		d.width += padding.width;
		d.height += padding.height;
		*size = maxdim(*size, d);
	}

	/**********************************************************************************************************************/
	/********************************************* Assembling output strings **********************************************/
	/**********************************************************************************************************************/

	/** Prepares for output or measurement of the string to be printed for the start/offset/length line.
	 *  (code is extracted to this method, since it is exactly the same for both DrawString and GetStringBoundingBox)
	 */
	void PrepareForPropertyLine() const
	{
		const OrderList *order_list = this->vehicle->orders.list;
		Date start_date = (order_list == NULL ? INVALID_DATE : order_list->GetStartTime());
		Duration timetable_length = (order_list == NULL ? Duration(0, DU_INVALID) : order_list->GetTimetableDuration());

		SetDParam(0, start_date);
		SetDParam(1, this->vehicle->timetable_offset.GetLength());
		SetDParam(2, this->vehicle->timetable_offset.GetUnit());
		SetDParam(3, timetable_length.GetLength());
		SetDParam(4, timetable_length.GetUnit());
	}

	/** Prepares for output or measurement of the string to be printed in the Vehicle timetable line.
     *  Does not deal with potential autofill information towards the end of that line.
     */
	void PrepareForVehicleIntervalLine() const
	{
		const OrderList *order_list = this->vehicle->orders.list;
		Duration length = (order_list == NULL ? Duration(0, DU_INVALID) : order_list->GetTimetableDuration()); // Timetable length, e.g. 2 months
		Date vehicle_timetable_start = this->vehicle->timetable_start;          // Start of vehicle timetable, e.g. 1st Feb 1905
		Date next_iteration_start = AddToDate(vehicle_timetable_start, length); // Start of next iteration of vehicle timetable, e.g. 1st April 1905
		Date vehicle_timetable_end = SubtractFromDate(next_iteration_start, Duration(1, DU_DAYS)); // Last date of vehicle timetable, e.g. 31st March 1905

		SetDParam(0, vehicle_timetable_start);
		SetDParam(1, vehicle_timetable_end);
	}

	/** Calculates and returns the string assembled for a Timetable line corresponding to some Order.
	 *  @param buffer buffer to write to
	 *  @param last end of buffer
	 *  @param order the order
	 *  @param order_id the VehicleOrderID of the order in the timetable of the vehicle.
     *  @return pointer to the given buffer
	 */
	const char* GetTimetableLineString(char* buffer, const char* last, const Order *order, VehicleOrderID order_id) const
	{
		/* The position where the next token will be written to */
		char* curr_buffer_pointer = buffer;

		/* The temporary buffer one token will be written to, before strings are concatenated */
		char tmp_buffer[200];

		uint16 max_speed = order->GetMaxSpeed();

		/* Three possible clauses: Arrival, Departure, Max Speed.  Depending on which of them exist, the assembled string will have a different structure */
		bool first_clause_exists = order->HasArrival();
		bool second_clause_exists = order->HasDeparture();
		bool third_clause_exists = max_speed != UINT16_MAX;

		Duration offset = this->vehicle->GetTimetableOffset();

		if (first_clause_exists) {
			/* Arrival at the Order, in terms of this vehicle. */
			Date order_arrival = AddToDate(order->GetArrival(), offset);

			SetDParam(0, order_arrival);
			GetString(tmp_buffer, STR_TIMETABLE_ARRIVAL, lastof(tmp_buffer));
			curr_buffer_pointer = strecat(curr_buffer_pointer, tmp_buffer, last);
		}

		if (first_clause_exists && second_clause_exists && third_clause_exists) {
			/* After the first, not only a second, but also a third clause exists.   Add a comma (or something similar. */
			GetString(tmp_buffer, STR_TIMETABLE_NOT_LAST_SEPARATOR, lastof(tmp_buffer));
			curr_buffer_pointer = strecat(curr_buffer_pointer, tmp_buffer, last);
		} else if (first_clause_exists && second_clause_exists && !third_clause_exists) {
			/* After the first, only a second clause exists.  Add a final "and". */
			GetString(tmp_buffer, STR_TIMETABLE_LAST_SEPARATOR, lastof(tmp_buffer));
			curr_buffer_pointer = strecat(curr_buffer_pointer, tmp_buffer, last);
		}

		if (second_clause_exists) {
			/* Departure at the Order, in terms of this vehicle */
			Date order_departure = AddToDate(order->GetDeparture(), offset);

			SetDParam(0, order_departure);
			GetString(tmp_buffer, STR_TIMETABLE_DEPARTURE, lastof(tmp_buffer));
			curr_buffer_pointer = strecat(curr_buffer_pointer, tmp_buffer, last);
		}

		if ((first_clause_exists || second_clause_exists) && third_clause_exists) {
			/* Add a final "and" before the third and last clause */
			GetString(tmp_buffer, STR_TIMETABLE_LAST_SEPARATOR, lastof(tmp_buffer));
			curr_buffer_pointer = strecat(curr_buffer_pointer, tmp_buffer, last);
		}

		if (max_speed != UINT16_MAX) {
			SetDParam(2, order->GetMaxSpeed());
			GetString(tmp_buffer, (first_clause_exists || second_clause_exists ? STR_TIMETABLE_SPEEDLIMIT : STR_TIMETABLE_TRAVEL_NOT_TIMETABLED_SPEED), lastof(tmp_buffer));
			curr_buffer_pointer = strecat(curr_buffer_pointer, tmp_buffer, last);
		}

		if (!first_clause_exists && !second_clause_exists && !third_clause_exists) {
			/* Default string if none of the three clauses exist. */
			GetString(tmp_buffer, STR_TIMETABLE_TRAVEL_NOT_TIMETABLED, lastof(tmp_buffer));
			curr_buffer_pointer = strecat(curr_buffer_pointer, tmp_buffer, last);
		}

		return buffer;
	}

	/** This function calculates the width needed for the delay information painted into the timetable window.
     */
	int GetDelayInfoWidth() const
	{
		Dimension d_upper;
		Dimension d_lower;
		int32 lateness_counter = this->vehicle->lateness_counter;
		if (lateness_counter < 0) {
			SetDParam(0, (lateness_counter < 0 ? -lateness_counter : lateness_counter));
			d_upper = GetStringBoundingBox(STR_TIMETABLE_DAYS);
			d_lower = GetStringBoundingBox(STR_TIMETABLE_TOO_EARLY);
		} else if (lateness_counter == 0) {
			d_upper = GetStringBoundingBox(STR_TIMETABLE_ON_TIME_UPPER);
			d_lower = GetStringBoundingBox(STR_TIMETABLE_ON_TIME_LOWER);
		} else {
			SetDParam(0, (lateness_counter < 0 ? -lateness_counter : lateness_counter));
			d_upper = GetStringBoundingBox(STR_TIMETABLE_DAYS);
			d_lower = GetStringBoundingBox(STR_TIMETABLE_DELAY);
		}
		return max(d_upper.width, d_lower.width);
	}

	/**********************************************************************************************************************/
	/****************************************** Keeping track of widget state *********************************************/
	/**********************************************************************************************************************/

	/** Sets the displayed plane of the corresponding widget.
	 */
	void SetDisplayedPlane(uint widget_id, DisplayPlane plane)
	{
		NWidgetStacked *widget = this->GetWidget<NWidgetStacked>(widget_id);
		widget->SetDisplayedPlane(plane);
	}

	/** Sets the displayed plane of the corresponding widget, and at the same time updates the enabled state of the corresponding button, dropdown, etc.
	 *  Useful, to keep the code in UpdateButtonState below short.
	 */
	void SetDisplayedPlane(uint selection_widget_id, uint widget_id, DisplayPlane plane, bool enabled)
	{
		NWidgetStacked *selection_widget = this->GetWidget<NWidgetStacked>(selection_widget_id);
		selection_widget->SetDisplayedPlane(plane);
		this->SetWidgetDisabledState(widget_id, !enabled);
	}

	/** This function sets visibility and activation state of buttons, dropdowns, etc. according to the current widget state.
	 *  E.g. if the very first line at the top is selected, it makes the buttons for setting timetable start, offset, length
	 *  and name visible, and other buttons that are displayed at the same locations under other circumstances invisible.
	 *  Thus, this function needs to be called whenever any property / condition it evaluates changes, typically after mouse clicks etc.
	 */
	void UpdateButtonState()
	{
		if (this->vehicle->owner != _local_company) return; // No buttons are displayed with competitor order windows.

	    bool anything_enabled = true;
		bool property_line = IsPropertyLineSelected();
		bool vehicle_interval_line = IsVehicleIntervalLineSelected();
		bool dest_line = IsDestinationLineSelected();
		bool time_line = IsTimetableLineSelected();
		bool end_line = IsEndOfOrdersLineSelected();

		const Order *order = (dest_line || time_line ? this->vehicle->GetOrder(GetSelectedOrderID()) : NULL);
		bool station_order = order != NULL && order->GetType() == OT_GOTO_STATION;
		bool waypoint_order = order != NULL && order->GetType() == OT_GOTO_WAYPOINT;
		bool depot_order = order != NULL && order->GetType() == OT_GOTO_DEPOT;
		bool cond_order = order != NULL && order->GetType() == OT_CONDITIONAL;
		bool shared_orders = this->vehicle->IsOrderListShared();

		bool train = (this->vehicle->type == VEH_TRAIN);
		bool road = (this->vehicle->type == VEH_ROAD);

		if (property_line) {
			SetDisplayedPlane(WID_VT_TOP_SELECTION, DP_PROPERTY_LINE);
		} else if (vehicle_interval_line) {
			SetDisplayedPlane(WID_VT_TOP_SELECTION, DP_VEHICLE_INTERVAL_LINE);
		} else if (dest_line) {
			if (cond_order) {
				SetDisplayedPlane(WID_VT_TOP_SELECTION, DP_DEST_COND_LINE);
			} else if (station_order) {
				SetDisplayedPlane(WID_VT_TOP_SELECTION, DP_DEST_STATION_LINE);
			} else if (waypoint_order) {
				SetDisplayedPlane(WID_VT_TOP_SELECTION, DP_DEST_WAYPOINT_LINE);
			} else if (depot_order) {
				SetDisplayedPlane(WID_VT_TOP_SELECTION, DP_DEST_DEPOT_LINE);
			} else {
			  SetDisplayedPlane(WID_VT_TOP_SELECTION, DP_EMPTY_LINE);
			}
		} else if (time_line) {
			if (cond_order) {
				SetDisplayedPlane(WID_VT_TOP_SELECTION, DP_EMPTY_LINE);
			} else {
				SetDisplayedPlane(WID_VT_TOP_SELECTION, DP_TIMETABLE_LINE);
			}
 		} else {
			SetDisplayedPlane(WID_VT_TOP_SELECTION, DP_EMPTY_LINE);
		}

		if (vehicle_interval_line) {
			if (HasBit(this->vehicle->vehicle_flags, VF_AUTOFILL_TIMETABLE)) {
				SetDisplayedPlane(WID_VT_AUTOFILL_SELECTION, WID_VT_STOP_AUTOFILL_BUTTON, DP_AUTOFILL_STOP_BUTTON, anything_enabled);
			} else {
				SetDisplayedPlane(WID_VT_AUTOFILL_SELECTION, WID_VT_START_AUTOFILL_DROPDOWN, DP_AUTOFILL_START_DROPDOWN, anything_enabled);
			}
		}

		if (dest_line && cond_order) {
			OrderConditionVariable ocv = order->GetConditionVariable();
			this->GetWidget<NWidgetCore>(WID_VT_COND_VARIABLE_DROPDOWN)->widget_data   = STR_ORDER_CONDITIONAL_LOAD_PERCENTAGE + (order == NULL ? 0 : ocv);
			this->GetWidget<NWidgetCore>(WID_VT_COND_COMPARATOR_DROPDOWN)->widget_data = _order_conditional_condition[order == NULL ? 0 : order->GetConditionComparator()];

			this->SetWidgetDisabledState(WID_VT_COND_COMPARATOR_DROPDOWN, ocv == OCV_UNCONDITIONALLY);
			this->SetWidgetDisabledState(WID_VT_COND_VALUE_BUTTON, ocv == OCV_REQUIRES_SERVICE || ocv == OCV_UNCONDITIONALLY);
		}

		if (dest_line && station_order) {
			bool can_load_unload = (order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION) == 0;
			this->SetWidgetDisabledState(WID_VT_NON_STOP_DROPDOWN, !train && !road);
			this->SetWidgetDisabledState(WID_VT_FULL_LOAD_DROPDOWN, !can_load_unload);
			this->SetWidgetDisabledState(WID_VT_UNLOAD_DROPDOWN, !can_load_unload);

			bool can_do_autorefit = this->can_do_autorefit && order->GetLoadType() != OLFB_NO_LOAD
				&& !(order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION);
			this->SetWidgetDisabledState(WID_VT_REFIT_AUTO_DROPDOWN, !can_do_autorefit);
		}

		if (dest_line && depot_order) {
		    bool may_refit = !((order->GetDepotOrderType() & ODTFB_SERVICE) || (order->GetDepotActionType() & ODATFB_HALT));
			this->SetWidgetDisabledState(WID_VT_REFIT_BUTTON, !may_refit);
		}

		if (time_line) {
			bool timetable_meta_data_entered = this->vehicle->orders.list->GetStartTime() != INVALID_DATE
											&& !this->vehicle->orders.list->GetTimetableDuration().IsInvalid()
											&& !this->vehicle->timetable_offset.IsInvalid();
			this->SetWidgetDisabledState(WID_VT_ARRIVAL_BUTTON, !timetable_meta_data_entered);
			this->SetWidgetDisabledState(WID_VT_DEPARTURE_BUTTON, !timetable_meta_data_entered);
		}


/*
		if (property_line) {
			SetDisplayedPlane(WID_VT_SELECTION_TOP_1, WID_VT_START_BUTTON, DP_START_BUTTON, true);
			SetDisplayedPlane(WID_VT_SELECTION_TOP_2, WID_VT_OFFSET_BUTTON, DP_OFFSET_BUTTON, true);
			SetDisplayedPlane(WID_VT_SELECTION_TOP_3, WID_VT_LENGTH_BUTTON, DP_LENGTH_BUTTON, true);
			SetDisplayedPlane(WID_VT_SELECTION_TOP_4, DP_SHIFT_BUTTONS);
			this->SetWidgetDisabledState(WID_VT_SHIFT_ORDERS_PAST_BUTTON, false);     ///< Doing this not using the above helper function, as here one plane controls two buttons
			this->SetWidgetDisabledState(WID_VT_SHIFT_ORDERS_FUTURE_BUTTON, false);
		} else if (vehicle_interval_line) {
			SetDisplayedPlane(WID_VT_SELECTION_TOP_1, WID_VT_RENAME_BUTTON, DP_RENAME_BUTTON, true);
			if (HasBit(this->vehicle->vehicle_flags, VF_AUTOFILL_TIMETABLE)) {
				SetDisplayedPlane(WID_VT_SELECTION_TOP_2, WID_VT_STOP_AUTOFILL_BUTTON, DP_STOP_AUTOFILL_BUTTON, anything_enabled);
			} else {
				SetDisplayedPlane(WID_VT_SELECTION_TOP_2, WID_VT_START_AUTOFILL_DROPDOWN, DP_START_AUTOFILL_DROPDOWN, anything_enabled);
			}
			SetDisplayedPlane(WID_VT_SELECTION_TOP_3, DP_NO_TOP3);
			SetDisplayedPlane(WID_VT_SELECTION_TOP_4, DP_NO_TOP4);
		} else if (dest_line) {
			bool canLoadUnload = (order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION) == 0;

			if (cond_order) {
				OrderConditionVariable ocv = order->GetConditionVariable();
				this->GetWidget<NWidgetCore>(WID_VT_COND_VARIABLE_DROPDOWN)->widget_data   = STR_ORDER_CONDITIONAL_LOAD_PERCENTAGE + (order == NULL ? 0 : ocv);
				this->GetWidget<NWidgetCore>(WID_VT_COND_COMPARATOR_DROPDOWN)->widget_data = _order_conditional_condition[order == NULL ? 0 : order->GetConditionComparator()];
			}

			if (cond_order) {
				SetDisplayedPlane(WID_VT_SELECTION_TOP_1, WID_VT_COND_VARIABLE_DROPDOWN, DP_COND_VARIABLE_DROPDOWN, true);
			} else {
				SetDisplayedPlane(WID_VT_SELECTION_TOP_1, WID_VT_NON_STOP_DROPDOWN, DP_NON_STOP_DROPDOWN, train || road);
			}

			if (station_order) {
				SetDisplayedPlane(WID_VT_SELECTION_TOP_2, WID_VT_FULL_LOAD_DROPDOWN, DP_FULL_LOAD_DROPDOWN, canLoadUnload);
			} else if (waypoint_order) {
				SetDisplayedPlane(WID_VT_SELECTION_TOP_2, WID_VT_FULL_LOAD_DROPDOWN, DP_FULL_LOAD_DROPDOWN, false);
			} else if (depot_order) {
			    bool mayRefit = !((order->GetDepotOrderType() & ODTFB_SERVICE) || (order->GetDepotActionType() & ODATFB_HALT));
				SetDisplayedPlane(WID_VT_SELECTION_TOP_2, WID_VT_REFIT_BUTTON, DP_REFIT_BUTTON_TOP2, mayRefit);
			} else if (cond_order) {
				OrderConditionVariable ocv = order->GetConditionVariable();
				SetDisplayedPlane(WID_VT_SELECTION_TOP_2, WID_VT_COND_COMPARATOR_DROPDOWN, DP_COND_COMPARATOR_DROPDOWN, ocv != OCV_UNCONDITIONALLY);
			} else {
				SetDisplayedPlane(WID_VT_SELECTION_TOP_2, DP_NO_TOP2);
			}

			if (station_order) {
				SetDisplayedPlane(WID_VT_SELECTION_TOP_3, WID_VT_UNLOAD_DROPDOWN, DP_UNLOAD_DROPDOWN, canLoadUnload);
			} else if (waypoint_order) {
				SetDisplayedPlane(WID_VT_SELECTION_TOP_3, WID_VT_UNLOAD_DROPDOWN, DP_UNLOAD_DROPDOWN, false);
			} else if (depot_order) {
				SetDisplayedPlane(WID_VT_SELECTION_TOP_3, WID_VT_SERVICE_DROPDOWN, DP_SERVICE_DROPDOWN, true);
			} else if (cond_order) {
				OrderConditionVariable ocv = order->GetConditionVariable();
				SetDisplayedPlane(WID_VT_SELECTION_TOP_3, WID_VT_COND_VALUE_BUTTON, DP_COND_VALUE_BUTTON, ocv != OCV_REQUIRES_SERVICE && ocv != OCV_UNCONDITIONALLY);
			} else {
				SetDisplayedPlane(WID_VT_SELECTION_TOP_3, DP_NO_TOP3);
			}

			if (station_order) {
				bool can_do_autorefit = this->can_do_autorefit && order->GetLoadType() != OLFB_NO_LOAD && !(order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION);
				SetDisplayedPlane(WID_VT_SELECTION_TOP_4, WID_VT_REFIT_AUTO_DROPDOWN, DP_REFIT_AUTO_DROPDOWN, can_do_autorefit);
			} else if (waypoint_order) {
				SetDisplayedPlane(WID_VT_SELECTION_TOP_4, WID_VT_REFIT_AUTO_DROPDOWN, DP_REFIT_AUTO_DROPDOWN, false);
			} else if (depot_order) {
				SetDisplayedPlane(WID_VT_SELECTION_TOP_4, DP_NO_TOP4);
			} else if (cond_order) {
				SetDisplayedPlane(WID_VT_SELECTION_TOP_4, DP_NO_TOP4);
			} else {
				SetDisplayedPlane(WID_VT_SELECTION_TOP_4, DP_NO_TOP4);
			}
		} else if (time_line) {
			bool timetable_meta_data_entered = this->vehicle->orders.list->GetStartTime() != INVALID_DATE
											&& !this->vehicle->orders.list->GetTimetableDuration().IsInvalid()
											&& !this->vehicle->timetable_offset.IsInvalid();
			SetDisplayedPlane(WID_VT_SELECTION_TOP_1, WID_VT_ARRIVAL_BUTTON, DP_ARRIVAL_BUTTON, timetable_meta_data_entered);
			SetDisplayedPlane(WID_VT_SELECTION_TOP_2, DP_NO_TOP2);
			SetDisplayedPlane(WID_VT_SELECTION_TOP_3, WID_VT_SPEEDLIMIT_BUTTON, DP_SPEEDLIMIT_BUTTON, true);
			SetDisplayedPlane(WID_VT_SELECTION_TOP_4, WID_VT_DEPARTURE_BUTTON, DP_DEPARTURE_BUTTON, timetable_meta_data_entered);
		} else if (end_line) {
			SetDisplayedPlane(WID_VT_SELECTION_TOP_1, WID_VT_NON_STOP_DROPDOWN, DP_NON_STOP_DROPDOWN, false);
			SetDisplayedPlane(WID_VT_SELECTION_TOP_2, WID_VT_FULL_LOAD_DROPDOWN, DP_FULL_LOAD_DROPDOWN, false);
			SetDisplayedPlane(WID_VT_SELECTION_TOP_3, WID_VT_UNLOAD_DROPDOWN, DP_UNLOAD_DROPDOWN, false);
			SetDisplayedPlane(WID_VT_SELECTION_TOP_4, WID_VT_REFIT_BUTTON_4, DP_REFIT_BUTTON_TOP4, false);
		} else {
			// Nothing selected
			SetDisplayedPlane(WID_VT_SELECTION_TOP_1, WID_VT_NON_STOP_DROPDOWN, DP_NON_STOP_DROPDOWN, false);
			SetDisplayedPlane(WID_VT_SELECTION_TOP_2, WID_VT_FULL_LOAD_DROPDOWN, DP_FULL_LOAD_DROPDOWN, false);
			SetDisplayedPlane(WID_VT_SELECTION_TOP_3, WID_VT_UNLOAD_DROPDOWN, DP_UNLOAD_DROPDOWN, false);
			SetDisplayedPlane(WID_VT_SELECTION_TOP_4, WID_VT_REFIT_BUTTON_4, DP_REFIT_BUTTON_TOP4, false);
		}
*/
		if (end_line) {
			SetDisplayedPlane(WID_VT_SELECTION_BOTTOM_2, WID_VT_STOP_SHARING_BUTTON, DP_STOP_SHARING_BUTTON, true);
		} else {
			SetDisplayedPlane(WID_VT_SELECTION_BOTTOM_2, WID_VT_DELETE_ORDER_BUTTON, DP_DELETE_ORDER_BUTTON, (dest_line || time_line || end_line) && vehicle->GetNumOrders() > 0);
		}

		this->SetWidgetDisabledState(WID_VT_STOP_SHARING_BUTTON, !shared_orders);
		this->SetWidgetDisabledState(WID_VT_SKIP_ORDER_BUTTON , vehicle->GetNumOrders() == 0);
	}

	/** Cache auto-refittability of the vehicle chain. */
	void UpdateAutoRefitState()
	{
		this->can_do_autorefit = false;
		for (const Vehicle *w = this->vehicle; w != NULL; w = w->Next()) {
			if (HasBit(Engine::Get(w->engine_type)->info.misc_flags, EF_AUTO_REFIT)) this->can_do_autorefit = true;
		}
	}

	/**********************************************************************************************************************/
	/************************************** Processing clicks to the various buttons etc. *********************************/
	/**********************************************************************************************************************/

	void ProcessMetaDataPanelClick(Point pt)
	{
		int clicked_line = (pt.y - this->GetWidget<NWidgetBase>(WID_VT_SUMMARY_PANEL)->pos_y - WD_FRAMERECT_TOP) / FONT_HEIGHT_NORMAL;

		if (clicked_line == 0) {
			SelectPropertyLine();
		} else if (clicked_line == 1) {
			SelectVehicleIntervalLine();
		}

		this->UpdateButtonState();
	}

	/** This function processes a click into the destination / timetable panel.
	 *  @param pt Point where the user clicked to
	 */
	void ProcessListClick(Point pt)
	{
		int clicked_line = GetLineFromPt(pt.y);

		if (this->place_object_type == TIMETABLE_POS_CONDITIONAL) {
			this->place_object_type = TIMETABLE_POS_GOTO;

			if (IsDestinationLine(clicked_line) || IsTimetableLine(clicked_line)) {
				VehicleOrderID clicked_order_id = GetOrderID(clicked_line);

				Order order;
				order.next = NULL;
				order.index = 0;
				order.MakeConditional(clicked_order_id);

				VehicleOrderID order_id = (IsDestinationLineSelected() || IsTimetableLineSelected() ? this->GetSelectedOrderID() : this->vehicle->GetNumOrders());
				DoCommandP(this->vehicle->tile, this->vehicle->index + (order_id << 20), order.Pack(), CMD_INSERT_ORDER | CMD_MSG(STR_ERROR_CAN_T_INSERT_NEW_ORDER));
			}
			ResetObjectToPlace();
			return;
		}


		if (_ctrl_pressed && (IsDestinationLine(clicked_line) || IsTimetableLine(clicked_line))) {
			TileIndex xy = this->vehicle->GetOrder(GetOrderID(clicked_line))->GetLocation(this->vehicle);
			if (xy != INVALID_TILE) ScrollMainWindowToTile(xy);
			return;
		}

		/* This order won't be selected any more, close all child windows and dropdowns */
		this->DeleteChildWindows();
		HideDropDownMenu(this);

		if (!IsContentLine(clicked_line) || this->vehicle->owner != _local_company) {
			/* Deselect clicked order */
			UpdateSelectedTimetableLine(INVALID_SELECTION);
		} else if (IsContentLine(clicked_line) && clicked_line == this->selected_timetable_line) {
			if (this->vehicle->type == VEH_TRAIN && IsDestinationLine(clicked_line)) {
				VehicleOrderID clicked_order_id = GetOrderID(clicked_line);
				DoCommandP(this->vehicle->tile, this->vehicle->index + (clicked_order_id << 20),
						MOF_STOP_LOCATION | ((this->vehicle->GetOrder(clicked_order_id)->GetStopLocation() + 1) % OSL_END) << 4,
						CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
			} else if (IsEndOfOrdersLineSelected()) {
				UpdateSelectedTimetableLine(INVALID_SELECTION);
			}
		} else {
			/* Select clicked order */
			UpdateSelectedTimetableLine(clicked_line);

			if (this->vehicle->owner == _local_company) {
				/* Activate drag and drop */
				SetObjectToPlaceWnd(SPR_CURSOR_MOUSE, PAL_NONE, HT_DRAG, this);
			}
		}

		this->UpdateButtonState();
	}

	/**
	 * Handle the click on the full load button.
	 * @param load_type the way to load.
	 */
	void ProcessFullLoadClick(int load_type)
	{
		VehicleOrderID sel_ord = this->GetSelectedOrderID();
		const Order *order = this->vehicle->GetOrder(sel_ord);

		if (order == NULL || order->GetLoadType() == load_type) return;

		if (load_type < 0) {
			load_type = order->GetLoadType() == OLF_LOAD_IF_POSSIBLE ? OLF_FULL_LOAD_ANY : OLF_LOAD_IF_POSSIBLE;
		}
		DoCommandP(this->vehicle->tile, this->vehicle->index + (sel_ord << 20), MOF_LOAD | (load_type << 4), CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
	}

	/**
	 * Handle the click on the unload button.
	 */
	void ProcessUnloadClick(int unload_type)
	{
		VehicleOrderID sel_ord = this->GetSelectedOrderID();
		const Order *order = this->vehicle->GetOrder(sel_ord);

		if (order == NULL || order->GetUnloadType() == unload_type) return;

		if (unload_type < 0) {
			unload_type = order->GetUnloadType() == OUF_UNLOAD_IF_POSSIBLE ? OUFB_UNLOAD : OUF_UNLOAD_IF_POSSIBLE;
		}

		DoCommandP(this->vehicle->tile, this->vehicle->index + (sel_ord << 20), MOF_UNLOAD | (unload_type << 4), CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));

		/* Transfer orders with leave empty as default */
		if (unload_type == OUFB_TRANSFER) {
			DoCommandP(this->vehicle->tile, this->vehicle->index + (sel_ord << 20), MOF_LOAD | (OLFB_NO_LOAD << 4), CMD_MODIFY_ORDER);
			this->SetWidgetDirty(WID_VT_FULL_LOAD_DROPDOWN);
		}
	}

	/**
	 * Handle the click on the nonstop button.
	 * @param non_stop what non-stop type to use; -1 to use the 'next' one.
	 */
	void ProcessNonStopClick(int non_stop)
	{
		if (!this->vehicle->IsGroundVehicle()) return;

		VehicleOrderID sel_ord = this->GetSelectedOrderID();
		const Order *order = this->vehicle->GetOrder(sel_ord);

		if (order == NULL || order->GetNonStopType() == non_stop) return;

		/* Keypress if negative, so 'toggle' to the next */
		if (non_stop < 0) {
			non_stop = order->GetNonStopType() ^ ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS;
		}

		this->SetWidgetDirty(WID_VT_NON_STOP_STATION_DROPDOWN);
		this->SetWidgetDirty(WID_VT_NON_STOP_WAYPOINT_DROPDOWN);
		this->SetWidgetDirty(WID_VT_NON_STOP_DEPOT_DROPDOWN);
		DoCommandP(this->vehicle->tile, this->vehicle->index + (sel_ord << 20), MOF_NON_STOP | non_stop << 4,  CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
	}

	/**
	 * Handle the click on the goto button.
	 */
	void ProcessGotoClick()
	{
		this->SetWidgetDirty(WID_VT_GOTO_BUTTON);
		this->ToggleWidgetLoweredState(WID_VT_GOTO_BUTTON);
		if (this->IsWidgetLowered(WID_VT_GOTO_BUTTON)) {
			SetObjectToPlaceWnd(ANIMCURSOR_PICKSTATION, PAL_NONE, HT_RECT | HT_VEHICLE, this);
			this->place_object_type = TIMETABLE_POS_GOTO;
		} else {
			ResetObjectToPlace();
		}
	}

	/**
	 * Handle the click on the service in nearest depot button.
	 */
	void ProcessGotoNearestDepotClick()
	{
		Order order;
		order.next = NULL;
		order.index = 0;
		order.MakeGoToDepot(0, ODTFB_PART_OF_ORDERS,
				_settings_client.gui.new_nonstop && this->vehicle->IsGroundVehicle() ? ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS : ONSF_STOP_EVERYWHERE);
		order.SetDepotActionType(ODATFB_NEAREST_DEPOT);

		VehicleOrderID order_id = (IsDestinationLineSelected() || IsTimetableLineSelected() ? this->GetSelectedOrderID() : this->vehicle->GetNumOrders());
		DoCommandP(this->vehicle->tile, this->vehicle->index + (order_id << 20), order.Pack(), CMD_INSERT_ORDER | CMD_MSG(STR_ERROR_CAN_T_INSERT_NEW_ORDER));
	}

	/**
	 * Handle the click on the refit button.
	 * If ctrl is pressed, cancel refitting, else show the refit window.
	 * @param i Selected refit command.
	 * @param auto_refit Select refit for auto-refitting.
	 */
	void ProcessRefitClick(int i, bool auto_refit)
	{
		if (_ctrl_pressed) {
			/* Cancel refitting */
			DoCommandP(this->vehicle->tile, this->vehicle->index, (this->GetSelectedOrderID() << 16) | (CT_NO_REFIT << 8) | CT_NO_REFIT, CMD_ORDER_REFIT);
		} else {
			if (i == 1) { // Auto-refit to available cargo type.
				DoCommandP(this->vehicle->tile, this->vehicle->index, (this->GetSelectedOrderID() << 16) | CT_AUTO_REFIT, CMD_ORDER_REFIT);
			} else {
				ShowVehicleRefitWindow(this->vehicle, this->GetSelectedOrderID(), this, auto_refit);
			}
		}
	}

	/**
	 * Handle the click on the service.
	 */
	void ProcessServiceClick(int i)
	{
		VehicleOrderID sel_ord = this->GetSelectedOrderID();

		if (i < 0) {
			const Order *order = this->vehicle->GetOrder(sel_ord);
			if (order == NULL) return;
			i = (order->GetDepotOrderType() & ODTFB_SERVICE) ? DA_ALWAYS_GO : DA_SERVICE;
		}
		DoCommandP(this->vehicle->tile, this->vehicle->index + (sel_ord << 20), MOF_DEPOT_ACTION | (i << 4), CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
	}

	void ProcessSetDepartureClick()
	{
		const Order *o = this->vehicle->GetOrder(this->GetSelectedOrderID());

		/* We try to choose an appropriate default date for the choose dialog.
		 * The goal is not, that we exactly offer the indented date (the probability to do so is rather low),
		 * but saving the user clicks, i.e. coming close to the intended date.
		 * Thus, we offer a date somewhat after the last defined date, use some default values for traviling
		 * and stopping.  If we do not find a previous order with a timetabled value, we just use the start date
		 * of the vehicles timetable. */
		Date default_date = INVALID_DATE;
		int assumed_offset = (IsNonStopOrder(o) ? 0 : DEFAULT_STATION_TIME);

		if (o->HasDeparture()) {
			/* If there is already a departure set, use it as a default date */
			default_date = o->GetDeparture();
		} else if (o->HasArrival()) {
			default_date = o->GetArrival() + assumed_offset;
		} else {
			/* Note concerning the loop: VehicleOrderIDs are unsigned.  Thus, to check wether we passed the first one, we must
			 * test for 0xFF, i.e. INVALID_VEH_ORDER_ID. */
			for (VehicleOrderID prev_order_id = this->GetSelectedOrderID() - 1; prev_order_id != INVALID_VEH_ORDER_ID; prev_order_id--) {
				Order *prev_order = this->vehicle->GetOrder(prev_order_id);

				assumed_offset += DEFAULT_TRAVEL_TIME;
				if (prev_order->HasDeparture()) {
					default_date = prev_order->GetDeparture() + assumed_offset;
					break;
				}

				assumed_offset += (IsNonStopOrder(prev_order) ? 0 : DEFAULT_STATION_TIME);
				if (prev_order->HasArrival()) {
					default_date = prev_order->GetArrival() + assumed_offset;
					break;
				}
			}
		}

		Date timetable_start = this->vehicle->timetable_start;
		Date timetable_end = this->vehicle->timetable_end;

		/* If we did not find a appropriate default date, just take the timetable start.  Otherwise, apply the vehicles timetable offset. */
		if (default_date == INVALID_DATE) {
			default_date = timetable_start;
		} else {
			Duration offset = vehicle->timetable_offset;
			default_date = AddToDate(default_date, offset);
		}

		/* Offering a date the user may not use makes no sense */
		default_date = Clamp(default_date, timetable_start, timetable_end - 1);

		char buffer[256] = "";

		GetDepartureQueryCaption(this->vehicle, buffer, lastof(buffer), o);

		ShowSetDateFastWindow(this, this->vehicle->index, default_date, timetable_start, timetable_end, buffer, _timetable_setdate_offsets, _timetable_setdate_strings, TimetableWindow::SetDepartureCallback);
	}

	void ProcessSetSpeedLimitClick()
	{
		const Order *order = this->vehicle->GetOrder(this->GetSelectedOrderID());

		StringID current = STR_EMPTY;
		if (order->GetMaxSpeed() != UINT16_MAX) {
			SetDParam(0, ConvertKmhishSpeedToDisplaySpeed(order->GetMaxSpeed()));
			current = STR_JUST_INT;
		}

		this->query_type = TQT_SPEED;
		ShowQueryString(current, STR_TIMETABLE_CHANGE_SPEED, 31, this, CS_NUMERAL, QSF_NONE);
	}

	void ProcessSetArrivalClick()
	{
		const Order *o = this->vehicle->GetOrder(this->GetSelectedOrderID());

		/* We try to choose an appropriate default date for the choose dialog.
		 * The goal is not, that we exactly offer the indented date (the probability to do so is rather low),
		 * but saving the user clicks, i.e. coming close to the intended date.
		 * Thus, we offer a date somewhat after the last defined date, use some default values for traviling
		 * and stopping.  If we do not find a previous order with a timetabled value, we just use the start date
		 * of the vehicles timetable. */
		Date default_date = INVALID_DATE;
		int assumed_offset = 0;

		if (o->HasArrival()) {
			default_date = o->GetArrival();
		} else {
			/* Note concerning the loop: VehicleOrderIDs are unsigned.  Thus, to check wether we passed the first one, we must
			 * test for 0xFF, i.e. INVALID_VEH_ORDER_ID. */
			for (VehicleOrderID prev_order_id = this->GetSelectedOrderID() - 1; prev_order_id != INVALID_VEH_ORDER_ID; prev_order_id--) {
				Order *prev_order = this->vehicle->GetOrder(prev_order_id);

				assumed_offset += DEFAULT_TRAVEL_TIME;
				if (prev_order->HasDeparture()) {
					default_date = prev_order->GetDeparture() + assumed_offset;
					break;
				}

				assumed_offset += (IsNonStopOrder(prev_order) ? 0 : DEFAULT_STATION_TIME);
				if (prev_order->HasArrival()) {
					default_date = prev_order->GetArrival() + assumed_offset;
					break;
				}
			}
		}

		Date timetable_start = this->vehicle->timetable_start;
		Date timetable_end = this->vehicle->timetable_end;

		/* If we did not find a appropriate default date, just take the timetable start.  Otherwise, apply the vehicles timetable offset. */
		if (default_date == INVALID_DATE) {
			default_date = timetable_start;
		} else {
			Duration offset = vehicle->timetable_offset;
			default_date = AddToDate(default_date, offset);
		}

		/* Offering a date the user may not use makes no sense */
		default_date = Clamp(default_date, timetable_start, timetable_end - 1);

		char buffer[256] = "";
		GetArrivalQueryCaption(this->vehicle, buffer, lastof(buffer), o);

		ShowSetDateFastWindow(this, this->vehicle->index, default_date, timetable_start, timetable_end, buffer, _timetable_setdate_offsets, _timetable_setdate_strings, TimetableWindow::SetArrivalCallback);
	}

	/**
	 * Handle the click on the conditional order button.
	 * @param i Dummy parameter.
	 */
	void ProcessGotoConditionalClick()
	{
		this->LowerWidget(WID_VT_GOTO_BUTTON);
		this->SetWidgetDirty(WID_VT_GOTO_BUTTON);
		SetObjectToPlaceWnd(ANIMCURSOR_PICKSTATION, PAL_NONE, HT_NONE, this);
		this->place_object_type = TIMETABLE_POS_CONDITIONAL;
	}

	/**
	 * Handle the click on the share button.
	 * @param i Dummy parameter.
	 */
	void ProcessGotoShareClick()
	{
		this->LowerWidget(WID_VT_GOTO_BUTTON);
		this->SetWidgetDirty(WID_VT_GOTO_BUTTON);
		SetObjectToPlaceWnd(ANIMCURSOR_PICKSTATION, PAL_NONE, HT_VEHICLE, this);
		this->place_object_type = TIMETABLE_POS_SHARE;
	}

	/**
	 * Handle the click on the skip button.
	 * If ctrl is pressed, skip to selected order, else skip to current order + 1
	 * @param i Dummy parameter.
	 */
	void ProcessSkipClick()
	{
		VehicleOrderID order_id = (IsDestinationLineSelected() || IsTimetableLineSelected() ? this->GetSelectedOrderID() : this->vehicle->GetNumOrders());

		/* Don't skip when there's nothing to skip */
		if (_ctrl_pressed && this->vehicle->cur_implicit_order_index == order_id) return;
		if (this->vehicle->GetNumOrders() <= 1) return;

		DoCommandP(this->vehicle->tile, this->vehicle->index, _ctrl_pressed ? order_id : ((this->vehicle->cur_implicit_order_index + 1) % this->vehicle->GetNumOrders()),
				CMD_SKIP_TO_ORDER | CMD_MSG(_ctrl_pressed ? STR_ERROR_CAN_T_SKIP_TO_ORDER : STR_ERROR_CAN_T_SKIP_ORDER));
	}

	/**
	 * Handle the click on the delete button.
	 * @param i Dummy parameter.
	 */
	void ProcessDeleteClick()
	{
		VehicleOrderID order_id = ((IsDestinationLineSelected() || IsTimetableLineSelected()) ? this->GetSelectedOrderID() : this->vehicle->GetNumOrders());
		if (DoCommandP(this->vehicle->tile, this->vehicle->index, order_id, CMD_DELETE_ORDER | CMD_MSG(STR_ERROR_CAN_T_DELETE_THIS_ORDER))) {
			this->UpdateButtonState();
		}
	}

	/**
	 * Handle the click on the 'stop sharing' button.
	 * If 'End of Shared Orders' isn't selected, do nothing. If Ctrl is pressed, call OrderClick_Delete and exit.
	 * To stop sharing this vehicle order list, we copy the orders of a vehicle that share this order list. That way we
	 * exit the group of shared vehicles while keeping the same order list.
	 * @param i Dummy parameter.
	 */
	void ProcessStopSharingClick()
	{
		/* Don't try to stop sharing orders if 'End of Shared Orders' isn't selected. */
		if (!this->vehicle->IsOrderListShared() || !IsEndOfOrdersLineSelected()) return;
		/* If Ctrl is pressed, delete the order list as if we clicked the 'Delete' button. */
		if (_ctrl_pressed) {
			this->ProcessDeleteClick();
			return;
		}

		/* Get another vehicle that share orders with this vehicle. */
		Vehicle *other_shared = (this->vehicle->FirstShared() == this->vehicle) ? this->vehicle->NextShared() : this->vehicle->PreviousShared();
		/* Copy the order list of the other vehicle. */
		if (DoCommandP(this->vehicle->tile, this->vehicle->index | CO_COPY << 30, other_shared->index, CMD_CLONE_ORDER | CMD_MSG(STR_ERROR_CAN_T_STOP_SHARING_ORDER_LIST))) {
			this->UpdateButtonState();
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		switch (widget) {
			case WID_VT_NON_STOP_STATION_DROPDOWN:
			case WID_VT_NON_STOP_WAYPOINT_DROPDOWN:
			case WID_VT_NON_STOP_DEPOT_DROPDOWN:
				this->ProcessNonStopClick(index);
				break;

			case WID_VT_FULL_LOAD_DROPDOWN:
				this->ProcessFullLoadClick(index);
				break;

			case WID_VT_UNLOAD_DROPDOWN:
				this->ProcessUnloadClick(index);
				break;

			case WID_VT_COND_VARIABLE_DROPDOWN:
				DoCommandP(this->vehicle->tile, this->vehicle->index + (this->GetSelectedOrderID() << 20), MOF_COND_VARIABLE | index << 4,  CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
				break;

			case WID_VT_COND_COMPARATOR_DROPDOWN:
				DoCommandP(this->vehicle->tile, this->vehicle->index + (this->GetSelectedOrderID() << 20), MOF_COND_COMPARATOR | index << 4,  CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
				break;

			case WID_VT_REFIT_AUTO_DROPDOWN:
				this->ProcessRefitClick(index, true);
				break;

			case WID_VT_SERVICE_DROPDOWN:
				this->ProcessServiceClick(index);
				break;

			case WID_VT_GOTO_BUTTON:
				switch (index) {
					case 0: this->ProcessGotoClick(); break;
					case 1: this->ProcessGotoNearestDepotClick(); break;
					case 2: this->ProcessGotoConditionalClick(); break;
					case 3: this->ProcessGotoShareClick(); break;
					default: NOT_REACHED();
				}
				break;
		}
	}


	/**********************************************************************************************************************/
	/*********************************************************** Drawing **************************************************/
	/**********************************************************************************************************************/

	static void DrawTabularTimetableLine(const Vehicle *vehicle, VehicleOrderID order_id, Dimension dest_bounding_box, Dimension date_bounding_box,
										 Dimension speed_bounding_box, int x1, int x2, int y, TextColour colour)
	{
		const Order *order = vehicle->GetOrder(order_id);

		int curr_x = x1 + dest_bounding_box.width;
		StringID str;

		/* First draw the destination column, aligned to the right in order to paint the destinations closer to the timetable information */
       if (order->IsWaypointOrder()) {
			SetDParam(0, order->GetDestination());
			str = STR_WAYPOINT_NAME;
		} else if (order->IsDepotOrder()) {
			SetDParam(0, order->GetDestination());
			str = STR_DEPOT_NAME;
		} else if (order->IsStationOrder()) {
			SetDParam(0, order->GetDestination());
			str = STR_STATION_NAME;
		} else {
			assert(false);
		}

		DrawString(x1, curr_x, y, str, colour, SA_RIGHT);

		/* Then draw the arrival and departure dates.  If departure and arrival are equal, e.g. in a waypoint, draw one date centered,
		 * otherwise draw the arrival in the left column and the departure in the right column. */
		int date_width = date_bounding_box.width + 10;
 		Duration offset = vehicle->timetable_offset;
		Date arrival_date = (order->HasArrival() ? AddToDate(order->GetArrival(), offset) : INVALID_DATE);
		Date departure_date = (order->HasDeparture() ? AddToDate(order->GetDeparture(), offset) : INVALID_DATE);

		if (arrival_date != INVALID_DATE && departure_date != INVALID_DATE && arrival_date == departure_date) {
			SetDParam(0, arrival_date);
			DrawString(curr_x + date_width / 2, curr_x + date_width + date_width / 2, y, STR_JUST_DATE_LONG, colour, SA_HOR_CENTER);
		} else {
			if (arrival_date != INVALID_DATE) {
				SetDParam(0, arrival_date);
				DrawString(curr_x, curr_x + date_width, y, STR_JUST_DATE_LONG, colour, SA_HOR_CENTER);
			}
			if (departure_date != INVALID_DATE) {
				SetDParam(0, departure_date);
				DrawString(curr_x + date_width, curr_x + 2 * date_width, y, STR_JUST_DATE_LONG, colour, SA_HOR_CENTER);
			}
		}

		curr_x += 2 * date_width;

		/* Draw the speed limit (if it exists) */

		int speed_width = speed_bounding_box.width + 20;
		if (order->GetMaxSpeed() != UINT16_MAX) {
			SetDParam(0, order->GetMaxSpeed());
			DrawString(curr_x, curr_x + speed_width, y, STR_JUST_VELOCITY, colour, SA_RIGHT);
		}

		curr_x += speed_width;

	}

public:
	TimetableWindow(WindowDesc *desc, WindowNumber window_number) :
			Window(desc),
			selected_timetable_line(INVALID_SELECTION),
			property_line_selected(false),
			vehicle_interval_line_selected(false),
			vehicle(Vehicle::Get(window_number))
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_VT_SCROLLBAR);
		this->FinishInitNested(window_number);

		this->owner = this->vehicle->owner;
		this->order_over = INVALID_VEH_ORDER_ID;

		this->UpdateAutoRefitState();

		this->filter_mode = TFM_SHOW_ALL;
		this->SetWidgetLoweredState(WID_VT_FULL_FILTER_BUTTON, true);

		// Trigger certain refresh activities e.g. regarding button state
		this->OnInvalidateData(-2);
	}

	/** Selects the line corresponding to the given order_id.
	 *  @param order_id the VehicleOrderID inside the order list
	 */
	void SelectTimetableLineForOrder(VehicleOrderID order_id)
	{
		this->selected_timetable_line = (IsInShowAllMode() ? (2 * order_id) + 1 : order_id);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case WID_VT_TIMETABLE_PANEL: {
				resize->height = FONT_HEIGHT_NORMAL;
				size->height = WD_FRAMERECT_TOP + 8 * resize->height + WD_FRAMERECT_BOTTOM;
				break;
			}

			case WID_VT_SUMMARY_PANEL: {
				Dimension d = {0, 0};
				this->PrepareForPropertyLine();
				d = maxdim(d, GetStringBoundingBox(STR_TIMETABLE_PROPERTY_LINE));
				
				this->PrepareForVehicleIntervalLine();
				d = maxdim(d, GetStringBoundingBox(STR_TIMETABLE_VEHICLE_INTERVAL_LINE));

				d.width += padding.width + WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;

				size->width = max(size->width, d.width);
 				size->height = WD_FRAMERECT_TOP + 2 * FONT_HEIGHT_NORMAL + WD_FRAMERECT_BOTTOM;
 				break;
			}

			case WID_VT_COND_VARIABLE_DROPDOWN: {
				Dimension d = {0, 0};
				for (uint i = 0; i < lengthof(_order_conditional_variable); i++) {
					d = maxdim(d, GetStringBoundingBox(STR_ORDER_CONDITIONAL_LOAD_PERCENTAGE + _order_conditional_variable[i]));
				}
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case WID_VT_COND_COMPARATOR_DROPDOWN:
				this->EnlargeSizeForDropdownIfNeeded(_order_conditional_condition, size, padding);
				break;

			case WID_VT_NON_STOP_STATION_DROPDOWN:
			case WID_VT_NON_STOP_WAYPOINT_DROPDOWN:
			case WID_VT_NON_STOP_DEPOT_DROPDOWN:
				this->EnlargeSizeForDropdownIfNeeded(_order_non_stop_dropdown, size, padding);
				break;

			case WID_VT_FULL_LOAD_DROPDOWN:
				this->EnlargeSizeForDropdownIfNeeded(_order_full_load_dropdown, size, padding);
				break;

			case WID_VT_UNLOAD_DROPDOWN:
				this->EnlargeSizeForDropdownIfNeeded(_order_unload_dropdown, size, padding);
				break;

			case WID_VT_REFIT_AUTO_DROPDOWN:
				this->EnlargeSizeForDropdownIfNeeded(_order_refit_action_dropdown, size, padding);
				break;

			case WID_VT_SERVICE_DROPDOWN:
				this->EnlargeSizeForDropdownIfNeeded(_order_depot_action_dropdown, size, padding);
				break;
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		switch (data) {
			case VIWD_AUTOREPLACE:
				/* Autoreplace replaced the vehicle */
				this->vehicle = Vehicle::Get(this->window_number);
				this->UpdateAutoRefitState();
				break;

			case VIWD_REMOVE_ALL_ORDERS:
				/* Removed / replaced all orders (after deleting / sharing) */
				if (this->selected_timetable_line == INVALID_SELECTION) break;

				this->DeleteChildWindows();
				HideDropDownMenu(this);
				UpdateSelectedTimetableLine(INVALID_SELECTION);
				break;

			case VIWD_MODIFY_ORDERS:
				if (!gui_scope) break;
				this->ReInit();
				break;

			default: {
				if (gui_scope) break; // only do this once; from command scope

				/* Moving an order. If one of these is INVALID_VEH_ORDER_ID, then
				 * the order is being created / removed */
				if (this->selected_timetable_line == INVALID_SELECTION) break;

				VehicleOrderID from = GB(data, 0, 8);
				VehicleOrderID to   = GB(data, 8, 8);

				if (from == to) break; /* no need to change anything */

				VehicleOrderID old_selected_order = GetSelectedOrderID();
				if (selected_timetable_line == INVALID_SELECTION) {
					/* If there is no selection, we don´t have to adjust one */
					break;
				}

				if (to == INVALID_VEH_ORDER_ID) {
					if (old_selected_order == INVALID_VEH_ORDER_ID) {
						/* The case that selection scrolls out to the end of orders line */
						selected_timetable_line = GetEndOfOrdersIndex();
						this->property_line_selected = false;
						this->vehicle_interval_line_selected = false;
					} else if (from < old_selected_order) {
						/* a line above the current selection was deleted, thus selection scrolls by one line towards the top of the list */
						AdjustSelectionByNumberOfOrders(-1);
					} else if (from == old_selected_order) {
						/* the currently selected order was deleted.  Keep the selection in the same line, but adress the possible case that that line
						   now is below the end of orders line. */
						if (selected_timetable_line > GetEndOfOrdersIndex()) {
							selected_timetable_line = GetEndOfOrdersIndex();
							this->property_line_selected = false;
							this->vehicle_interval_line_selected = false;
						}
					} else {
						/* Deleting an order below the currently selected one does not cause any need for changing the selection. */
					}
				}

				if (from == INVALID_VEH_ORDER_ID) {
					if (to <= old_selected_order) {
						/* a line was added above or at the index of the current selection.  Keep the same order selected. */
						AdjustSelectionByNumberOfOrders(1);
					} else {
						/* Again, we don´t care about anything happening below the current selection */
					}
				}

				/* Scroll to the new order. */
				if (from == INVALID_VEH_ORDER_ID && to != INVALID_VEH_ORDER_ID && !this->vscroll->IsVisible(to)) {
					this->vscroll->ScrollTowards(to);
				}
				break;
			}
		}

		this->vscroll->SetCount(this->vehicle->GetNumOrders() + 1);
		if (gui_scope) this->UpdateButtonState();
	}


	virtual void OnPaint()
	{
		this->DrawWidgets();

 		const Vehicle *v = this->vehicle;
		this->vscroll->SetCount(v->GetNumOrders() * GetLineMultiplier() + 1);
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_VT_COND_VALUE_BUTTON: {
				VehicleOrderID sel = this->GetSelectedOrderID();
				const Order *order = this->vehicle->GetOrder(sel);

				if (order != NULL && order->IsType(OT_CONDITIONAL)) {
					uint value = order->GetConditionValue();
					if (order->GetConditionVariable() == OCV_MAX_SPEED) value = ConvertSpeedToDisplaySpeed(value);
					SetDParam(0, value);
				}
				break;
			}
			case WID_VT_CAPTION: {
				SetDParam(0, this->vehicle->index);

				const OrderList *order_list = this->vehicle->orders.list;
				const char* timetable_name = (order_list != NULL ? order_list->GetName() : NULL);
				if (timetable_name != NULL) {
					SetDParamStr(1, " ");
					SetDParamStr(2, timetable_name);
				} else {
					SetDParamStr(1, "");
					SetDParamStr(2, "");
				}

				break;
			}
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
 		const Vehicle *v = this->vehicle;
		int selected = this->selected_timetable_line;

		/* If the filter mode is set to timetable only, we display a tabular view with timetable information.
	     * The destination column is painted left, its width is set according to the width of the biggest
		 * string bounding box for a destination of the vehicle. */
		Dimension dest_bounding_box;
		if (IsInShowTimetableMode()) {
			dest_bounding_box = GetMaxOrderStringBoundingBox(v);
		}

		Date some_date = ConvertYMDToDate(2222, 11, 30);
		SetDParam(0, some_date);
		Dimension date_bounding_box = GetStringBoundingBox(STR_JUST_DATE_LONG);

		SetDParam(0, ConvertKmhishSpeedToDisplaySpeed(1000));
		Dimension speed_bounding_box = GetStringBoundingBox(STR_JUST_VELOCITY);

		switch (widget) {
			case WID_VT_TIMETABLE_PANEL: {
				int y = r.top + WD_FRAMERECT_TOP;
				int i = this->vscroll->GetPosition();
				VehicleOrderID order_id = (IsInShowAllMode() ? (i + 1) / GetLineMultiplier() : i);

				bool rtl = _current_text_dir == TD_RTL;
				SetDParamMaxValue(0, v->GetNumOrders(), 2);
				int index_column_width = GetStringBoundingBox(STR_ORDER_INDEX).width + 2 * GetSpriteSize(rtl ? SPR_ARROW_RIGHT : SPR_ARROW_LEFT).width + 3;
				int middle = rtl ? r.right - WD_FRAMERECT_RIGHT - index_column_width : r.left + WD_FRAMERECT_LEFT + index_column_width;

				const Order *order = v->GetOrder(order_id);
				bool any_order = (order != NULL);
				while (order != NULL) {
					/* Don't draw anything if it extends past the end of the window. */
					if (!this->vscroll->IsVisible(i)) break;

					if ((IsInShowAllMode() && i % 2 == 0) || IsInShowDestinationsMode()) {
						DrawOrderString(v, order, order_id, y, i == selected, r.left + WD_FRAMERECT_LEFT, middle, r.right - WD_FRAMERECT_RIGHT);
						if (IsInShowDestinationsMode()) {
							order_id++;
							if (order_id >= v->GetNumOrders()) {
								break;
							} else {
								order = order->next;
							}
						}
					} else {
						StringID string;
						TextColour colour = (i == selected) ? TC_WHITE : TC_BLACK;
						if (order->IsType(OT_CONDITIONAL)) {
							string = STR_TIMETABLE_NO_TRAVEL;
							if (IsInShowTimetableMode()) {
								DrawOrderMarker(this->vehicle, order_id, y, r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT);
							}
							DrawString(rtl ? r.left + WD_FRAMERECT_LEFT : middle, rtl ? middle : r.right - WD_FRAMERECT_LEFT, y, string, colour);
						} else if (order->IsType(OT_IMPLICIT)) {
							string = STR_TIMETABLE_NOT_TIMETABLEABLE;
							colour = ((i == selected) ? TC_SILVER : TC_GREY) | TC_NO_SHADE;
							if (IsInShowTimetableMode()) {
								DrawOrderMarker(this->vehicle, order_id, y, r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT);
							}
						} else {
							/* Mark orders which violate the time order, e.g. because arrival is later than departure. */
							if (!IsOrderTimetableValid(v, order)) {
								colour = TC_RED;
							}
							/* Mark orders which violate the time order, e.g. because arrival is later than departure. */
							if (!IsOrderTimetableValid(this->vehicle, order)) {
								colour = TC_RED;
							}

							// If we are in the mode "show only timetable information", then we draw it in a tabular manner.
							if (IsInShowTimetableMode()) {
								DrawOrderMarker(this->vehicle, order_id, y, r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT);
								// TODO: Replace approximation for the order marker width by a calculated value
							    DrawTabularTimetableLine(this->vehicle, order_id, dest_bounding_box, date_bounding_box, speed_bounding_box,
														 r.left + WD_FRAMERECT_LEFT + 30, r.right - WD_FRAMERECT_LEFT, y, colour);
							} else {
								char buffer[2048] = "";
								const char* timetable_string = this->GetTimetableLineString(buffer, lastof(buffer), order, order_id);
								DrawString(rtl ? r.left + WD_FRAMERECT_LEFT : middle, rtl ? middle : r.right - WD_FRAMERECT_LEFT, y, timetable_string, colour);
							}
						}
						order_id++;

						if (order_id >= v->GetNumOrders()) {
							break;
						} else {
							order = order->next;
						}
					}

					i++;
					y += FONT_HEIGHT_NORMAL;
				}

				if (any_order) {
					i++;
					y += FONT_HEIGHT_NORMAL;
				}
				if (this->vscroll->IsVisible(i)) {
					StringID str = this->vehicle->IsOrderListShared() ? STR_ORDERS_END_OF_SHARED_ORDERS : STR_ORDERS_END_OF_ORDERS;
					DrawString(rtl ? r.left + WD_FRAMETEXT_LEFT : middle, rtl ? middle : r.right - WD_FRAMETEXT_RIGHT, y, str, (i == selected) ? TC_WHITE : TC_BLACK);
				}

				break;
			}

			case WID_VT_SUMMARY_PANEL: {
				int y = r.top + WD_FRAMERECT_TOP;
				
				const OrderList *order_list = this->vehicle->orders.list;
				int delay_info_width = this->GetDelayInfoWidth();

				this->PrepareForPropertyLine();
				TextColour offset_color = IsPropertyLineSelected() ? TC_WHITE : TC_BLACK;
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_TIMETABLE_PROPERTY_LINE, offset_color);

				int32 lateness_counter = this->vehicle->lateness_counter;
				if (lateness_counter != 0) {
					SetDParam(0, (lateness_counter < 0 ? -lateness_counter : lateness_counter));
					DrawString(r.right - WD_FRAMERECT_RIGHT - delay_info_width, r.right - WD_FRAMERECT_RIGHT, y, STR_TIMETABLE_DAYS, TC_BLACK, SA_HOR_CENTER);
				} else {
					DrawString(r.right - WD_FRAMERECT_RIGHT - delay_info_width, r.right - WD_FRAMERECT_RIGHT, y, STR_TIMETABLE_ON_TIME_UPPER, TC_BLACK, SA_HOR_CENTER);
				}

				y += FONT_HEIGHT_NORMAL;

				this->PrepareForVehicleIntervalLine();
				TextColour status_color = IsVehicleIntervalLineSelected() ? TC_WHITE : TC_BLACK;
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT - delay_info_width, y, STR_TIMETABLE_VEHICLE_INTERVAL_LINE, status_color);

				if (lateness_counter < 0) {
					DrawString(r.right - WD_FRAMERECT_RIGHT - delay_info_width, r.right - WD_FRAMERECT_RIGHT, y, STR_TIMETABLE_TOO_EARLY, TC_BLACK, SA_HOR_CENTER);
				} else if (lateness_counter == 0) {
					DrawString(r.right - WD_FRAMERECT_RIGHT - delay_info_width, r.right - WD_FRAMERECT_RIGHT, y, STR_TIMETABLE_ON_TIME_LOWER, TC_BLACK, SA_HOR_CENTER);
				} else {
					DrawString(r.right - WD_FRAMERECT_RIGHT - delay_info_width, r.right - WD_FRAMERECT_RIGHT, y, STR_TIMETABLE_DELAY, TC_BLACK, SA_HOR_CENTER);
				}

				break;
			}
		}
	}

	/** Returns wether the given order is regarded as non stop order from the timetable perspective.  The orders for which
	 *  true is returned are those where we assume a difference of zero between arrival at the order destination and departure by default.
	 */
	static bool IsNonStopOrder(const Order *order)
	{
		return order->GetNonStopType() == ONSF_NO_STOP_AT_DESTINATION_STATION || order->GetNonStopType() == ONSF_NO_STOP_AT_ANY_STATION
			   || order->IsWaypointOrder() || order->IsDepotOrder();
	}

	/** Callback to be executed once the user has chosen some departure.  Depending on wether Choose & Close or Choose & Next was
     *  chosen in the datefast_gui, either the datefast_gui is just closed, or reopened for choosing the next arrival.
     *  @param w the corresponding timetable window
     *  @param date the chosen date
     *  @param choose_next wether the user hit Choose & Next
     */
	static void SetDepartureCallback(Window *w, Date date, bool choose_next)
	{
		TimetableWindow *timetable_window = (TimetableWindow*)w;
		if (timetable_window->IsTimetableLineSelected()) {
			VehicleOrderID selected_order_id = timetable_window->GetSelectedOrderID();
			Order *selected_order = timetable_window->vehicle->orders.list->GetOrderAt(selected_order_id);
			Duration vehicle_offset = timetable_window->vehicle->timetable_offset;

			uint32 p1 = ((uint32)timetable_window->vehicle->index << 16 | (uint32)selected_order->index);
			uint32 p2 = SubtractFromDate(date, vehicle_offset);
			DoCommandP(0, p1, p2, CMD_SET_ORDER_DEPARTURE | CMD_MSG(STR_ERROR_TIMETABLE_CAN_T_SET_DEPARTURE));

			if (choose_next) {
				VehicleOrderID next_order_id = (selected_order_id < timetable_window->vehicle->orders.list->GetNumOrders() - 1 ? selected_order_id + 1 : 0);
				Order *next_order = timetable_window->vehicle->orders.list->GetOrderAt(next_order_id);
				timetable_window->SelectTimetableLineForOrder(next_order_id);
				timetable_window->SetDirty();

				char buffer[256] = "";
				GetArrivalQueryCaption(timetable_window->vehicle, buffer, lastof(buffer), next_order);

				/* Let the user choose the next arrival, for initialization, make a guess that he will probably choose a date somewhat later than the just chosen date. */
				UpdateSetDateFastWindow(date + DEFAULT_TRAVEL_TIME, buffer, TimetableWindow::SetArrivalCallback);
			}
		}
	}

	/** Callback to be executed once the user has chosen some arrival.  Depending on wether Choose & Close or Choose & Next was
     *  chosen in the datefast_gui, either the datefast_gui is just closed, or reopened for choosing the next departure.
     *  @param w the corresponding timetable window
     *  @param date the chosen date
     *  @param choose_next wether the user hit Choose & Next
     */
	static void SetArrivalCallback(Window *w, Date date, bool choose_next)
	{
		TimetableWindow *timetable_window = (TimetableWindow*)w;
		if (timetable_window->IsTimetableLineSelected()) {
			VehicleOrderID selected_order_id = timetable_window->GetSelectedOrderID();
			Order *order = timetable_window->vehicle->orders.list->GetOrderAt(selected_order_id);
			Duration vehicle_offset = timetable_window->vehicle->timetable_offset;

			uint32 p1 = ((uint32)timetable_window->vehicle->index << 16 | (uint32)order->index);
			uint32 p2 = SubtractFromDate(date, vehicle_offset);
			DoCommandP(0, p1, p2, CMD_SET_ORDER_ARRIVAL | CMD_MSG(STR_ERROR_TIMETABLE_CAN_T_SET_ARRIVAL));

			bool non_stop_order = IsNonStopOrder(order);
			/* If the order does not imply stopping at some location, set the departure equal to the arrival */
			if (non_stop_order) {
				p1 = ((uint32)timetable_window->vehicle->index << 16 | (uint32)order->index);
				DoCommandP(0, p1, p2, CMD_SET_ORDER_DEPARTURE | CMD_MSG(STR_ERROR_TIMETABLE_CAN_T_SET_ARRIVAL));
			}

			if (choose_next) {
				char buffer[256] = "";
				if (non_stop_order) {
					VehicleOrderID next_order_id = (selected_order_id < timetable_window->vehicle->orders.list->GetNumOrders() - 1 ? selected_order_id + 1 : 0);
					Order *next_order = timetable_window->vehicle->orders.list->GetOrderAt(next_order_id);
					timetable_window->SelectTimetableLineForOrder(next_order_id);
					GetArrivalQueryCaption(timetable_window->vehicle, buffer, lastof(buffer), next_order);
				} else {
					GetDepartureQueryCaption(timetable_window->vehicle, buffer, lastof(buffer), order);
				}

				/* Let the user choose the next departure, for initialization, make a guess that he will probably choose a date somewhat later than the just chosen date. */
				UpdateSetDateFastWindow(date + DEFAULT_STATION_TIME, buffer, (non_stop_order ? TimetableWindow::SetArrivalCallback : TimetableWindow::SetDepartureCallback));
			}
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		const Vehicle *v = this->vehicle;

		switch (widget) {
			case WID_VT_FULL_FILTER_BUTTON:
				AdjustShowModeAfterFilterChange(this->filter_mode, TFM_SHOW_ALL);
				this->filter_mode = TFM_SHOW_ALL;
				this->SetWidgetLoweredState(WID_VT_FULL_FILTER_BUTTON, true);
				this->SetWidgetLoweredState(WID_VT_DESTINATION_FILTER_BUTTON, false);
				this->SetWidgetLoweredState(WID_VT_TIMETABLE_FILTER_BUTTON, false);
				this->InvalidateData();
				break;

			case WID_VT_DESTINATION_FILTER_BUTTON:
				AdjustShowModeAfterFilterChange(this->filter_mode, TFM_SHOW_DESTINATION_LINES);
				this->filter_mode = TFM_SHOW_DESTINATION_LINES;
				this->SetWidgetLoweredState(WID_VT_FULL_FILTER_BUTTON, false);
				this->SetWidgetLoweredState(WID_VT_DESTINATION_FILTER_BUTTON, true);
				this->SetWidgetLoweredState(WID_VT_TIMETABLE_FILTER_BUTTON, false);
				this->InvalidateData();
				break;

			case WID_VT_TIMETABLE_FILTER_BUTTON:
				AdjustShowModeAfterFilterChange(this->filter_mode, TFM_SHOW_TIMETABLE_LINES);
				this->filter_mode = TFM_SHOW_TIMETABLE_LINES;
				this->SetWidgetLoweredState(WID_VT_FULL_FILTER_BUTTON, false);
				this->SetWidgetLoweredState(WID_VT_DESTINATION_FILTER_BUTTON, false);
				this->SetWidgetLoweredState(WID_VT_TIMETABLE_FILTER_BUTTON, true);
				this->InvalidateData();
				break;

			case WID_VT_ORDER_VIEW: // Order view button
				ShowOrdersWindow(v);
				break;

			case WID_VT_SUMMARY_PANEL: {
				ProcessMetaDataPanelClick(pt);
				break;
			}

			case WID_VT_TIMETABLE_PANEL: { // Main panel.
				ProcessListClick(pt);
				break;
			}

			case WID_VT_SHARED_ORDER_LIST:
				ShowVehicleListWindow(v);
				break;

			case WID_VT_NON_STOP_STATION_DROPDOWN:
			case WID_VT_NON_STOP_WAYPOINT_DROPDOWN:
			case WID_VT_NON_STOP_DEPOT_DROPDOWN: {
				if (this->GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->ProcessNonStopClick(-1);
				} else {
					const Order *o = this->vehicle->GetOrder(this->GetSelectedOrderID());
					ShowDropDownMenu(this, _order_non_stop_dropdown, o->GetNonStopType(), widget, 0,
													o->IsType(OT_GOTO_STATION) ? 0 : (o->IsType(OT_GOTO_WAYPOINT) ? 3 : 12));
				}
				break;
			}

			case WID_VT_COND_VARIABLE_DROPDOWN: {
				DropDownList *list = new DropDownList();
				for (uint i = 0; i < lengthof(_order_conditional_variable); i++) {
					*list->Append() = new DropDownListStringItem(STR_ORDER_CONDITIONAL_LOAD_PERCENTAGE + _order_conditional_variable[i], _order_conditional_variable[i], false);
				}
				ShowDropDownList(this, list, this->vehicle->GetOrder(this->GetSelectedOrderID())->GetConditionVariable(), WID_VT_COND_VARIABLE_DROPDOWN);
				break;
			}

			case WID_VT_FULL_LOAD_DROPDOWN: {
				if (this->GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->ProcessFullLoadClick(-1);
				} else {
					ShowDropDownMenu(this, _order_full_load_dropdown, this->vehicle->GetOrder(this->GetSelectedOrderID())->GetLoadType(), WID_VT_FULL_LOAD_DROPDOWN, 0, 2);
				}
				break;
			}

			case WID_VT_DEPARTURE_BUTTON: {
				this->ProcessSetDepartureClick();
				break;
			}

			case WID_VT_SPEEDLIMIT_BUTTON: {
				this->ProcessSetSpeedLimitClick();
				break;
			}

			case WID_VT_ARRIVAL_BUTTON: {
				this->ProcessSetArrivalClick();
				break;
			}

			case WID_VT_REFIT_BUTTON: {
				this->ProcessRefitClick(0, false);
				break;
			}

			case WID_VT_REFIT_BUTTON_4: {
				this->ProcessRefitClick(0, false);
				break;
			}

			case WID_VT_COND_COMPARATOR_DROPDOWN: {
				const Order *o = this->vehicle->GetOrder(this->GetSelectedOrderID());
				ShowDropDownMenu(this, _order_conditional_condition, o->GetConditionComparator(), WID_VT_COND_COMPARATOR_DROPDOWN, 0, (o->GetConditionVariable() == OCV_REQUIRES_SERVICE) ? 0x3F : 0xC0);
				break;
			}

			case WID_VT_UNLOAD_DROPDOWN: {
				if (this->GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->ProcessUnloadClick(-1);
				} else {
					ShowDropDownMenu(this, _order_unload_dropdown, this->vehicle->GetOrder(this->GetSelectedOrderID())->GetUnloadType(), WID_VT_UNLOAD_DROPDOWN, 0, 8);
				}
				break;
			}

			case WID_VT_SERVICE_DROPDOWN: {
				if (this->GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->ProcessServiceClick(-1);
				} else {
					ShowDropDownMenu(this, _order_depot_action_dropdown, DepotActionStringIndex(this->vehicle->GetOrder(this->GetSelectedOrderID())), WID_VT_SERVICE_DROPDOWN, 0, 0);
				}
				break;
			}

			case WID_VT_COND_VALUE_BUTTON: {
				const Order *order = this->vehicle->GetOrder(this->GetSelectedOrderID());
				uint value = order->GetConditionValue();
				if (order->GetConditionVariable() == OCV_MAX_SPEED) value = ConvertSpeedToDisplaySpeed(value);
				this->query_type = TQT_COND;
				SetDParam(0, value);
				ShowQueryString(STR_JUST_INT, STR_ORDER_CONDITIONAL_VALUE_CAPT, 5, this, CS_NUMERAL, QSF_NONE);
				break;
			}

			case WID_VT_REFIT_AUTO_DROPDOWN: {
				if (this->GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->ProcessRefitClick(0, true);
				} else {
					ShowDropDownMenu(this, _order_refit_action_dropdown, 0, WID_VT_REFIT_AUTO_DROPDOWN, 0, 0);
				}
				break;
			}

			case WID_VT_START_BUTTON: {
				Date default_date = _date;
				if (this->vehicle->orders.list != NULL && this->vehicle->orders.list->GetStartTime() != INVALID_DATE) {
					default_date = this->vehicle->orders.list->GetStartTime();
				}
				ShowSetDateWindow(this, this->vehicle->index, default_date, _cur_year - 5, _cur_year + 5, ChangeTimetableStartCallback);
				break;
			}

			case WID_VT_OFFSET_BUTTON: {
				Duration default_offset = (this->vehicle->timetable_offset.GetUnit() != DU_INVALID ? this->vehicle->timetable_offset : Duration(1, DU_MONTHS));
				ShowSetDurationWindow(this, this->vehicle->index, default_offset, true, STR_TIMETABLE_OFFSET_CAPTION, SetOffsetCallback);
				break;
			}

			case WID_VT_LENGTH_BUTTON: {
				Duration default_length = Duration(2, DU_MONTHS);
				if (this->vehicle->orders.list != NULL && this->vehicle->orders.list->GetTimetableDuration().GetUnit() != DU_INVALID) {
					default_length = this->vehicle->orders.list->GetTimetableDuration();
				}
				ShowSetDurationWindow(this, this->vehicle->index, default_length, false, STR_TIMETABLE_LENGTH_CAPTION, SetLengthCallback);
				break;
			}

			case WID_VT_SHIFT_BY_LENGTH_PAST_BUTTON: {
				DoCommandP(this->vehicle->tile, this->vehicle->index, (uint32)-1, CMD_SHIFT_TIMETABLE | CMD_MSG(STR_ERROR_TIMETABLE_CAN_T_SHIFT_TIMETABLE));
				break;
			}

			case WID_VT_SHIFT_BY_LENGTH_FUTURE_BUTTON: {
				DoCommandP(this->vehicle->tile, this->vehicle->index, (uint32)+1, CMD_SHIFT_TIMETABLE | CMD_MSG(STR_ERROR_TIMETABLE_CAN_T_SHIFT_TIMETABLE));
				break;
			}

			case WID_VT_RENAME_BUTTON: {
				ShowRenameTimetableWindow();
				break;
			}

			case WID_VT_GOTO_BUTTON: {
				if (this->GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->ProcessGotoClick();
				} else {
					ShowDropDownMenu(this, this->vehicle->type == VEH_AIRCRAFT ? _order_goto_dropdown_aircraft : _order_goto_dropdown, 0, WID_VT_GOTO_BUTTON, 0, 0);
				}
				break;
			}

			case WID_VT_DELETE_ORDER_BUTTON: {
				ProcessDeleteClick();
				break;
			}

			case WID_VT_STOP_SHARING_BUTTON: {
				ProcessStopSharingClick();
				break;
			}

			case WID_VT_SKIP_ORDER_BUTTON: {
				ProcessSkipClick();
				break;
			}
		}

		this->SetDirty();
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		if (this->place_object_type == TIMETABLE_POS_GOTO) {
			const Order cmd = GetOrderCmdFromTile(this->vehicle, tile);
			if (cmd.IsType(OT_NOTHING)) return;

			VehicleOrderID order_id = (IsDestinationLineSelected() || IsTimetableLineSelected() ? this->GetSelectedOrderID() : this->vehicle->GetNumOrders());
			if (DoCommandP(this->vehicle->tile, this->vehicle->index + (order_id << 20), cmd.Pack(), CMD_INSERT_ORDER | CMD_MSG(STR_ERROR_CAN_T_INSERT_NEW_ORDER))) {
				/* With quick goto the Go To button stays active */
				if (!_settings_client.gui.quick_goto) ResetObjectToPlace();
			}
		}
	}

	virtual bool OnVehicleSelect(const Vehicle *v)
	{
		/* v is vehicle getting orders. Only copy/clone orders if vehicle doesn't have any orders yet.
		 * We disallow copying orders of other vehicles if we already have at least one order entry
		 * ourself as it easily copies orders of vehicles within a station when we mean the station.
		 * Obviously if you press CTRL on a non-empty orders vehicle you know what you are doing
		 * TODO: give a warning message */
		bool share_order = _ctrl_pressed || this->place_object_type == TIMETABLE_POS_SHARE;
		if (this->vehicle->GetNumOrders() != 0 && !share_order) return false;

		if (DoCommandP(this->vehicle->tile, this->vehicle->index | (share_order ? CO_SHARE : CO_COPY) << 30, v->index,
				share_order ? CMD_CLONE_ORDER | CMD_MSG(STR_ERROR_CAN_T_SHARE_ORDER_LIST) : CMD_CLONE_ORDER | CMD_MSG(STR_ERROR_CAN_T_COPY_ORDER_LIST))) {
			UpdateSelectedTimetableLine(INVALID_SELECTION);
			ResetObjectToPlace();
		}
		return true;
	}

	virtual void OnPlaceObjectAbort()
	{
		this->RaiseWidget(WID_VT_GOTO_BUTTON);
		this->SetWidgetDirty(WID_VT_GOTO_BUTTON);

		/* Remove drag highlighting if it exists. */
		if (this->order_over != INVALID_VEH_ORDER_ID) {
			this->order_over = INVALID_VEH_ORDER_ID;
			this->SetWidgetDirty(WID_VT_TIMETABLE_PANEL);
		}
	}


	virtual void OnQueryTextFinished(char *str)
	{
		if (this->query_type == TQT_NAME) {
			if (str != NULL) {
				DoCommandP(0, this->vehicle->index, 0, CMD_RENAME_TIMETABLE | CMD_MSG(STR_ERROR_TIMETABLE_CAN_T_RENAME), NULL, str);
			}

			this->InvalidateData();
		} else if (this->query_type == TQT_SPEED) {
			const Order *order = this->vehicle->GetOrder(this->GetSelectedOrderID());
			uint32 p1 = ((uint32)this->vehicle->index << 16 | (uint32)order->index);
			uint32 p2 = str == NULL || StrEmpty(str) ? UINT16_MAX : strtoul(str, NULL, 10);

			DoCommandP(0, p1, p2, CMD_SET_ORDER_SPEED_LIMIT | CMD_MSG(STR_ERROR_TIMETABLE_CAN_T_SET_SPEEDLIMIT));
		} else if (this->query_type == TQT_COND) {
			if (!StrEmpty(str)) {
				VehicleOrderID sel = this->GetSelectedOrderID();
				uint value = atoi(str);

				switch (this->vehicle->GetOrder(sel)->GetConditionVariable()) {
					case OCV_MAX_SPEED:
						value = ConvertDisplaySpeedToSpeed(value);
						break;

					case OCV_RELIABILITY:
					case OCV_LOAD_PERCENTAGE:
						value = Clamp(value, 0, 100);
						break;

					default:
						break;
				}
				DoCommandP(this->vehicle->tile, this->vehicle->index + (sel << 20), MOF_COND_VALUE | Clamp(value, 0, 2047) << 4, CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
			}
		}
	}

	virtual void OnResize()
	{
		/* Update the scroll bar */
		this->vscroll->SetCapacityFromWidget(this, WID_VT_TIMETABLE_PANEL, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM);
	}

	virtual void OnDragDrop(Point pt, int widget)
	{
		switch (widget) {
			case WID_VT_TIMETABLE_PANEL: {
				VehicleOrderID from_order = this->GetSelectedOrderID();
				VehicleOrderID to_order = this->GetOrderID(this->GetLineFromPt(pt.y));

				if (!(from_order == to_order || from_order == INVALID_VEH_ORDER_ID || from_order > this->vehicle->GetNumOrders() || to_order == INVALID_VEH_ORDER_ID || to_order > this->vehicle->GetNumOrders()) &&
					DoCommandP(this->vehicle->tile, this->vehicle->index, from_order | (to_order << 16), CMD_MOVE_ORDER | CMD_MSG(STR_ERROR_CAN_T_MOVE_THIS_ORDER))) {
					UpdateSelectedTimetableLine(INVALID_SELECTION);
					this->UpdateButtonState();
				}
				break;
			}

			case WID_VT_DELETE_ORDER_BUTTON:
//				OrderClick_Delete(0);
				break;

			case WID_VT_STOP_SHARING_BUTTON:
//				this->OrderClick_StopSharing(0);
				break;
		}

		ResetObjectToPlace();

		if (this->order_over != INVALID_VEH_ORDER_ID) {
			/* End of drag-and-drop, hide dragged order destination highlight. */
			this->order_over = INVALID_VEH_ORDER_ID;
			this->SetWidgetDirty(WID_VT_TIMETABLE_PANEL);
		}
	}

	virtual void OnMouseDrag(Point pt, int widget)
	{
		if ((IsDestinationLineSelected() || IsTimetableLineSelected()) && widget == WID_VT_TIMETABLE_PANEL) {
			/* An order is dragged.. */
			VehicleOrderID from_order = this->GetSelectedOrderID();
			VehicleOrderID to_order = this->GetOrderID(this->GetLineFromPt(pt.y));
			uint num_orders = this->vehicle->GetNumOrders();

			if (from_order != INVALID_VEH_ORDER_ID && from_order <= num_orders) {
				if (to_order != INVALID_VEH_ORDER_ID && to_order <= num_orders) { // ..over an existing order.
					this->order_over = to_order;
					this->SetWidgetDirty(widget);
				} else if (from_order != to_order && this->order_over != INVALID_VEH_ORDER_ID) { // ..outside of the order list.
					this->order_over = INVALID_VEH_ORDER_ID;
					this->SetWidgetDirty(widget);
				}
			}
		}
	}

	virtual void OnTimeout()
	{
		static const int raise_widgets[] = {
			WID_VT_SHIFT_ORDERS_PAST_BUTTON, WID_VT_SHIFT_ORDERS_FUTURE_BUTTON, WID_VT_DEPARTURE_BUTTON, WID_VT_START_BUTTON, WID_VT_START_AUTOFILL_DROPDOWN, WID_VT_STOP_AUTOFILL_BUTTON,
			WID_VT_REFIT_BUTTON, WID_VT_REFIT_BUTTON_4,
			WID_VT_OFFSET_BUTTON, WID_VT_SPEEDLIMIT_BUTTON, WID_VT_LENGTH_BUTTON, WID_VT_COND_VALUE_BUTTON, WID_VT_ARRIVAL_BUTTON, WID_VT_RENAME_BUTTON, WID_VT_SKIP_ORDER_BUTTON,
			WID_VT_DELETE_ORDER_BUTTON, WID_VT_STOP_SHARING_BUTTON, WIDGET_LIST_END
		};

		/* Unclick all buttons in raise_widgets[]. */
		for (const int *widnum = raise_widgets; *widnum != WIDGET_LIST_END; widnum++) {
			if (this->IsWidgetLowered(*widnum)) {
				this->RaiseWidget(*widnum);
				this->SetWidgetDirty(*widnum);
			}
		}
	}

	void ShowRenameTimetableWindow()
	{
		this->query_type = TQT_NAME;

		StringID str;
		const OrderList *order_list = this->vehicle->orders.list;
		if (order_list == NULL || order_list->GetName() == NULL) {
			str = STR_EMPTY;
		} else {
			SetDParamStr(0, order_list->GetName());
			str = STR_JUST_RAW_STRING;
		}

		ShowQueryString(str, STR_TIMETABLE_RENAME_CAPTION, MAX_LENGTH_TIMETABLE_NAME_CHARS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT | QSF_LEN_IN_CHARS);
	}
};

static const NWidgetPart _nested_timetable_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_VT_CAPTION), SetDataTip(STR_TIMETABLE_TITLE_NAMED, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_ORDER_VIEW), SetMinimalSize(61, 14), SetDataTip( STR_TIMETABLE_ORDER_VIEW, STR_TIMETABLE_ORDER_VIEW_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_VERTICAL),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_GREY, WID_VT_SUMMARY_PANEL), SetResize(1, 0),
			EndContainer(),
			NWidget(NWID_VERTICAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_SHIFT_BY_LENGTH_PAST_BUTTON), SetResize(0, 0), SetMinimalSize(28, 12),
														SetDataTip(STR_TIMETABLE_SHIFT_BY_LENGTH_PAST_BUTTON, STR_TIMETABLE_SHIFT_BY_LENGTH_PAST_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_SHIFT_BY_LENGTH_FUTURE_BUTTON), SetResize(0, 0), SetMinimalSize(28, 12),
														SetDataTip(STR_TIMETABLE_SHIFT_BY_LENGTH_FUTURE_BUTTON, STR_TIMETABLE_SHIFT_BY_LENGTH_FUTURE_TOOLTIP),
			EndContainer(),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_VT_FULL_FILTER_BUTTON),
					SetResize(0, 0), SetMinimalSize(28, 24), SetDataTip(STR_TIMETABLE_FULL_FILTER_BUTTON, STR_TIMETABLE_FULL_FILTER_TOOLTIP),
			NWidget(NWID_VERTICAL, NC_EQUALSIZE),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_VT_DESTINATION_FILTER_BUTTON),
						SetResize(0, 0), SetMinimalSize(28, 12), SetDataTip(STR_TIMETABLE_DEST_FILTER_BUTTON, STR_TIMETABLE_DEST_FILTER_TOOLTIP),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_VT_TIMETABLE_FILTER_BUTTON),
						SetResize(0, 0), SetMinimalSize(28, 12), SetDataTip(STR_TIMETABLE_TIMETABLE_FILTER_BUTTON, STR_TIMETABLE_TIMETABLE_FILTER_TOOLTIP),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_GREY, WID_VT_TIMETABLE_PANEL), SetResize(1, 10), SetDataTip(STR_NULL, STR_TIMETABLE_TOOLTIP), SetScrollbar(WID_VT_SCROLLBAR), EndContainer(),
			NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_VT_SCROLLBAR),
		EndContainer(),
		NWidget(NWID_VERTICAL, NC_EQUALSIZE),
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_SELECTION, INVALID_COLOUR, WID_VT_TOP_SELECTION),
					NWidget(NWID_HORIZONTAL), // property line
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_START_BUTTON), SetResize(1, 0), SetFill(1, 1),
							SetDataTip(STR_TIMETABLE_START_BUTTON_CAPTION, STR_TIMETABLE_START_BUTTON_TOOLTIP),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_OFFSET_BUTTON), SetFill(1, 0),
							SetDataTip(STR_TIMETABLE_OFFSET_BUTTON_CAPTION, STR_TIMETABLE_OFFSET_BUTTON_TOOLTIP), SetResize(1, 0),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_LENGTH_BUTTON), SetResize(1, 0), SetFill(1, 1),
							SetDataTip(STR_TIMETABLE_LENGTH_BUTTON_CAPTION, STR_TIMETABLE_LENGTH_BUTTON_TOOLTIP),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_SHIFT_ORDERS_PAST_BUTTON), SetResize(1, 0), SetFill(1, 1),
							SetDataTip(STR_TIMETABLE_SHIFT_ORDERS_PAST_BUTTON_CAPTION, STR_TIMETABLE_SHIFT_ORDERS_PAST_BUTTON_TOOLTIP),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_SHIFT_ORDERS_FUTURE_BUTTON), SetFill(1, 0),
							SetDataTip(STR_TIMETABLE_SHIFT_ORDERS_FUTURE_BUTTON_CAPTION, STR_TIMETABLE_SHIFT_ORDERS_FUTURE_BUTTON_TOOLTIP), SetResize(1, 0),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_RENAME_BUTTON), SetResize(1, 0), SetFill(1, 1),
							SetDataTip(STR_TIMETABLE_RENAME_BUTTON_CAPTION, STR_TIMETABLE_RENAME_BUTTON_TOOLTIP),
					EndContainer(),
					NWidget(NWID_HORIZONTAL), // vehicle interval line
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_VT_AUTOFILL_SELECTION),
							NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, WID_VT_START_AUTOFILL_DROPDOWN), SetFill(1, 1),
								SetDataTip(STR_TIMETABLE_START_AUTOFILL_DROPDOWN_CAPTION, STR_TIMETABLE_START_AUTOFILL_DROPDOWN_TOOLTIP), SetResize(1, 0),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_STOP_AUTOFILL_BUTTON), SetResize(1, 0), SetFill(1, 1),
								SetDataTip(STR_TIMETABLE_STOP_AUTOFILL_BUTTON_CAPTION, STR_TIMETABLE_STOP_AUTOFILL_BUTTON_TOOLTIP),
							NWidget(WWT_PANEL, COLOUR_GREY, WID_VT_AUTOFILL_INFO_PANEL), SetResize(1, 0), EndContainer(),
						EndContainer(),
					EndContainer(),				
					NWidget(NWID_HORIZONTAL), // destination line, case conditional order
						NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_VT_COND_VARIABLE_DROPDOWN), SetFill(1, 0),
							SetDataTip(STR_NULL, STR_ORDER_CONDITIONAL_VARIABLE_TOOLTIP), SetResize(1, 0),
						NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_VT_COND_COMPARATOR_DROPDOWN), SetFill(1, 0),
							SetDataTip(STR_NULL, STR_ORDER_CONDITIONAL_COMPARATOR_TOOLTIP), SetResize(1, 0),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_COND_VALUE_BUTTON), SetFill(1, 0),
							SetDataTip(STR_BLACK_COMMA, STR_ORDER_CONDITIONAL_VALUE_TOOLTIP), SetResize(1, 0),
						NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 0), SetResize(1, 0), EndContainer(),
					EndContainer(),
					NWidget(NWID_HORIZONTAL), // destination line, case station
						NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, WID_VT_NON_STOP_STATION_DROPDOWN), SetFill(1, 0),
							SetDataTip(STR_ORDER_NON_STOP, STR_ORDER_TOOLTIP_NON_STOP), SetResize(1, 0),
						NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, WID_VT_FULL_LOAD_DROPDOWN), SetFill(1, 0),
							SetDataTip(STR_ORDER_TOGGLE_FULL_LOAD, STR_ORDER_TOOLTIP_FULL_LOAD), SetResize(1, 0),
						NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, WID_VT_UNLOAD_DROPDOWN), SetFill(1, 0),
							SetDataTip(STR_ORDER_TOGGLE_UNLOAD, STR_ORDER_TOOLTIP_UNLOAD), SetResize(1, 0),
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_VT_REFIT_SELECTION),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_REFIT_BUTTON_4), SetFill(1, 0),
																	SetDataTip(STR_ORDER_REFIT, STR_ORDER_REFIT_TOOLTIP), SetResize(1, 0),
							NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, WID_VT_REFIT_AUTO_DROPDOWN), SetFill(1, 0),
																	SetDataTip(STR_ORDER_REFIT_AUTO, STR_ORDER_REFIT_AUTO_TOOLTIP), SetResize(1, 0),
						EndContainer(),
					EndContainer(),			
					NWidget(NWID_HORIZONTAL), // destination line, case waypoint
						NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, WID_VT_NON_STOP_WAYPOINT_DROPDOWN), SetFill(1, 0),
							SetDataTip(STR_ORDER_NON_STOP, STR_ORDER_TOOLTIP_NON_STOP), SetResize(1, 0),
						NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 0), SetResize(1, 0), EndContainer(),
					EndContainer(),				
					NWidget(NWID_HORIZONTAL), // destination line, case depot
						NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, WID_VT_NON_STOP_DEPOT_DROPDOWN), SetFill(1, 0),
							SetDataTip(STR_ORDER_NON_STOP, STR_ORDER_TOOLTIP_NON_STOP), SetResize(1, 0),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_REFIT_BUTTON), SetFill(1, 0),
							SetDataTip(STR_ORDER_REFIT, STR_ORDER_REFIT_TOOLTIP), SetResize(1, 0),
						NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, WID_VT_SERVICE_DROPDOWN), SetFill(1, 0),
							SetDataTip(STR_ORDER_SERVICE, STR_ORDER_SERVICE_TOOLTIP), SetResize(1, 0),
					EndContainer(),				
					NWidget(NWID_HORIZONTAL), // timetable line
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_ARRIVAL_BUTTON), SetResize(1, 0), SetFill(1, 1),
							SetDataTip(STR_TIMETABLE_ARRIVAL_BUTTON_CAPTION, STR_TIMETABLE_ARRIVAL_BUTTON_TOOLTIP),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_DEPARTURE_BUTTON), SetResize(1, 0), SetFill(1, 1),
							SetDataTip(STR_TIMETABLE_DEPARTURE_BUTTON_CAPTION, STR_TIMETABLE_DEPARTURE_BUTTON_TOOLTIP),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_SPEEDLIMIT_BUTTON), SetResize(1, 0), SetFill(1, 1),
							SetDataTip(STR_TIMETABLE_SPEEDLIMIT_BUTTON_CAPTION, STR_TIMETABLE_SPEEDLIMIT_BUTTON_TOOLTIP),
					EndContainer(),				
					NWidget(NWID_HORIZONTAL), // default line (nothing or end-of-orders selected)
						NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 0), SetResize(1, 0), EndContainer(),
					EndContainer(),
				EndContainer(),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_VT_SHARED_ORDER_LIST), SetFill(0, 1),
					SetDataTip(SPR_SHARED_ORDERS_ICON, STR_ORDERS_VEH_WITH_SHARED_ORDERS_LIST_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_SKIP_ORDER_BUTTON), SetFill(1, 0),
										SetDataTip(STR_ORDERS_SKIP_BUTTON, STR_ORDERS_SKIP_TOOLTIP), SetResize(1, 0),
				NWidget(NWID_SELECTION, INVALID_COLOUR, WID_VT_SELECTION_BOTTOM_2),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_DELETE_ORDER_BUTTON), SetFill(1, 0),
										SetDataTip(STR_ORDERS_DELETE_BUTTON, STR_ORDERS_DELETE_TOOLTIP), SetResize(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VT_STOP_SHARING_BUTTON), SetFill(1, 0),
										SetDataTip(STR_ORDERS_STOP_SHARING_BUTTON, STR_ORDERS_STOP_SHARING_TOOLTIP), SetResize(1, 0),
				EndContainer(),
				NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, WID_VT_GOTO_BUTTON), SetFill(1, 0),
										SetDataTip(STR_ORDERS_GO_TO_BUTTON, STR_ORDERS_GO_TO_TOOLTIP), SetResize(1, 0),
				NWidget(WWT_RESIZEBOX, COLOUR_GREY),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _timetable_desc(
	WDP_AUTO, "view_vehicle_timetable", 400, 130,
	WC_VEHICLE_TIMETABLE, WC_VEHICLE_VIEW,
	WDF_CONSTRUCTION,
	_nested_timetable_widgets, lengthof(_nested_timetable_widgets)
);

/**
 * Show the timetable for a given vehicle.
 * @param v The vehicle to show the timetable for.
 */
void ShowTimetableWindow(const Vehicle *v)
{
	DeleteWindowById(WC_VEHICLE_DETAILS, v->index, false);
	DeleteWindowById(WC_VEHICLE_ORDERS, v->index, false);
	AllocateWindowDescFront<TimetableWindow>(&_timetable_desc, v->index);
}

/*
property_line = Start Offset Length Rename Shift_Back Shift_Forward
vehicle_interval = Autofill 
destination, cond_order = Cond_Variable Comparator Cond_Value ---
destination, station = Non_Stop Full_Load Unload Refit
destination, waypoint = Non_Stop Full_Load Unload Refit
destination, depot = Non_Stop Refit Service --- 
time = Arrival --- Speed_Limit Departure
end = Non_Stop Full_Load Unload Refit
nothing = Non_Stop Full_Load Unload Refit
*/
