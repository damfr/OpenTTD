/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file duration_gui.h Functions related to the graphical selection of a duration. */

#ifndef DURATION_GUI_H
#define DURATION_GUI_H

#include "date_type.h"
#include "querystring_gui.h"
#include "window_type.h"

/**
 * Callback for when a duration has been chosen
 * @param w the window that sends the callback
 * @param duration the duration that has been chosen
 */
typedef void SetDurationCallback(const Window *w, Duration duration);

static const uint MAX_LENGTH_LENGTH_INPUT = 7;
static const int MAX_LENGTH = 1000000;
static const int SLOW_STEP_SIZE = 1;
static const int FAST_STEP_SIZE = 20;

/** Window to select a date graphically by using dropdowns */
struct SetDurationWindow : Window {

private:
	QueryString query_string;
	StringID caption;

	SetDurationCallback *callback; ///< Callback to call when a duration has been selected
	Duration duration; ///< The currently selected duration
	bool min_value; ///< Minimum value this window may select, regardless of the chosen unit.

	enum UnitDropDownIndices {
		UDDI_DAYS = 1,
		UDDI_MONTHS = 2,
		UDDI_YEARS = 3,
	};

	void ShowUnitDropDown();
	void ParseEditBox();
	void ProcessChoose();

public:
	SetDurationWindow(WindowDesc *desc, WindowNumber window_number, Window *parent, Duration initial_duration, bool allow_zero, StringID caption, SetDurationCallback *callback);

	void CopyLengthIntoEditbox();
	void AdjustUnitDropDown();

	virtual void OnClick(Point pt, int widget, int click_count);
	virtual void OnDropdownSelect(int widget, int index);
	virtual EventState OnHotkey(int hotkey);
	virtual void OnEditboxChanged(int widget);
	virtual void OnPaint();
	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize);
};

void ShowSetDurationWindow(Window *parent, int window_number, Duration initial_duration, bool allow_zero, StringID caption, SetDurationCallback *callback);

#endif /* DATE_GUI_H */
