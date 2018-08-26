/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file nodetimetable_gui.cpp GUI for displaying timetables for route nodes, e.g. stations, depots etc. */

#include "stdafx.h"

#include "date_func.h"
#include "debug.h"
#include "gfx_func.h"
#include "order_base.h"
#include "date_type.h"
#include "depot_base.h"
#include "station_base.h"
#include "strings_func.h"
#include "timetable_func.h"
#include "vehicle_base.h"
#include "waypoint_base.h"
#include "window_gui.h"

#include <algorithm>
#include <set>
#include <vector>

#include "table/strings.h"

#include "widgets/timetable_widget.h"

#include "safeguards.h"

static const NWidgetPart _nested_nodetimetable_window_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_NTW_CAPTION), SetDataTip(STR_NULL, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_NTW_TIMETABLE_PANEL), SetMinimalSize(300, 300), SetResize(1, 1),
		EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_NTW_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_NTW_MODE_BUTTON), SetFill(1, 0), SetResize(0, 0), SetMinimalSize(80, 12),
				SetDataTip(STR_NODETIMETABLE_ARRIVAL_BUTTON_CAPTION, STR_NODETIMETABLE_ARRIVAL_BUTTON_TOOLTIP),
		NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

static WindowDesc _nodetimetable_window_desc(
	WDP_AUTO, NULL, 100, 138,
	WC_NODETIMETABLE_WINDOW, WC_NONE,
	0,
	_nested_nodetimetable_window_widgets, lengthof(_nested_nodetimetable_window_widgets)
);

struct OrderInfo {
	Order *order;
	OrderList *order_list;

	OrderInfo(Order *order, OrderList *order_list)
	{
		this->order = order;
		this->order_list = order_list;
	}
};

struct LocationTimetableWindow : public Window {

private:
	struct TimetableDestination {
		const Order *order;
		Date date;
		int x1;
		int x2;
		int y;

		TimetableDestination(const Order *order, Date date, int x1, int x2, int y)
		{
			this->order = order;
			this->date = date;
			this->x1 = x1;
			this->x2 = x2;
			this->y = y;
		}
	};

	struct TimetableEntry {
		const Vehicle *vehicle;         ///< The given vehicle
		Order *order;             ///< will pass in its timetable the station/depot/etc. at hand using this order
		Duration offset;

		std::vector<TimetableDestination*> destinations;

		int header_x;
		int header_y;
		int header_date_width;
		int header_line_width;
		int header_dest_width;
		int column;
		int row;
		bool print;

		TimetableEntry(const Vehicle *vehicle, Order *order, Duration offset)
		{
			this->vehicle = vehicle;
			this->order = order;
			this->offset = offset;
			this->destinations = std::vector<TimetableDestination*>();
			this->print = false;
		}

		~TimetableEntry()
		{
			for (std::vector<TimetableDestination*>::iterator it = destinations.begin(); it != destinations.end(); it++) {
				delete *it;
			}
		}

		void AddDestination(const Order *order, Date date, int x1, int x2, int y)
		{
			this->destinations.push_back(new TimetableDestination(order, date, x1, x2, y));
		}

		void SetHeaderLocation(int header_x, int header_y)
		{
			this->header_x = header_x;
			this->header_y = header_y;
		}
	};

	static bool CompareTimetableEntriesDeparture(TimetableEntry *e1, TimetableEntry *e2)
	{
		if (e1->vehicle->orders.list == NULL || !e1->order->HasDeparture()) {
			return true;
		} else if (e2->vehicle->orders.list == NULL || !e2->order->HasDeparture()) {
			return false;
		} else {
			Date start_date_one = AddToDate(e1->order->GetDeparture(), e1->offset);
			Date start_date_two = AddToDate(e2->order->GetDeparture(), e2->offset);
			return start_date_one < start_date_two;
		}
	}

	static bool CompareTimetableEntriesArrival(TimetableEntry *e1, TimetableEntry *e2)
	{
		if (e1->vehicle->orders.list == NULL || !e1->order->HasArrival()) {
			return true;
		} else if (e2->vehicle->orders.list == NULL || !e2->order->HasArrival()) {
			return false;
		} else {
			Date start_date_one = AddToDate(e1->order->GetArrival(), e1->offset);
			Date start_date_two = AddToDate(e2->order->GetArrival(), e2->offset);
			return start_date_one < start_date_two;
		}
	}

	std::vector<TimetableEntry*> timetable_entries;
	bool initialized;
	bool end_of_constructor_reached;

	int number_of_columns;
	int column_width;
	int column_height;

	Scrollbar *vscroll;

	/* The number of months, for which arrivals and departures should be displayed.
	 * Note that if the heuristic algorithm below does not reserve enough space,
	 * then the timetable may be actually somewhat shorter. */
	static const int NUMBER_OF_MONTHS = 30;

	/* The minimal width of the timetable columns. */
	static const int MIN_COLUMN_WIDTH = 300;

	/* The width of the delimiter between two timetable destinations. */
	static const int DELIMITER_WIDTH = 10;

	/* The width spend around the vertical lines between two timetable columns. */
	static const int ENTRY_DELIMITER_WIDTH = 10;

	/* The height spent around the horizontal line between two timetable entries. */
	static const int ENTRY_DELIMITER_HEIGHT = 5;

	/* The minimal offset above the bottom of the timetable (i.e. column_height) necessary
	 * for printing a final horizontal line below the last block. */
	static const int MIN_LAST_BLOCK_LINE_OFFSET = 30;

	/* Height of the header line of a timetable entry. */
	int header_height;

	/* Space between two tokens of the header. */
	static const int HEADER_DELIMITER_WIDTH = 10;

	/* Height of the destination lines */
	int destination_height;

	/* An approximation of the average loss per line, i.e. the space that is wasted
	 * because an order ends before end of line, but the next one cannot be painted
	 * because it does not fit into the remaining space. */
	static const int APPROXIMATE_LOSS_PER_LINE = 20;

