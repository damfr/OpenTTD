/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file nodetimetable_gui.cpp GUI for displaying timetables for route nodes, e.g. stations, depots etc. */

#include "stdafx.h"

#include "depot_base.h"
#include "station_base.h"
#include "strings_func.h"
#include "timetable_func.h"
#include "waypoint_base.h"
#include "window_gui.h"

#include "table/strings.h"

#include "widgets/timetable_widget.h"

static const NWidgetPart _nested_nodetimetable_window_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_NTW_CAPTION), SetDataTip(STR_NULL, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_NTW_TIMETABLE_PANEL), SetMinimalSize(150, 150), SetResize(1, 1),
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
	WDP_AUTO, NULL, 220, 138,
	WC_NODETIMETABLE_WINDOW, WC_NONE,
	0,
	_nested_nodetimetable_window_widgets, lengthof(_nested_nodetimetable_window_widgets)
);

struct LocationTimetableWindow : public Window {

public:
	bool departure_timetable;

	LocationTimetableWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->FinishInitNested(window_number);

		this->window_number = window_number;
		this->departure_timetable = (window_number < 0x100000);

		NWidgetCore *widget = this->GetWidget<NWidgetCore>(WID_NTW_MODE_BUTTON);
		widget->widget_data = this->departure_timetable ? STR_NODETIMETABLE_ARRIVAL_BUTTON_CAPTION : STR_NODETIMETABLE_DEPARTURE_BUTTON_CAPTION;
		widget->tool_tip  = this->departure_timetable ? STR_NODETIMETABLE_ARRIVAL_BUTTON_TOOLTIP : STR_NODETIMETABLE_DEPARTURE_BUTTON_TOOLTIP;
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

public:
	DepotTimetableWindow(WindowDesc *desc, WindowNumber window_number, DepotID depot_id) : LocationTimetableWindow(desc, window_number)
	{
		this->depot_id = depot_id;
		this->GetWidget<NWidgetCore>(WID_NTW_CAPTION)->widget_data = this->departure_timetable ? STR_NODETIMETABLE_DEPOT_DEPARTURE_CAPTION : STR_NODETIMETABLE_DEPOT_ARRIVAL_CAPTION;
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_NTW_CAPTION: {
				SetDParam(0, this->depot_id);
				break;
			}
		}
	}

	virtual void ShowCorrespondingArrivalTimetable()
	{
		ShowDepotTimetableWindow(this->depot_id, false, true);
	}

	virtual void ShowCorrespondingDepartureTimetable()
	{
		ShowDepotTimetableWindow(this->depot_id, true, false);
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

void ShowDepotTimetableWindow(DepotID depot_id, bool departure, bool arrival)
{
	if (!Depot::IsValidID(depot_id)) return;

	if (departure) {
		int window_number = DepotTimetableWindow::GetDepotDepartureTimetableWindowNumber(depot_id);
		if (!BringWindowToFrontById(WC_NODETIMETABLE_WINDOW, window_number)) {
			new DepotTimetableWindow(&_nodetimetable_window_desc, window_number, depot_id);
		}
	}
	if (arrival) {
		int window_number = DepotTimetableWindow::GetDepotArrivalTimetableWindowNumber(depot_id);
		if (!BringWindowToFrontById(WC_NODETIMETABLE_WINDOW, window_number)) {
			new DepotTimetableWindow(&_nodetimetable_window_desc, window_number, depot_id);
		}
	}
}
