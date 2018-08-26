/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timetable_widget.h Types related to the timetable widgets. */

#ifndef WIDGETS_TIMETABLE_WIDGET_H
#define WIDGETS_TIMETABLE_WIDGET_H

/** Widgets of the #TimetableWindow class. */
enum VehicleTimetableWidgets {
	WID_VT_CAPTION,                     ///< Caption of the window.
	WID_VT_ORDER_VIEW,                  ///< Order view.

	WID_VT_SHIFT_BY_LENGTH_PAST_BUTTON, ///< Button for shifting the timetable into the past by its length
	WID_VT_SHIFT_BY_LENGTH_FUTURE_BUTTON, ///< Button for shifting the timetable into the future by its length
	WID_VT_FULL_FILTER_BUTTON,          ///< Filter button for showing all data lines
	WID_VT_DESTINATION_FILTER_BUTTON,   ///< Filter button for showing the destination lines
	WID_VT_TIMETABLE_FILTER_BUTTON,     ///< Filter button for showing the timetable lines

	WID_VT_SUMMARY_PANEL,               ///< Summary panel.
	WID_VT_TIMETABLE_PANEL,             ///< Timetable panel.
	WID_VT_SCROLLBAR,                   ///< Scrollbar for the panel.

	WID_VT_TOP_SELECTION,               ///< Selection for the whole upper button row
	WID_VT_SELECTION_TOP_1,             ///< Selection for the first button of the upper button row
	WID_VT_NON_STOP_DROPDOWN,           ///< The non-stop drop down
	WID_VT_NON_STOP_STATION_DROPDOWN,   ///< The non-stop drop down, for the station row
	WID_VT_NON_STOP_WAYPOINT_DROPDOWN,  ///< The non-stop drop down, for the waypoint row
	WID_VT_NON_STOP_DEPOT_DROPDOWN,     ///< The non-stop drop down, for the depot row
	WID_VT_RENAME_BUTTON,               ///< Button for renaming a timetable
	WID_VT_ARRIVAL_BUTTON,              ///< Button for setting the arrival of an order
	WID_VT_START_BUTTON,                ///< The button for setting the start of the timetable
	WID_VT_AUTOFILL_SELECTION,          ///< Selection for the autofill start vs. stop buttons
	WID_VT_START_AUTOFILL_DROPDOWN,     ///< The dropdown for starting autofill.
	WID_VT_STOP_AUTOFILL_BUTTON,        ///< The button for stopping autofill.
	WID_VT_AUTOFILL_INFO_PANEL,         ///< Panel with status information about autofill
	WID_VT_COND_VARIABLE_DROPDOWN,      ///< Dropdown for the variable of conditional orders

	WID_VT_SELECTION_TOP_2,             ///< Selection for the second button of the upper button row
	WID_VT_FULL_LOAD_DROPDOWN,          ///< The full load drop down
	WID_VT_REFIT_BUTTON,                ///< The refit button
	WID_VT_OFFSET_BUTTON,               ///< The button for setting the offset of the timetable
	WID_VT_COND_COMPARATOR_DROPDOWN,    ///< Dropdown for the comparator of conditional orders

	WID_VT_SELECTION_TOP_3,             ///< Selection for the third button of the upper button row
	WID_VT_UNLOAD_DROPDOWN,             ///< The unload drop down
	WID_VT_SERVICE_DROPDOWN,            ///< The service dropdown
	WID_VT_SPEEDLIMIT_BUTTON,           ///< The button for setting the speed limit
	WID_VT_LENGTH_BUTTON,               ///< The button for setting the length of the timetable
	WID_VT_COND_VALUE_BUTTON,           ///< Button for the value of conditional orders

	WID_VT_SELECTION_TOP_4,             ///< Selection for the fourth button of the upper button row
	WID_VT_REFIT_SELECTION,             ///< Selection for refit button vs. dropdown
	WID_VT_REFIT_BUTTON_4,              ///< The refit button
	WID_VT_REFIT_AUTO_DROPDOWN,         ///< Drop down for auto refit
	WID_VT_DEPARTURE_BUTTON,            ///< The button for setting the departure time of an order
	WID_VT_SHIFT_ORDERS_PAST_BUTTON,    ///< Button for shifting parts of the timetable into the past by a specified amount
	WID_VT_SHIFT_ORDERS_FUTURE_BUTTON,  ///< Button for shifting parts of the timetable into the future by a specified amount

	WID_VT_SHARED_ORDER_LIST,           ///< Show the shared order list.
	WID_VT_EXPECTED_SELECTION,          ///< Disable/hide the expected selection button.

	WID_VT_SKIP_ORDER_BUTTON,           ///< The skip order button

	WID_VT_SELECTION_BOTTOM_2,          ///< Selection for the second button in the lower button row
	WID_VT_DELETE_ORDER_BUTTON,         ///< Button for deleting an order
	WID_VT_STOP_SHARING_BUTTON,         ///< Button for stopping sharing orders

	WID_VT_GOTO_BUTTON,                 ///< The goto button
};

/** Widgets of the node timetable window */
enum NodeTimetableWindowWidgets {
	WID_NTW_CAPTION,                          ///< Caption of the window
	WID_NTW_TIMETABLE_PANEL,                  ///< The panel where the timetable is displayed
	WID_NTW_SCROLLBAR,                        ///< The scrollbar
	WID_NTW_MODE_BUTTON                       ///< Button for controlling wether the Window is in departure or arrival mode
};

#endif /* WIDGETS_TIMETABLE_WIDGET_H */