	/* The same for columns. */
	static const int APPROXIMATE_LOSS_PER_COLUMN = 30;

	static const int LEFT_MARGIN = WD_FRAMERECT_LEFT;
	static const int RIGHT_MARGIN = WD_FRAMERECT_RIGHT;
	static const int TOP_MARGIN = WD_FRAMERECT_TOP;
	static const int BOTTOM_MARGIN = WD_FRAMERECT_BOTTOM;

	/** Returns the Date of the given Order relevant for the timetable as home date.  E.g. if a vehicle
     *  departs in A at 1, arrives in B at 2 and departs at 3, and arrives in C at 4, and this is the
     *  timetable window for B, then in a arrival timetable it returns 2, and in a departure timetable 3.
	 *  Returns an INVALID_DATE, if the order at hand does not contain the needed timetable information.
	 */
	Date GetHomeDate(const Order *order)
	{
		if (departure_timetable) {
			return (order->HasDeparture() ? order->GetDeparture() : INVALID_DATE);
		} else {
			return (order->HasArrival() ? order->GetArrival() : INVALID_DATE);
		}
	}

	/** Returns the Date of the given Order relevant for the timetable as neighbor date.  E.g. if a vehicle
     *  departs in A at 1, arrives in B at 2 and departs at 3, and arrives in C at 4, and this is the
     *  timetable window for B, then in a arrival timetable it returns 1 for order A, and in a departure
     *  timetable 4 for order C.  I.e. we say "it departs at A at 1 (home date),
     *  arrives in B at 2 (neighbor date), arrives in C at 4 (neighbor date),
     *  and so on.
	 *  Returns an INVALID_DATE, if the order at hand does not contain the needed timetable information.
	 */
	Date GetNeighborDate(const Order *order)
	{
		if (departure_timetable) {
			return (order->HasArrival() ? order->GetArrival() : INVALID_DATE);
		} else {
			return (order->HasDeparture() ? order->GetDeparture() : INVALID_DATE);
		}
	}

	virtual void CalculateTimetableInformation()
	{
		this->CalculateTimetableEntrys();

		/* The total width (including delimiters) of all destination entries of our timetable. */
		int total_width = 0;

		for (std::vector<TimetableEntry*>::iterator it = this->timetable_entries.begin(); it != this->timetable_entries.end(); it++) {
			TimetableEntry *entry = *it;
			const Order *final_destination_order = CalculateFinalDestination(entry);
			int curr_width = ConstructDestinations(entry, final_destination_order);
			if (curr_width > 0) {
				total_width += curr_width;
			}
		}

		NWidgetBase *timetable_panel = this->GetWidget<NWidgetBase>(WID_NTW_TIMETABLE_PANEL);
		int width = timetable_panel->current_x - (LEFT_MARGIN + RIGHT_MARGIN);
		/* Find out how many columns of at least width MIN_COLUMN_WIDTH we have.  E.g. if we have 400 pixels, and MIN_COLUMN_WIDTH is 300 pixels,
		 * we will have one column. */
		if (width >= MIN_COLUMN_WIDTH) {
			this->number_of_columns = width / MIN_COLUMN_WIDTH;
			/* If the total width is not a multiple of MIN_COLUMN_WIDTH, then the resulting column width is bigger than MIN_COLUMN_WIDTH.  Distribute
			 * the extra space among all columns. */
			this->column_width = width / this->number_of_columns;
		} else {
			this->number_of_columns = 1;
			this->column_width = width;
		}
		int approximate_height = CalculateApproximateHeight(total_width);


		/* Set the column height.  On its basis, then the actual timetable placement will be calculated. */
		this->column_height = (approximate_height + this->number_of_columns * APPROXIMATE_LOSS_PER_COLUMN) / this->number_of_columns + TOP_MARGIN + BOTTOM_MARGIN;

		int panel_height = timetable_panel->current_y;

		if (this->column_height < panel_height) {
			this->column_height = panel_height;
		}

		DEBUG(misc, 9, "CalculateTimetableInformation: approx. height = %i, width = %i, number of columns = %i, column_height = %i, panel_height = %i",
					  approximate_height, width, this->number_of_columns, this->column_height, panel_height);

		if (this->departure_timetable) {
			std::sort(this->timetable_entries.begin(), this->timetable_entries.end(), CompareTimetableEntriesDeparture);
		} else {
			std::sort(this->timetable_entries.begin(), this->timetable_entries.end(), CompareTimetableEntriesArrival);
		}

		CalculateStringPlacements();
	}

