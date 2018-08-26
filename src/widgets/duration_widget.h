/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file duration_widget.h Types related to the GUI for setting durations. */

#ifndef WIDGETS_DURATION_WIDGET_H
#define WIDGETS_DURATION_WIDGET_H

/** Widgets of the #SetDurationWindow class. */
enum SetDurationWidgets {
	WID_SDU_CAPTION,                ///< The caption of the window
	WID_SDU_FAST_SMALLER_BUTTON,    ///< The button for making the value smaller in bigger steps
	WID_SDU_SMALLER_BUTTON,         ///< The button for making the value smaller in steps of size one
	WID_SDU_LENGTH_EDITBOX,         ///< The editbox for entering the value
	WID_SDU_BIGGER_BUTTON,          ///< The button for making the value bigger in steps of size one
	WID_SDU_FAST_BIGGER_BUTTON,     ///< The button for making the value bigger in bigger steps
	WID_SDU_UNIT_DROPDOWN,          ///< The dropdown for choosing the unit
	WID_SDU_CHOOSE_BUTTON,          ///< The button for choosing a value
};

#endif /* WIDGETS_DURATION_WIDGET_H */
