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

#include "widgets/timetable_widget.h"

#include "table/sprites.h"
#include "table/strings.h"

#include "safeguards.h"

struct TimetableWindow : Window {

	const Vehicle *vehicle; ///< Vehicle monitored by the window.

	Scrollbar *vscroll;

	TimetableWindow(WindowDesc *desc, WindowNumber window_number) :
			Window(desc),
			vehicle(Vehicle::Get(window_number))
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_VT_SCROLLBAR);
		this->FinishInitNested(window_number);

		this->owner = this->vehicle->owner;
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{

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
				break;

			case VIWD_MODIFY_ORDERS:
				if (!gui_scope) break;
				this->ReInit();
				break;

			default: {
				break;
			}
		}
	}


	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_VT_CAPTION: SetDParam(0, this->vehicle->index); break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case WID_VT_TIMETABLE_PANEL: {
				break;
			}

			case WID_VT_SUMMARY_PANEL: {
				break;
			}
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
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