	virtual void CalculateTimetableEntrys()
	{
		for (std::vector<TimetableEntry*>::iterator it = this->timetable_entries.begin(); it != this->timetable_entries.end(); it++) {
			delete *it;
		}

		this->timetable_entries.clear();

		/*  We start with all orders, that have as destination the destination of this timetable window, e.g.
         *  some particular station. */
		std::vector<OrderInfo> order_infos = this->GetAffectedOrders();

		/*  Define arrivals / departures in which range the timetable should contain.
         *  We might offer the player the possibility to set that setting, for now it is a constant.
		 */
		Duration timetable_window_duration = Duration(NUMBER_OF_MONTHS, DU_MONTHS);
		Date min_date = _date;
		Date max_date = AddToDate(_date, timetable_window_duration);

		/* Now, inspect each affected order.  As vehicles iterate through their timetable, and thus
		 * process the same order multiple times, which offsets added to the scheduled arrivals / departures,
		 * the goal here is finding those iterations where the particular order is processed within the
		 * interval defined above. */
		for (std::vector<OrderInfo>::const_iterator it = order_infos.begin(); it != order_infos.end(); it++) {
			OrderInfo order_info = *it;

			/* First, find out the pure arrival/departure as defined in the timetable itself, without any vehicle offset. */
			Date uncorrected_home_date = GetHomeDate(order_info.order);
			if (uncorrected_home_date != INVALID_DATE && order_info.order_list->HasStartTime()) {
				/* If it is set at all, iterate over all vehicles, as they will in generally have different offsets. */
				for (const Vehicle *vehicle = order_info.order_list->GetFirstSharedVehicle(); vehicle != NULL; vehicle = vehicle->NextShared()) {
					if (!vehicle->timetable_offset.IsInvalid()) {
						Duration curr_offset = vehicle->timetable_offset;

						/* Apply vehicle offset, and lateness counter, and iterate into the future and into the past, until
						 * the boundaries of the time range defined above are reached; register any occurrence inside that
						 * time range.
						 * Regarding lateness, an arrival or departure is regarded as being inside the range if it inside
						 * either with or without lateness.  E.g. if we have May 3rd, and a vehicle that was supposed to
						 * arrive at May 1st will in reality arrive at May 5th, we display it, and will do so also
						 * for another vehicle supposed to arrive at May 8th that is too early and in fact already
						 * arrived at May 2nd.
						 * With respect to this, note that the lateness_counter is negative if a vehicle is too early. */
						Date home_date = AddToDate(uncorrected_home_date, vehicle->timetable_offset);
						if ((home_date >= min_date || home_date + vehicle->lateness_counter >= min_date)
						  && (home_date <= max_date || home_date + vehicle->lateness_counter <= max_date)) {
							this->timetable_entries.push_back(new TimetableEntry(vehicle, order_info.order, curr_offset));
						}
						Date curr_home_date = home_date;
						do {
							curr_home_date = SubtractFromDate(curr_home_date, order_info.order_list->GetTimetableDuration());
							curr_offset.Subtract(order_info.order_list->GetTimetableDuration());
							if ((curr_home_date >= min_date || curr_home_date + vehicle->lateness_counter >= min_date)
							  && (curr_home_date <= max_date || curr_home_date + vehicle->lateness_counter <= max_date)) {
								this->timetable_entries.push_back(new TimetableEntry(vehicle, order_info.order, curr_offset));
							}
						} while (curr_home_date >= min_date || curr_home_date + vehicle->lateness_counter >= min_date);

						curr_home_date = home_date;
						curr_offset = vehicle->timetable_offset;
						do {
							curr_home_date = AddToDate(curr_home_date, order_info.order_list->GetTimetableDuration());
							curr_offset.Add(order_info.order_list->GetTimetableDuration());
							if ((curr_home_date >= min_date || curr_home_date + vehicle->lateness_counter >= min_date)
							  && (curr_home_date <= max_date || curr_home_date + vehicle->lateness_counter <= max_date)) {
								this->timetable_entries.push_back(new TimetableEntry(vehicle, order_info.order, curr_offset));
							}
						} while (curr_home_date <= max_date || curr_home_date + vehicle->lateness_counter <= max_date);
					}
				}
			}
		}
	}

	const Order* CalculateFinalDestination(TimetableEntry *timetable_entry)
	{
		const Vehicle *vehicle = timetable_entry->vehicle;

		/* Heuristic for departure tables: Search forward through the order list, restart at the beginning if necessary, and
		 * stop if a destination would be seen for the second time.
		 * Heuristic for arrival tables: The other way round, search backward. */

		/* Determine the VehicleOrderID of the order stored in the timetable_entry. */
  		VehicleOrderID order_index = INVALID_VEH_ORDER_ID;
		for (VehicleOrderID curr_order_index = 0; curr_order_index < vehicle->orders.list->GetNumOrders(); curr_order_index++) {
			const Order *curr_order = vehicle->orders.list->GetOrderAt(curr_order_index);
			if (curr_order == timetable_entry->order) {
				order_index = curr_order_index;
				break;
			}
		}

		if (order_index == INVALID_VEH_ORDER_ID) {
			/* Default case, if order is not found at all: Return the last / first order */
			return this->departure_timetable ? vehicle->GetLastOrder() : vehicle->GetFirstOrder();
		} else {
			/* We only record the ids of stations the train actually stops here.
			 * If we would record all, we would have to do it in a more sophisticated way, as station and depot ids are not disjunct */
			std::set<StationID> destinations_already_seen;
			int delta = this->departure_timetable ? +1 : -1;

			/* The previous order in terms of searching.  In term of order list, its the previous (departure tables) or the next (arrival tables) order. */
			const Order* prev_order = timetable_entry->order;

			/* Departure timetables: Step forward to the end.  Arrival timetables: Step backwards to the beginning */
			for (int c = order_index + delta; (this->departure_timetable && c < vehicle->orders.list->GetNumOrders())
										   || (!this->departure_timetable && c >= 0); c += delta) {
				VehicleOrderID curr_order_index = (VehicleOrderID)c;    //< We cannot use curr_order_index directly as loop variable, as it is unsigned, which breaks the ">= 0" test
				const Order *curr_order = vehicle->orders.list->GetOrderAt(curr_order_index);
				if (destinations_already_seen.find(curr_order->GetDestination()) == destinations_already_seen.end()) {
					if (curr_order->IsStationOrder() && (curr_order->GetLoadType() != OLFB_NO_LOAD || curr_order->GetUnloadType() != OUFB_NO_UNLOAD)) {
						/* Ignore non-station-orders, as most probably, a senseful destination for a timetable will never be
						   the depot or the waypoint right after the final destination. */
						destinations_already_seen.insert(curr_order->GetDestination());
						prev_order = curr_order;
					}
				} else {
					return prev_order;
				}
			}

			/* Departure timetables: Step forward until we are again at the start index.  Arrival timetables: Do the same backwards */
			for (VehicleOrderID curr_order_index = this->departure_timetable ? 0 : vehicle->orders.list->GetNumOrders() - 1; curr_order_index != order_index; curr_order_index += delta) {
				const Order *curr_order = vehicle->orders.list->GetOrderAt(curr_order_index);
				if (destinations_already_seen.find(curr_order->GetDestination()) == destinations_already_seen.end()) {
					if (curr_order->IsStationOrder() && (curr_order->GetLoadType() != OLFB_NO_LOAD || curr_order->GetUnloadType() != OUFB_NO_UNLOAD)) {
						/* Ignore non-station-orders, as most probably, a senseful destination for a timetable will never be
						   the depot or the waypoint right after the final destination. */
						destinations_already_seen.insert(curr_order->GetDestination());
						prev_order = curr_order;
					}
				} else {
					return prev_order;
				}
			}

			return prev_order;
		}
	}

