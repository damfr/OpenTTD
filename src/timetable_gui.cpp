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
#include "timetable.h"
#include "tilehighlight_func.h"
#include "tilehighlight_type.h"
#include "viewport_func.h"

#include "widget_type.h"
#include "widgets/dropdown_func.h"
#include "widgets/timetable_widget.h"

#include "table/sprites.h"
#include "table/strings.h"

#include "safeguards.h"

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

	const Vehicle *vehicle; ///< Vehicle monitored by the window.

	Scrollbar *vscroll;

	VehicleOrderID order_over;         ///< Order over which another order is dragged, \c INVALID_VEH_ORDER_ID if none.

  	/** Under what reason are we using the PlaceObject functionality? */
	enum TimetablePlaceObjectState {
		TIMETABLE_POS_GOTO,
		TIMETABLE_POS_CONDITIONAL,
		TIMETABLE_POS_SHARE,
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
		if (selected_line_before_list_bounds < this->vehicle->GetNumOrders() * 2 + 1 && selected_line_before_list_bounds >= 0) {
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
		return IsContentLine(line) && vehicle->GetNumOrders() != INVALID_VEH_ORDER_ID && line < vehicle->GetNumOrders() * 2 && (line % 2 == 0);
	}

	/** Returns wether any timetable line (i.e. a line containing Arrival, Departure, Speed Limit) is currently selected. */
	bool IsTimetableLineSelected() const
	{
		return IsTimetableLine(this->selected_timetable_line);
	}

	/** Returns wether the given line is a timetable line. */
	bool IsTimetableLine(int line) const
	{
		return IsContentLine(line) && vehicle->GetNumOrders() != INVALID_VEH_ORDER_ID && line < vehicle->GetNumOrders() * 2 && (line % 2 == 1);
	}

	/** Returns wether the end of orders line is selected. */
	bool IsEndOfOrdersLineSelected() const
	{
		return IsAnyLineSelected() && this->selected_timetable_line == GetEndOfOrdersIndex();
	}

	/** Returns the index of the "End of orders" line. */
	int GetEndOfOrdersIndex() const
	{
		return vehicle->GetNumOrders() * 2;
	}

	/** Returns the VehicleOrderID corresponding to the currently selected line, or INVALID_VEH_ORDER_ID if that line doesn´t correspond to an Order. */
	VehicleOrderID GetSelectedOrderID() const
	{
		return GetOrderID(this->selected_timetable_line);
	}

	/** Returns the VehicleOrderId corresponding to the given line, or INVALID_VEH_ORDER_ID if that line doesn´t correspond to an Order. */
	VehicleOrderID GetOrderID(int line) const
	{
		return (IsDestinationLine(line) || IsTimetableLine(line)) ? line / 2 : INVALID_VEH_ORDER_ID;
 	}

	/**********************************************************************************************************************/
	/********************************************* Assembling output strings **********************************************/
	/**********************************************************************************************************************/

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

	/**********************************************************************************************************************/
	/****************************************** Keeping track of widget state *********************************************/
	/**********************************************************************************************************************/

	/** This function sets visibility and activation state of buttons, dropdowns, etc. according to the current widget state.
	 *  E.g. if the very first line at the top is selected, it makes the buttons for setting timetable start, offset, length
	 *  and name visible, and other buttons that are displayed at the same locations under other circumstances invisible.
	 *  Thus, this function needs to be called whenever any property / condition it evaluates changes, typically after mouse clicks etc.
	 */
	void UpdateButtonState()
	{
	}

	/**********************************************************************************************************************/
	/************************************** Processing clicks to the various buttons etc. *********************************/
	/**********************************************************************************************************************/

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
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case WID_VT_TIMETABLE_PANEL: {
				resize->height = FONT_HEIGHT_NORMAL;
				size->height = WD_FRAMERECT_TOP + 8 * resize->height + WD_FRAMERECT_BOTTOM;
				break;
			}
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
				break;

			case VIWD_REMOVE_ALL_ORDERS:
				/* Removed / replaced all orders (after deleting / sharing) */
				if (this->selected_timetable_line == INVALID_SELECTION) break;

				this->DeleteChildWindows();
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

				/* if from == INVALID_VEH_ORDER_ID, one order was added; if to == INVALID_VEH_ORDER_ID, one order was removed */
				uint old_num_orders = this->vehicle->GetNumOrders() - (uint)(from == INVALID_VEH_ORDER_ID) + (uint)(to == INVALID_VEH_ORDER_ID);

				VehicleOrderID selected_order = (this->selected_timetable_line + 1) / 2;
				if (selected_order == old_num_orders) selected_order = 0; /* when last travel time is selected, it belongs to order 0 */

				bool travel = HasBit(this->selected_timetable_line, 0);

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
						UpdateSelectedTimetableLine(INVALID_SELECTION);
						break;
					} else {
						/* Moving selected order */
						selected_order = to;
					}
				}

				/* recompute new selected_timetable_line */
				this->selected_timetable_line = 2 * selected_order - (int)travel;

				/* travel time of first order needs special handling */
				if (this->selected_timetable_line == INVALID_SELECTION) this->selected_timetable_line = this->vehicle->GetNumOrders() * 2 - 1;
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
		this->vscroll->SetCount(v->GetNumOrders() * 2 + 1);
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_VT_CAPTION: SetDParam(0, this->vehicle->index); break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
 		const Vehicle *v = this->vehicle;
		int selected = this->selected_timetable_line;

		switch (widget) {
			case WID_VT_TIMETABLE_PANEL: {
				int y = r.top + WD_FRAMERECT_TOP;
				int i = this->vscroll->GetPosition();
				VehicleOrderID order_id = (i + 1) / 2;

				bool rtl = _current_text_dir == TD_RTL;
				SetDParamMaxValue(0, v->GetNumOrders(), 2);
				int index_column_width = GetStringBoundingBox(STR_ORDER_INDEX).width + 2 * GetSpriteSize(rtl ? SPR_ARROW_RIGHT : SPR_ARROW_LEFT).width + 3;
				int middle = rtl ? r.right - WD_FRAMERECT_RIGHT - index_column_width : r.left + WD_FRAMERECT_LEFT + index_column_width;

				const Order *order = v->GetOrder(order_id);
				bool any_order = (order != NULL);
				while (order != NULL) {
					/* Don't draw anything if it extends past the end of the window. */
					if (!this->vscroll->IsVisible(i)) break;

					if (i % 2 == 0) {
						DrawOrderString(v, order, order_id, y, i == selected, r.left + WD_FRAMERECT_LEFT, middle, r.right - WD_FRAMERECT_RIGHT);
					} else {
						StringID string;
						TextColour colour = (i == selected) ? TC_WHITE : TC_BLACK;
						if (order->IsType(OT_CONDITIONAL)) {
							string = STR_TIMETABLE_NO_TRAVEL;
							DrawString(rtl ? r.left + WD_FRAMERECT_LEFT : middle, rtl ? middle : r.right - WD_FRAMERECT_LEFT, y, string, colour);
						} else if (order->IsType(OT_IMPLICIT)) {
							string = STR_TIMETABLE_NOT_TIMETABLEABLE;
							colour = ((i == selected) ? TC_SILVER : TC_GREY) | TC_NO_SHADE;
						} else {
							char buffer[2048] = "";
							const char* timetable_string = this->GetTimetableLineString(buffer, lastof(buffer), order, order_id);
							/* Mark orders which violate the time order, e.g. because arrival is later than departure. */
							if (!IsOrderTimetableValid(v, order)) {
								colour = TC_RED;
							}
							DrawString(rtl ? r.left + WD_FRAMERECT_LEFT : middle, rtl ? middle : r.right - WD_FRAMERECT_LEFT, y, timetable_string, colour);
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
				break;
			}
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		const Vehicle *v = this->vehicle;

		switch (widget) {
			case WID_VT_ORDER_VIEW: // Order view button
				ShowOrdersWindow(v);
				break;

			case WID_VT_TIMETABLE_PANEL: { // Main panel.
				ProcessListClick(pt);
				break;
			}

			case WID_VT_SHARED_ORDER_LIST:
				ShowVehicleListWindow(v);
				break;
		}

		this->SetDirty();
	}

	virtual void OnQueryTextFinished(char *str)
	{
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
			WID_VT_REFIT_BUTTON,
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
