/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timetable_gui.cpp GUI for time tabling. */

#include "stdafx.h"
#include "command_func.h"
#include "gui.h"
#include "window_gui.h"
#include "window_func.h"
#include "textbuf_gui.h"
#include "strings_func.h"
#include "vehicle_base.h"
#include "string_func.h"
#include "gfx_func.h"
#include "company_func.h"
#include "date_func.h"
#include "date_gui.h"
#include "vehicle_gui.h"
#include "settings_type.h"

#include "widgets/timetable_widget.h"

#include "table/sprites.h"
#include "table/strings.h"

#include "safeguards.h"

/** Container for the arrival/departure dates of a vehicle */
struct TimetableArrivalDeparture {
	Ticks arrival;   ///< The arrival time
	Ticks departure; ///< The departure time
};

/**
 * Set the timetable parameters in the format as described by the setting.
 * @param param1 the first DParam to fill
 * @param param2 the second DParam to fill
 * @param ticks  the number of ticks to 'draw'
 */
void SetTimetableParams(int param1, int param2, Ticks ticks)
{
	if (_settings_client.gui.timetable_in_ticks) {
		SetDParam(param1, STR_TIMETABLE_TICKS);
		SetDParam(param2, ticks);
	} else {
		SetDParam(param1, STR_TIMETABLE_DAYS);
		SetDParam(param2, ticks / DAY_TICKS);
	}
}

/**
 * Check whether it is possible to determine how long the order takes.
 * @param order the order to check.
 * @param travelling whether we are interested in the travel or the wait part.
 * @return true if the travel/wait time can be used.
 */
static bool CanDetermineTimeTaken(const Order *order, bool travelling)
{
	/* Current order is conditional */
	if (order->IsType(OT_CONDITIONAL) || order->IsType(OT_IMPLICIT)) return false;
	/* No travel time and we have not already finished travelling */
	if (travelling && !order->IsTravelTimetabled()) return false;
	/* No wait time but we are loading at this timetabled station */
	if (!travelling && !order->IsWaitTimetabled() && order->IsType(OT_GOTO_STATION) &&
			!(order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION)) {
		return false;
	}

	return true;
}


/**
 * Fill the table with arrivals and departures
 * @param v Vehicle which must have at least 2 orders.
 * @param start order index to start at
 * @param travelling Are we still in the travelling part of the start order
 * @param table Fill in arrival and departures including intermediate orders
 * @param offset Add this value to result and all arrivals and departures
 */
static void FillTimetableArrivalDepartureTable(const Vehicle *v, VehicleOrderID start, bool travelling, TimetableArrivalDeparture *table, Ticks offset)
{
	assert(table != nullptr);
	assert(v->GetNumOrders() >= 2);
	assert(start < v->GetNumOrders());

	Ticks sum = offset;
	VehicleOrderID i = start;
	const Order *order = v->GetOrder(i);

	/* Pre-initialize with unknown time */
	for (int i = 0; i < v->GetNumOrders(); ++i) {
		table[i].arrival = table[i].departure = INVALID_TICKS;
	}

	/* Cyclically loop over all orders until we reach the current one again.
	 * As we may start at the current order, do a post-checking loop */
	do {
		/* Automatic orders don't influence the overall timetable;
		 * they just add some untimetabled entries, but the time till
		 * the next non-implicit order can still be known. */
		if (!order->IsType(OT_IMPLICIT)) {
			if (travelling || i != start) {
				if (!CanDetermineTimeTaken(order, true)) return;
				sum += order->GetTimetabledTravel();
				table[i].arrival = sum;
			}

			if (!CanDetermineTimeTaken(order, false)) return;
			sum += order->GetTimetabledWait();
			table[i].departure = sum;
		}

		++i;
		order = order->next;
		if (i >= v->GetNumOrders()) {
			i = 0;
			assert(order == nullptr);
			order = v->orders.list->GetFirstOrder();
		}
	} while (i != start);

	/* When loading at a scheduled station we still have to treat the
	 * travelling part of the first order. */
	if (!travelling) {
		if (!CanDetermineTimeTaken(order, true)) return;
		sum += order->GetTimetabledTravel();
		table[i].arrival = sum;
	}
}