	Date GetDestinationOrderDate(const Order *order)
	{
		/* Note: This is no bug.  In case of a departure timetable,
		 * we are interested in the arrival dates, and vice versa. */
		if (this->departure_timetable) {
			if (order->HasArrival()) {
				return order->GetArrival();
			} else {
				return INVALID_DATE;
			}
		} else {
			if (order->HasDeparture()) {
				return order->GetDeparture();
			} else {
				return INVALID_DATE;
			}
		}
	}

	void PrepareForHeaderDate(TimetableEntry *entry) const
	{
		Duration offset = entry->offset;
		if (departure_timetable) {
			if (entry->order->HasDeparture()) {
				SetDParam(0, AddToDate(entry->order->GetDeparture(), offset));
			}
		} else {
			if (entry->order->HasArrival()) {
				SetDParam(0, AddToDate(entry->order->GetArrival(), offset));
			}
		}
	}

	StringID PrepareForHeaderDestination(VehicleType vehicle_type, const Order *order) const
	{
		if (order->IsWaypointOrder()) {
			SetDParam(0, order->GetDestination());
			return STR_BIG_WAYPOINT_NAME;
		} else if (order->IsDepotOrder()) {
			SetDParam(0, vehicle_type);
			SetDParam(1, order->GetDestination());
			return STR_BIG_DEPOT_NAME;
		} else if (order->IsStationOrder()) {
			SetDParam(0, order->GetDestination());
			return STR_BIG_STATION_NAME;
		} else {
			return STR_EMPTY;
		}
	}

	StringID PrepareForDestinationString(VehicleType vehicle_type, const Order *order, Date date) const
	{
		SetDParam(0, date);
		if (order->IsWaypointOrder()) {
			SetDParam(1, order->GetDestination());
			return STR_NODETIMETABLE_WAYPOINT_DEST_DATE;
		} else if (order->IsDepotOrder()) {
			SetDParam(1, vehicle_type);
			SetDParam(2, order->GetDestination());
			return STR_NODETIMETABLE_DEPOT_DEST_DATE;
		} else if (order->IsStationOrder()) {
			SetDParam(1, order->GetDestination());
			return STR_NODETIMETABLE_STATION_DEST_DATE;
		} else {
			return STR_EMPTY;
		}
	}

	int ConstructDestinations(TimetableEntry *timetable_entry, const Order* final_destination)
	{
		const Vehicle *vehicle = timetable_entry->vehicle;
		const Order *order = timetable_entry->order;

		/* First find out the current order index.  As the Order has no previous pointer, we need it at least in the case of an
		 * arrival timetable where we step backwards. */
		VehicleOrderID order_index = INVALID_VEH_ORDER_ID;
		for (VehicleOrderID curr_order_index = 0; curr_order_index < vehicle->orders.list->GetNumOrders(); curr_order_index++) {
			const Order *curr_order = vehicle->orders.list->GetOrderAt(curr_order_index);
			if (curr_order == order) {
				order_index = curr_order_index;
				break;
			}
		}

		if (order_index == INVALID_VEH_ORDER_ID) {
			return -1;
		}

		int total_width = 0;
		Duration shift_offset = Duration();

		/* Now step through the timetable, until the final destination is reached. For each of those destinations,
		 * record an TimetableDestination entry. */
		const Order *curr_order;
		do {
			if (this->departure_timetable) {
				if (order_index < vehicle->orders.list->GetNumOrders() - 1) {
					order_index++;
				} else {
					order_index = 0;
					shift_offset = vehicle->orders.list->GetTimetableDuration();
				}
			} else {
				if (order_index > 0) {
					order_index--;
				} else {
					order_index = vehicle->orders.list->GetNumOrders() - 1;
					shift_offset = -vehicle->orders.list->GetTimetableDuration();
				}
			}

			curr_order = vehicle->orders.list->GetOrderAt(order_index);

			Date date = AddToDate(GetDestinationOrderDate(curr_order), timetable_entry->offset);
			if (!shift_offset.IsInvalid()) {
				date = AddToDate(date, shift_offset);
			}
			StringID s = PrepareForDestinationString(vehicle->type, curr_order, date);
			Dimension d = GetStringBoundingBox(s);
			int x1 = 0;
			int x2 = d.width;
			int y = d.height;

			total_width += d.width + DELIMITER_WIDTH;

			/* Note that in this step, we just record the size of each destination string individually.
			 * In a subsequent step, we plan string placement on a global, window wide, level. */
			if (curr_order->IsStationOrder() && curr_order->GetNonStopType() != ONSF_NO_STOP_AT_ANY_STATION) {
				timetable_entry->AddDestination(curr_order, date, x1, x2, y);
			}
		} while (curr_order != order && curr_order != final_destination);

		return total_width;
	}

	/*  Based on the total width (ignoring the space being lost for not optimally placed line breaks) of
	 *  all destinations, this function makes an approximation of the total timetable height.
	 *  Based on this height, and the number of columns of the timetable, a timetable height will be
	 *  calculated heuristically.
	 */
	int CalculateApproximateHeight(int total_width)
	{
		int number_of_entries = this->timetable_entries.size();
		int total_header_height = number_of_entries * this->header_height;
		int number_of_lines = total_width / (this->column_width - APPROXIMATE_LOSS_PER_LINE);

		int total_destination_height = number_of_lines * this->destination_height;
		DEBUG(misc, 9, "CalculateApproximateHeight: number of entries = %i, total_header_height = %i, number_of_lines = %i, total_dest_height = %i",
						  number_of_entries, total_header_height, number_of_lines, total_destination_height);
		return total_header_height + total_destination_height;
	}

