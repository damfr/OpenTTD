/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file date_widget.h Types related to the date widgets. */

#ifndef WIDGETS_DATE_WIDGET_H
#define WIDGETS_DATE_WIDGET_H

/** Widgets of the #SetDateWindow class. */
enum SetDateWidgets {
	WID_SD_DAY,      ///< Dropdown for the day.
	WID_SD_MONTH,    ///< Dropdown for the month.
	WID_SD_YEAR,     ///< Dropdown for the year.
	WID_SD_SET_DATE, ///< Actually set the date.
};

enum SetDateFastWidgets {
	WID_SDF_CAPTION,                  ///< Caption of the Window
	WID_SDF_SMALLER_BUTTON_ONE,       ///< Button for decreaing the date in small steps
	WID_SDF_SMALLER_BUTTON_TWO,       ///< Button for decreaing the date in somewhat bigger steps
	WID_SDF_SMALLER_BUTTON_THREE,     ///< Button for decreaing the date in big steps
	WID_SDF_BIGGER_BUTTON_ONE,        ///< Button for increaing the date in small steps
	WID_SDF_BIGGER_BUTTON_TWO,        ///< Button for increaing the date in somewhat bigger steps
	WID_SDF_BIGGER_BUTTON_THREE,      ///< Button for increaing the date in big steps
	WID_SDF_DATE_PANEL,               ///< Panel for painting the currently selected date
	WID_SDF_CHOOSE_AND_CLOSE_BUTTON,  ///< Button for choosing the current date, and not choosing another one right away
	WID_SDF_CHOOSE_AND_NEXT_BUTTON,   ///< Button for choosing the current date, and continuing with choosing the next one right away (if possible)
};

#endif /* WIDGETS_DATE_WIDGET_H */