struct TimetableWindow : Window {
	int sel_index;
	const Vehicle *vehicle; ///< Vehicle monitored by the window.
	bool show_expected;     ///< Whether we show expected arrival or scheduled
	uint deparr_time_width; ///< The width of the departure/arrival time
	uint deparr_abbr_width; ///< The width of the departure/arrival abbreviation
	Scrollbar *vscroll;
	bool query_is_speed_query; ///< The currently open query window is a speed query and not a time query.

	TimetableWindow(WindowDesc *desc, WindowNumber window_number) :
			Window(desc),
			sel_index(-1),
			vehicle(Vehicle::Get(window_number)),
			show_expected(true)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_VT_SCROLLBAR);
		this->FinishInitNested(window_number);

		this->owner = this->vehicle->owner;
	}

	/**
	 * Build the arrival-departure list for a given vehicle
	 * @param v the vehicle to make the list for
	 * @param table the table to fill
	 * @return if next arrival will be early
	 */
	static bool BuildArrivalDepartureList(const Vehicle *v, TimetableArrivalDeparture *table)
	{
		assert(HasBit(v->vehicle_flags, VF_TIMETABLE_STARTED));

		bool travelling = (!v->current_order.IsType(OT_LOADING) || v->current_order.GetNonStopType() == ONSF_STOP_EVERYWHERE);
		Ticks start_time = _date_fract - v->current_order_time;

		FillTimetableArrivalDepartureTable(v, v->cur_real_order_index % v->GetNumOrders(), travelling, table, start_time);

		return (travelling && v->lateness_counter < 0);
	}

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override
	{
		switch (widget) {
			case WID_VT_TIMETABLE_PANEL:
				resize->height = FONT_HEIGHT_NORMAL;
				size->height = WD_FRAMERECT_TOP + 8 * resize->height + WD_FRAMERECT_BOTTOM;
				break;

			case WID_VT_SUMMARY_PANEL:
				size->height = WD_FRAMERECT_TOP + 2 * FONT_HEIGHT_NORMAL + WD_FRAMERECT_BOTTOM;
				break;
		}
	}

	int GetOrderFromTimetableWndPt(int y, const Vehicle *v)
	{
		int sel = (y - this->GetWidget<NWidgetBase>(WID_VT_TIMETABLE_PANEL)->pos_y - WD_FRAMERECT_TOP) / FONT_HEIGHT_NORMAL;

		if ((uint)sel >= this->vscroll->GetCapacity()) return INVALID_ORDER;

		sel += this->vscroll->GetPosition();

		return (sel < v->GetNumOrders() * 2 && sel >= 0) ? sel : INVALID_ORDER;
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		switch (data) {
			case VIWD_AUTOREPLACE:
				/* Autoreplace replaced the vehicle */
				this->vehicle = Vehicle::Get(this->window_number);
				break;

			case VIWD_REMOVE_ALL_ORDERS:
				/* Removed / replaced all orders (after deleting / sharing) */
				if (this->sel_index == -1) break;

				this->DeleteChildWindows();
				this->sel_index = -1;
				break;

			case VIWD_MODIFY_ORDERS:
				if (!gui_scope) break;
				this->ReInit();
				break;

			default: {
				if (gui_scope) break; // only do this once; from command scope

				/* Moving an order. If one of these is INVALID_VEH_ORDER_ID, then
				 * the order is being created / removed */
				if (this->sel_index == -1) break;

				VehicleOrderID from = GB(data, 0, 8);
				VehicleOrderID to   = GB(data, 8, 8);

				if (from == to) break; // no need to change anything

				/* if from == INVALID_VEH_ORDER_ID, one order was added; if to == INVALID_VEH_ORDER_ID, one order was removed */
				uint old_num_orders = this->vehicle->GetNumOrders() - (uint)(from == INVALID_VEH_ORDER_ID) + (uint)(to == INVALID_VEH_ORDER_ID);

				VehicleOrderID selected_order = (this->sel_index + 1) / 2;
				if (selected_order == old_num_orders) selected_order = 0; // when last travel time is selected, it belongs to order 0

				bool travel = HasBit(this->sel_index, 0);

				if (from != selected_order) {
					/* Moving from preceding order? */
					selected_order -= (int)(from <= selected_order);
					/* Moving to   preceding order? */
					selected_order += (int)(to   <= selected_order);
				} else {
					/* Now we are modifying the selected order */
					if (to == INVALID_VEH_ORDER_ID) {
						/* Deleting selected order */
						this->DeleteChildWindows();
						this->sel_index = -1;
						break;
					} else {
						/* Moving selected order */
						selected_order = to;
					}
				}

				/* recompute new sel_index */
				this->sel_index = 2 * selected_order - (int)travel;
				/* travel time of first order needs special handling */
				if (this->sel_index == -1) this->sel_index = this->vehicle->GetNumOrders() * 2 - 1;
				break;
			}
		}
	}


	void OnPaint() override
	{
		const Vehicle *v = this->vehicle;
		int selected = this->sel_index;

		this->vscroll->SetCount(v->GetNumOrders() * 2);

		if (v->owner == _local_company) {
			bool disable = true;
			if (selected != -1) {
				const Order *order = v->GetOrder(((selected + 1) / 2) % v->GetNumOrders());
				if (selected % 2 == 1) {
					disable = order != nullptr && (order->IsType(OT_CONDITIONAL) || order->IsType(OT_IMPLICIT));
				} else {
					disable = order == nullptr || ((!order->IsType(OT_GOTO_STATION) || (order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION)) && !order->IsType(OT_CONDITIONAL));
				}
			}
			this->SetWidgetDisabledState(WID_VT_SHARED_ORDER_LIST, !v->IsOrderListShared());
		} else {
			this->DisableWidget(WID_VT_SHARED_ORDER_LIST);
		}

		this->DrawWidgets();
	}

	void SetStringParameters(int widget) const override
	{
		switch (widget) {
			case WID_VT_CAPTION: SetDParam(0, this->vehicle->index); break;
		}
	}

	void DrawWidget(const Rect &r, int widget) const override
	{
		const Vehicle *v = this->vehicle;
		int selected = this->sel_index;

		switch (widget) {
			case WID_VT_TIMETABLE_PANEL: {
				int y = r.top + WD_FRAMERECT_TOP;
				int i = this->vscroll->GetPosition();
				VehicleOrderID order_id = (i + 1) / 2;
				bool final_order = false;

				bool rtl = _current_text_dir == TD_RTL;
				SetDParamMaxValue(0, v->GetNumOrders(), 2);
				int index_column_width = GetStringBoundingBox(STR_ORDER_INDEX).width + 2 * GetSpriteSize(rtl ? SPR_ARROW_RIGHT : SPR_ARROW_LEFT).width + 3;
				int middle = rtl ? r.right - WD_FRAMERECT_RIGHT - index_column_width : r.left + WD_FRAMERECT_LEFT + index_column_width;

				const Order *order = v->GetOrder(order_id);
				while (order != nullptr) {
					/* Don't draw anything if it extends past the end of the window. */
					if (!this->vscroll->IsVisible(i)) break;

					if (i % 2 == 0) {
						DrawOrderString(v, order, order_id, y, i == selected, true, r.left + WD_FRAMERECT_LEFT, middle, r.right - WD_FRAMERECT_RIGHT);

						order_id++;

						if (order_id >= v->GetNumOrders()) {
							order = v->GetOrder(0);
							final_order = true;
						} else {
							order = order->next;
						}
					} else {
						StringID string;
						TextColour colour = (i == selected) ? TC_WHITE : TC_BLACK;
						if (order->IsType(OT_CONDITIONAL)) {
							string = STR_TIMETABLE_NO_TRAVEL;
						} else if (order->IsType(OT_IMPLICIT)) {
							string = STR_TIMETABLE_NOT_TIMETABLEABLE;
							colour = ((i == selected) ? TC_SILVER : TC_GREY) | TC_NO_SHADE;
						} else if (!order->IsTravelTimetabled()) {
							if (order->GetTravelTime() > 0) {
								SetTimetableParams(0, 1, order->GetTravelTime());
								string = order->GetMaxSpeed() != UINT16_MAX ?
										STR_TIMETABLE_TRAVEL_FOR_SPEED_ESTIMATED  :
										STR_TIMETABLE_TRAVEL_FOR_ESTIMATED;
							} else {
								string = order->GetMaxSpeed() != UINT16_MAX ?
										STR_TIMETABLE_TRAVEL_NOT_TIMETABLED_SPEED :
										STR_TIMETABLE_TRAVEL_NOT_TIMETABLED;
							}
						} else {
							SetTimetableParams(0, 1, order->GetTimetabledTravel());
							string = order->GetMaxSpeed() != UINT16_MAX ?
									STR_TIMETABLE_TRAVEL_FOR_SPEED : STR_TIMETABLE_TRAVEL_FOR;
						}
						SetDParam(2, order->GetMaxSpeed());

						DrawString(rtl ? r.left + WD_FRAMERECT_LEFT : middle, rtl ? middle : r.right - WD_FRAMERECT_LEFT, y, string, colour);

						if (final_order) break;
					}

					i++;
					y += FONT_HEIGHT_NORMAL;
				}
				break;
			}

			case WID_VT_SUMMARY_PANEL: {
				int y = r.top + WD_FRAMERECT_TOP;

				Ticks total_time = v->orders.list != nullptr ? v->orders.list->GetTimetableDurationIncomplete() : 0;
				if (total_time != 0) {
					SetTimetableParams(0, 1, total_time);
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, v->orders.list->IsCompleteTimetable() ? STR_TIMETABLE_TOTAL_TIME : STR_TIMETABLE_TOTAL_TIME_INCOMPLETE);
				}
				y += FONT_HEIGHT_NORMAL;

				if (v->timetable_start != 0) {
					/* We are running towards the first station so we can start the
					 * timetable at the given time. */
					SetDParam(0, STR_JUST_DATE_TINY);
					SetDParam(1, v->timetable_start);
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_TIMETABLE_STATUS_START_AT);
				} else if (!HasBit(v->vehicle_flags, VF_TIMETABLE_STARTED)) {
					/* We aren't running on a timetable yet, so how can we be "on time"
					 * when we aren't even "on service"/"on duty"? */
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_TIMETABLE_STATUS_NOT_STARTED);
				} else if (v->lateness_counter == 0 || (!_settings_client.gui.timetable_in_ticks && v->lateness_counter / DAY_TICKS == 0)) {
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_TIMETABLE_STATUS_ON_TIME);
				} else {
					SetTimetableParams(0, 1, abs(v->lateness_counter));
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, v->lateness_counter < 0 ? STR_TIMETABLE_STATUS_EARLY : STR_TIMETABLE_STATUS_LATE);
				}
				break;
			}
		}
	}

	static inline uint32 PackTimetableArgs(const Vehicle *v, uint selected, bool speed)
	{
		uint order_number = (selected + 1) / 2;
		ModifyTimetableFlags mtf = (selected % 2 == 1) ? (speed ? MTF_TRAVEL_SPEED : MTF_TRAVEL_TIME) : MTF_WAIT_TIME;

		if (order_number >= v->GetNumOrders()) order_number = 0;

		return v->index | (order_number << 20) | (mtf << 28);
	}

	void OnClick(Point pt, int widget, int click_count) override
	{
		const Vehicle *v = this->vehicle;

		switch (widget) {
			case WID_VT_ORDER_VIEW: // Order view button
				ShowOrdersWindow(v);
				break;

			case WID_VT_TIMETABLE_PANEL: { // Main panel.
				int selected = GetOrderFromTimetableWndPt(pt.y, v);

				this->DeleteChildWindows();
				this->sel_index = (selected == INVALID_ORDER || selected == this->sel_index) ? -1 : selected;
				break;
			}

			case WID_VT_SHARED_ORDER_LIST:
				ShowVehicleListWindow(v);
				break;
		}

		this->SetDirty();
	}

	void OnQueryTextFinished(char *str) override
	{
		if (str == nullptr) return;

		const Vehicle *v = this->vehicle;

		uint32 p1 = PackTimetableArgs(v, this->sel_index, this->query_is_speed_query);

		uint64 val = StrEmpty(str) ? 0 : strtoul(str, nullptr, 10);
		if (this->query_is_speed_query) {
			val = ConvertDisplaySpeedToKmhishSpeed(val);
		} else {
			if (!_settings_client.gui.timetable_in_ticks) val *= DAY_TICKS;
		}

		uint32 p2 = std::min<uint32>(val, UINT16_MAX);

		DoCommandP(0, p1, p2, CMD_CHANGE_TIMETABLE | CMD_MSG(STR_ERROR_CAN_T_TIMETABLE_VEHICLE));
	}

	void OnResize() override
	{
		/* Update the scroll bar */
		this->vscroll->SetCapacityFromWidget(this, WID_VT_TIMETABLE_PANEL, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM);
	}
};

static const NWidgetPart _nested_timetable_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_VT_CAPTION), SetDataTip(STR_TIMETABLE_TITLE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
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