	/*  Calculates the string placements for the given entry, starting at the given position.
	 *  Note that this function does not take a possible end of column into account, it just
	 *  returns the y value of the last used line (there is one exception from that rule: If
	 *  we process the first entry of a column, we just stop at its end, since then shifting
	 *  the destinations into the next column would not help either...).
	 *
	 *  It is the responsibility of the calling
	 *  function to detect a end of column, and shift values into the next column.
	 *  Furthermore note, that calling this function a second time is not possible, since
	 *  it overwrites the x values in the TimetableDestinations, i.e. in that situation you
	 *  must just shift the y values and keep the x values.
	 */
	int CalculateStringPlacements(TimetableEntry *entry, int y, int min_x, int max_x, bool start_of_column)
	{
		PrepareForHeaderDate(entry);
		Dimension header_date_bounding_box = GetStringBoundingBox(STR_NODETIMETABLE_HEADER_DATE);
		entry->header_date_width = header_date_bounding_box.width;

		Dimension header_vehicle_bounding_box;
		if (entry->vehicle->orders.list->GetName() == NULL) {
			SetDParam(0, entry->vehicle->index);
			header_vehicle_bounding_box = GetStringBoundingBox(STR_NODETIMETABLE_HEADER_VEHICLE);
		} else {
			SetDParamStr(0, entry->vehicle->orders.list->GetName());
			header_vehicle_bounding_box = GetStringBoundingBox(STR_NODETIMETABLE_HEADER_TIMETABLE_NAME);
		}
		entry->header_line_width = header_vehicle_bounding_box.width;

		if (entry->destinations.size() > 0) {
			StringID str = PrepareForHeaderDestination(entry->vehicle->type, entry->destinations.at(entry->destinations.size() - 1)->order);
			Dimension header_dest_bounding_box = GetStringBoundingBox(str);
			entry->header_dest_width = header_dest_bounding_box.width;
		}

		int x = min_x;
		y += ENTRY_DELIMITER_HEIGHT / 2;
		entry->SetHeaderLocation(x, y);

		DEBUG(misc, 9, "Setting header pos (%i, %i)", x, y);

		y += this->header_height;

		for (std::vector<TimetableDestination*>::iterator it = entry->destinations.begin(); it != entry->destinations.end(); it++) {
			TimetableDestination *destination = *it;
			int dest_width = destination->x2;

			/* If we just place the first destination of the line, place it whatever width it has.
		     * Reason: It won´t help shifting it into the next line, since that will not be taller either...
			 * Otherwise, if the string fits into the line, use the previously calculated string width, which was stored in x2.
			 * If the string doesn´t fit, use the next line. */
			if (x > min_x && x + dest_width > max_x) {
				x = min_x;
				y += this->destination_height;

				/*
				if (start_of_column && y > this->column_height) {
					return this->column_height;
				}
				*/
			}

			destination->x1 = x;
			if (x + dest_width > max_x) {
				destination->x2 = max_x;
			} else {
				destination->x2 = x + dest_width;
			}
			destination->y = y;
			x += dest_width;
			x += DELIMITER_WIDTH;

			DEBUG(misc, 9, "Setting dest pos %i %i %i", destination->x1, destination->x2, destination->y);
		}

		y += this->destination_height;

		y += ENTRY_DELIMITER_HEIGHT / 2;

		return y;
	}

	void ShiftToNextColumn(TimetableEntry *entry, int y_offset)
	{
		entry->header_x += this->column_width;
		entry->header_y -= y_offset;
		entry->column++;
		entry->row = 0;

		for (std::vector<TimetableDestination*>::iterator it = entry->destinations.begin(); it != entry->destinations.end(); it++) {
			TimetableDestination *destination = *it;
			destination->x1 += this->column_width;
			destination->x2 += this->column_width;
			destination->y -= y_offset;
		}
	}

	void CalculateStringPlacements()
	{
		int min_x = ENTRY_DELIMITER_WIDTH / 2;
		int max_x = this->column_width - ENTRY_DELIMITER_WIDTH;

		int y = 0;

		int curr_column = 0;
		int curr_row = 0;
		bool start_of_column = true;

		for (std::vector<TimetableEntry*>::iterator it = this->timetable_entries.begin(); it != this->timetable_entries.end(); it++) {
			TimetableEntry *entry = *it;

			int start_y = y;

			/* Calculate the string placements.  If they exceed the lower end of the column, we afterwards shift them to the next column.
			 * If we reached the end of the last column, we abort since there is no more space left. */
			y = CalculateStringPlacements(entry, y, min_x, max_x, start_of_column);
			entry->column = curr_column;
			entry->row = curr_row;

			/* We exceeded the column height.  Note that if CalculateStringPlacements filled the complete column with one entry,
			 * it returns this->column_height, which does not count as exceeding.  Thus in this case, we do not shift, which
			 * is absolutely correct since CalculateStringPlacements stopped before exceeding anyway. */
			if (y > this->column_height) {
				/* Test wether there is a column left. */
				if (curr_column >= this->number_of_columns - 1) {
					return;
				}
				ShiftToNextColumn(entry, start_y);

				/* Proceed to next column.  Since we just shifted the entry to that column, and started at zero instead of
				 * start_y, we have to subtract start_y
				 */
				y = y - start_y;
				curr_column++;
				curr_row = 1;  // ShiftToNextColumn already produced the topmost entry of that column, thus start with index 1
				min_x += this->column_width;
				max_x += this->column_width;
			} else {
				curr_row++;
			}

			/* The entry may be printed. */
			entry->print = true;
		}
	}

protected:
	bool departure_timetable;

	static const uint DEFAULT_WIDTH = 400;

public:
	LocationTimetableWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->end_of_constructor_reached = false;

		this->CreateNestedTree();
		this->FinishInitNested(window_number);

		this->window_number = window_number;
		this->departure_timetable = (window_number < 0x100000);

		NWidgetCore *widget = this->GetWidget<NWidgetCore>(WID_NTW_MODE_BUTTON);
		widget->widget_data = this->departure_timetable ? STR_NODETIMETABLE_ARRIVAL_BUTTON_CAPTION : STR_NODETIMETABLE_DEPARTURE_BUTTON_CAPTION;
		widget->tool_tip  = this->departure_timetable ? STR_NODETIMETABLE_ARRIVAL_BUTTON_TOOLTIP : STR_NODETIMETABLE_DEPARTURE_BUTTON_TOOLTIP;

		this->timetable_entries = std::vector<TimetableEntry*>();

		this->departure_timetable = (window_number < 0x100000);

		this->vscroll = this->GetScrollbar(WID_NTW_SCROLLBAR);

		/* Initialize as fast as possible, and do not wait for the next day (when InvalidateData will be called anyway).
		 * However, we cannot call InvalidateData here, since a call to a virtual function is triggered, and this
		 * call will not work from the constructor due to C++ language design. */
		this->initialized = false;

		/* Test some arbitrary string which has BIG_FONT, to get an approximation of the height of a BIG_FONT line */
		Dimension d = GetStringBoundingBox(STR_NEWS_EXCLUSIVE_RIGHTS_TITLE);
		this->header_height = d.height;
		d = GetStringBoundingBox(STR_NODETIMETABLE_ARRIVAL_BUTTON_CAPTION);
		this->destination_height = d.height;

		DEBUG(misc, 0, "Calculated header_height = %i, destination_height = %i", this->header_height, this->destination_height);

		this->end_of_constructor_reached = true;
	}

	~LocationTimetableWindow()
	{
		for (std::vector<TimetableEntry*>::iterator it = this->timetable_entries.begin(); it != this->timetable_entries.end(); it++) {
			delete *it;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_NTW_MODE_BUTTON: {
				if (this->departure_timetable) {
					this->ShowCorrespondingArrivalTimetable();
				} else {
					this->ShowCorrespondingDepartureTimetable();
				}
			}
		}
	}

	virtual void ShowCorrespondingArrivalTimetable() = 0;
	virtual void ShowCorrespondingDepartureTimetable() = 0;

	virtual std::vector<OrderInfo> GetAffectedOrders() = 0;

	virtual void OnTick() {
		if (!this->initialized) {
			this->initialized = true;
			InvalidateData();
		} else {
			if (_date_fract == 0) {
				InvalidateData();
			}
		}
	}

	virtual void OnInvalidateData(int data, bool gui_scope)
	{
		/* FinishInitNested triggers OnResize.  Problem: When called from the constructor,
		 * the virtual function call does not work.  Thus we must disable that particular
		 * call chain, we invalidate in the first call of OnTick. */
		if (!end_of_constructor_reached) {
			return;
		}

		this->CalculateTimetableInformation();

		/* Unfortunately, via OnResize this function can be called before this->vscroll is initialized in the constructor. */
		if (this->vscroll == NULL) {
			return;
		}

		NWidgetBase *timetable_panel = this->GetWidget<NWidgetBase>(WID_NTW_TIMETABLE_PANEL);
		int height = timetable_panel->current_y;

		this->vscroll->SetCount(this->column_height);
		this->vscroll->SetCapacity(height);
	}

	virtual void OnResize()
	{
		this->InvalidateData();
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		DEBUG(misc, 9, "DrawWidget called for widget %i, TIME = %i, r = (%i, %i, %i, %i)", widget, WID_NTW_TIMETABLE_PANEL, r.left, r.top, r.right, r.bottom);
		switch (widget) {
			case WID_NTW_TIMETABLE_PANEL: {
				DrawPixelInfo tmp_dpi;
				if (!FillDrawPixelInfo(&tmp_dpi, r.left, r.top, r.right, r.bottom)) {
					break;
				}
				DrawPixelInfo *old_dpi = _cur_dpi;
				_cur_dpi = &tmp_dpi;

				int y_offset = vscroll->GetPosition();

				/* y coordinate of the last string being painted.  Used for painting a final horizontal line below the last block.
				 * Initialize in a way that it won´t trigger any action below, in the unprobable case it will not be overwritten.  */
				int last_y = column_height;
				int last_column = 0;

				for (std::vector<TimetableEntry*>::const_iterator it = this->timetable_entries.begin(); it != this->timetable_entries.end(); it++) {
					TimetableEntry *entry = *it;

					if (!entry->print) {
						continue;
					}

					if (entry->row > 0) {
						int y = entry->header_y - ENTRY_DELIMITER_HEIGHT / 2 - y_offset;
						GfxDrawLine(entry->column * this->column_width, y, (entry->column + 1) * this->column_width, y, PC_BLACK);
					}

					Duration offset = entry->offset;
					if (departure_timetable) {
						if (entry->order->HasDeparture()) {
							SetDParam(0, AddToDate(entry->order->GetDeparture(), offset));
						} else {
							continue;
						}
					} else {
						if (entry->order->HasArrival()) {
							SetDParam(0, AddToDate(entry->order->GetArrival(), offset));
						} else {
							continue;
						}
					}
					SetDParam(1, entry->vehicle->index);

					DEBUG(misc, 9, "Drawing header to top = %i, header_y = %i, y_offset = %i", r.top, entry->header_y, y_offset);

					int header_x = entry->header_x;
					if (header_x + entry->header_date_width < entry->header_x + this->column_width) {
						PrepareForHeaderDate(entry);
						DrawString(header_x, header_x + entry->header_date_width, entry->header_y - y_offset, STR_NODETIMETABLE_HEADER_DATE, TC_BLACK);
					}
					header_x += entry->header_date_width + HEADER_DELIMITER_WIDTH;

					if (header_x + entry->header_line_width < entry->header_x + this->column_width) {
						if (entry->vehicle->orders.list->GetName() == NULL) {
							SetDParam(0, entry->vehicle->index);
							DrawString(header_x, header_x + entry->header_line_width, entry->header_y - y_offset, STR_NODETIMETABLE_HEADER_VEHICLE, TC_BLACK);
						} else {
							SetDParamStr(0, entry->vehicle->orders.list->GetName());
							DrawString(header_x, header_x + entry->header_line_width, entry->header_y - y_offset, STR_NODETIMETABLE_HEADER_TIMETABLE_NAME, TC_BLACK);
						}
					}
					header_x += entry->header_line_width + HEADER_DELIMITER_WIDTH;

					if (header_x + entry->header_dest_width < entry->header_x + this->column_width && entry->destinations.size() > 0) {
						StringID str = PrepareForHeaderDestination(entry->vehicle->type, entry->destinations.at(entry->destinations.size() - 1)->order);
						DrawString(header_x, header_x + entry->header_dest_width, entry->header_y - y_offset, str, TC_BLACK);
					}

					last_y = entry->header_y - y_offset;
					last_column = entry->column;

					const std::vector<TimetableDestination*> destinations = entry->destinations;
					for (std::vector<TimetableDestination*>::const_iterator it = entry->destinations.begin(); it != entry->destinations.end(); it++) {
						const TimetableDestination *destination = *it;
						StringID str = PrepareForDestinationString(entry->vehicle->type, destination->order, destination->date);
						DrawString(destination->x1, destination->x2, destination->y - y_offset, str, TC_BLACK);
						last_y = destination->y - y_offset;
					}
				}

				int last_column_line_y = r.bottom - r.top;

				/* Paint a final line below the last block, if it is not just above the end of the column anyway. */
				if (last_y < column_height - MIN_LAST_BLOCK_LINE_OFFSET) {
					last_column_line_y = last_y + this->destination_height + ENTRY_DELIMITER_HEIGHT / 2;
					GfxDrawLine(last_column * this->column_width, last_column_line_y, (last_column + 1) * this->column_width, last_column_line_y, PC_BLACK);
				}

				/* Paint the vertical lines, but only until the last entry of the last column. */
				for (int column = 1; column < this->number_of_columns; column++) {
					if (column - 1 < last_column) {
						GfxDrawLine(column * this->column_width, 0, column * this->column_width, r.bottom - r.top, PC_BLACK);
					} else if (column - 1 == last_column) {
						GfxDrawLine(column * this->column_width, 0, column * this->column_width, last_column_line_y, PC_BLACK);
					}
				}

				_cur_dpi = old_dpi;
				break;
			}
		 }
	  }
};

struct StationTimetableWindow : public LocationTimetableWindow {
private:
	StationID station_id;
	bool is_waypoint;

public:
	StationTimetableWindow(WindowDesc *desc, WindowNumber window_number, StationID station_id, bool is_waypoint) : LocationTimetableWindow(desc, window_number)
	{
		this->station_id = station_id;
		this->is_waypoint = is_waypoint;
		if (this->is_waypoint) {
			this->GetWidget<NWidgetCore>(WID_NTW_CAPTION)->widget_data = this->departure_timetable ? STR_NODETIMETABLE_WAYPOINT_DEPARTURE_CAPTION : STR_NODETIMETABLE_WAYPOINT_ARRIVAL_CAPTION;
		} else {
			this->GetWidget<NWidgetCore>(WID_NTW_CAPTION)->widget_data = this->departure_timetable ? STR_NODETIMETABLE_STATION_DEPARTURE_CAPTION : STR_NODETIMETABLE_STATION_ARRIVAL_CAPTION;
		}
		this->ReInit(0, 0);
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_NTW_CAPTION: {
				SetDParam(0, this->station_id);
				break;
			}
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case WID_NTW_CAPTION: {
				StringID str = this->PrepareCaptionParameters();
				Dimension d = GetStringBoundingBox(str);
				size->width = std::max(d.width + 25, static_cast<uint>(this->DEFAULT_WIDTH));
				DEBUG(misc, 0, "default_width: %i", size->width);
				break;
			}
		}
	}

	StringID PrepareCaptionParameters()
	{
		SetDParam(0, this->station_id);
		if (this->is_waypoint) {
			return this->departure_timetable ? STR_NODETIMETABLE_WAYPOINT_DEPARTURE_CAPTION : STR_NODETIMETABLE_WAYPOINT_ARRIVAL_CAPTION;
		} else {
			return this->departure_timetable ? STR_NODETIMETABLE_STATION_DEPARTURE_CAPTION : STR_NODETIMETABLE_STATION_ARRIVAL_CAPTION;
		}
	}

	virtual void ShowCorrespondingArrivalTimetable()
	{
		if (this->is_waypoint) {
			ShowWaypointTimetableWindow(this->station_id, false, true);
		} else {
			ShowStationTimetableWindow(this->station_id, false, true);
		}
	}

	virtual void ShowCorrespondingDepartureTimetable()
	{
		if (this->is_waypoint) {
			ShowWaypointTimetableWindow(this->station_id, true, false);
		} else {
			ShowStationTimetableWindow(this->station_id, true, false);
		}
	}

	virtual std::vector<OrderInfo> GetAffectedOrders()
	{
		std::vector<OrderInfo> found_order_infos = std::vector<OrderInfo>();

		for (OrderList *order_list : OrderList::Iterate()) {
			Order *order = order_list->GetFirstOrder();
			while (order != NULL) {
				if ((order->IsStationOrder() || order->IsWaypointOrder()) && order->GetDestination() == this->station_id) {
					found_order_infos.push_back(OrderInfo(order, order_list));
				}

				order = order->next;
			}
		}

		return found_order_infos;
	}

	static int GetStationDepartureTimetableWindowNumber(StationID station_id)
	{
		return 0x10000 + station_id;
	}

	static int GetStationArrivalTimetableWindowNumber(StationID station_id)
	{
		return 0x110000 + station_id;
	}
};

struct DepotTimetableWindow : public LocationTimetableWindow {
private:
	DepotID depot_id;
	VehicleType vehicle_type;

public:
	DepotTimetableWindow(WindowDesc *desc, WindowNumber window_number, DepotID depot_id, VehicleType vehicle_type) : LocationTimetableWindow(desc, window_number)
	{
		this->depot_id = depot_id;
		this->vehicle_type = vehicle_type;
		this->GetWidget<NWidgetCore>(WID_NTW_CAPTION)->widget_data = this->departure_timetable ? STR_NODETIMETABLE_DEPOT_DEPARTURE_CAPTION : STR_NODETIMETABLE_DEPOT_ARRIVAL_CAPTION;

		this->ReInit(0, 0);
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_NTW_CAPTION: {
				SetDParam(0, this->vehicle_type);
				SetDParam(1, this->depot_id);
				break;
			}
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case WID_NTW_CAPTION: {
				StringID str = this->PrepareCaptionParameters();
				Dimension d = GetStringBoundingBox(str);
				size->width = std::max(d.width + 25, static_cast<uint>(this->DEFAULT_WIDTH));
				break;
			}
		}
	}

	StringID PrepareCaptionParameters()
	{
		SetDParam(0, this->vehicle_type);
		SetDParam(1, this->depot_id);
		return this->departure_timetable ? STR_NODETIMETABLE_DEPOT_DEPARTURE_CAPTION : STR_NODETIMETABLE_DEPOT_ARRIVAL_CAPTION;
	}

	virtual void ShowCorrespondingArrivalTimetable()
	{
		ShowDepotTimetableWindow(this->depot_id, false, true, this->vehicle_type);
	}

	virtual void ShowCorrespondingDepartureTimetable()
	{
		ShowDepotTimetableWindow(this->depot_id, true, false, this->vehicle_type);
	}

	virtual std::vector<OrderInfo> GetAffectedOrders()
	{
		std::vector<OrderInfo> found_order_infos = std::vector<OrderInfo>();

		for (OrderList *order_list : OrderList::Iterate()) {
			Order *order = order_list->GetFirstOrder();
			while (order != NULL) {
				if (order->IsDepotOrder() && order->GetDestination() == this->depot_id) {
					found_order_infos.push_back(OrderInfo(order, order_list));
				}

				order = order->next;
			}
		}

		return found_order_infos;
	}

	static int GetDepotDepartureTimetableWindowNumber(DepotID depot_id)
	{
		return 0x20000 + depot_id;
	}

	static int GetDepotArrivalTimetableWindowNumber(DepotID depot_id)
	{
		/* RouteNodeIDs and StationIDs are both 16 bit values, start after them */
		return 0x120000 + depot_id;
	}
};

/* ====================================================================
 * The encoding of the window_numbers is as follows:

 *  0x10000 to  0x1FFFF: Station/Waypoint Departure Timetable
 *  0x20000 to  0x2FFFF: Depot Departure Timetable
 * 0x110000 to 0x11FFFF: Station/Waypoint Arrival Timetable
 * 0x120000 to 0x12FFFF: Depot Arrival Timetable
 *
 * [ 0x0000 to   0xFFFF: Node Departure Timetable
 *   0x100000 to 0x10FFFF: Node Arrival Timetable
 *   --- reserved for routenode-centric timetables.
 *       Route nodes are not subject of this patch queue,
 *       extended version of the patch queue. ]
 *
 * By encoding things this way, it is possible to use the same Window, with
 * just relatively small adjustments in the subclasses (see above) for
 * all the different ids.  Note that nodes have RouteNodeIDs, stations and
 * waypoints have StationIDs and depots have DepotIDs, thus we need
 * different ranges for them.  But luckily, ids have 16 bit and window numbers
 * 32 bit, thus this is no problem.
 */

void ShowStationTimetableWindow(StationID station_id, bool departure, bool arrival)
{
	if (!Station::IsValidID(station_id)) return;

	if (departure) {
		int window_number = StationTimetableWindow::GetStationDepartureTimetableWindowNumber(station_id);
		if (!BringWindowToFrontById(WC_NODETIMETABLE_WINDOW, window_number)) {
			new StationTimetableWindow(&_nodetimetable_window_desc, window_number, station_id, false);
		}
	}
	if (arrival) {
		int window_number = StationTimetableWindow::GetStationArrivalTimetableWindowNumber(station_id);
		if (!BringWindowToFrontById(WC_NODETIMETABLE_WINDOW, window_number)) {
			new StationTimetableWindow(&_nodetimetable_window_desc, window_number, station_id, false);
		}
	}
}

void ShowWaypointTimetableWindow(StationID station_id, bool departure, bool arrival)
{
	if (!Waypoint::IsValidID(station_id)) return;

	if (departure) {
		int window_number = StationTimetableWindow::GetStationDepartureTimetableWindowNumber(station_id);
		if (!BringWindowToFrontById(WC_NODETIMETABLE_WINDOW, window_number)) {
			new StationTimetableWindow(&_nodetimetable_window_desc, window_number, station_id, true);
		}
	}
	if (arrival) {
		int window_number = StationTimetableWindow::GetStationArrivalTimetableWindowNumber(station_id);
		if (!BringWindowToFrontById(WC_NODETIMETABLE_WINDOW, window_number)) {
			new StationTimetableWindow(&_nodetimetable_window_desc, window_number, station_id, true);
		}
	}
}

void ShowDepotTimetableWindow(DepotID depot_id, bool departure, bool arrival, VehicleType vehicle_type)
{
	if (!Depot::IsValidID(depot_id)) return;

	if (departure) {
		int window_number = DepotTimetableWindow::GetDepotDepartureTimetableWindowNumber(depot_id);
		if (!BringWindowToFrontById(WC_NODETIMETABLE_WINDOW, window_number)) {
			new DepotTimetableWindow(&_nodetimetable_window_desc, window_number, depot_id, vehicle_type);
		}
	}
	if (arrival) {
		int window_number = DepotTimetableWindow::GetDepotArrivalTimetableWindowNumber(depot_id);
		if (!BringWindowToFrontById(WC_NODETIMETABLE_WINDOW, window_number)) {
			new DepotTimetableWindow(&_nodetimetable_window_desc, window_number, depot_id, vehicle_type);
		}
	}
}
