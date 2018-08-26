/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file datefast_gui.cpp Fast graphical selection of dates. */

#include "stdafx.h"
#include "strings_func.h"
#include "string_func.h"
#include "date_func.h"
#include "window_func.h"
#include "window_gui.h"
#include "date_gui.h"
#include "core/geometry_func.hpp"
#include "core/math_func.hpp"

#include "widgets/dropdown_type.h"
#include "widgets/date_widget.h"

/** Window to select a date fast, choosing it by buttons starting at a default date, with the option to choose the next date right away */
struct SetDateFastWindow : Window {
	SetDateFastCallback *callback; ///< Callback to call when a date has been selected
	Date date;                     ///< The currently selected date
	Date min_date;                 ///< The minimal allowed date (inclusive)
	Date max_date;                 ///< The maximal allowed date (exclusive)
	char caption_text[256];        ///< The text to display in the caption

	int step_sizes[6];             ///< The step sizes from left to right, must be exactly six

	SetDateFastWindow(WindowDesc *desc, WindowNumber window_number, Window *parent, Date initial_date, Date min_date, Date max_date, const char* caption_text,
				 const int step_sizes[], const StringID step_size_labels[], SetDateFastCallback *callback) :
			Window(desc)
	{
		assert(min_date <= max_date);

		this->callback = callback;
		this->date = initial_date;
		this->min_date = min_date;
		this->max_date = max_date;
		/* Deliberately make a copy of the String, as it might be overwritten outside this class */
		strecpy(this->caption_text, caption_text, lastof(this->caption_text));

		for (int n = 0; n < 6; n++) {
			this->step_sizes[n] = step_sizes[n];
		}

		this->parent = parent;

		this->InitNested(window_number);

		NWidgetCore *caption = this->GetWidget<NWidgetCore>(WID_SDF_CAPTION);
		caption->SetDataTip(STR_JUST_RAW_STRING, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS);

		NWidgetCore *smaller_three = this->GetWidget<NWidgetCore>(WID_SDF_SMALLER_BUTTON_THREE);
		smaller_three->SetDataTip(step_size_labels[0], STR_DATE_DECREASE_TOOLTIP);
		NWidgetCore *smaller_two = this->GetWidget<NWidgetCore>(WID_SDF_SMALLER_BUTTON_TWO);
		smaller_two->SetDataTip(step_size_labels[1], STR_DATE_DECREASE_TOOLTIP);
		NWidgetCore *smaller_one = this->GetWidget<NWidgetCore>(WID_SDF_SMALLER_BUTTON_ONE);
		smaller_one->SetDataTip(step_size_labels[2], STR_DATE_DECREASE_TOOLTIP);



		NWidgetCore *bigger_one = this->GetWidget<NWidgetCore>(WID_SDF_BIGGER_BUTTON_ONE);
		bigger_one->SetDataTip(step_size_labels[3], STR_DATE_DECREASE_TOOLTIP);
		NWidgetCore *bigger_two = this->GetWidget<NWidgetCore>(WID_SDF_BIGGER_BUTTON_TWO);
		bigger_two->SetDataTip(step_size_labels[4], STR_DATE_DECREASE_TOOLTIP);
		NWidgetCore *bigger_three = this->GetWidget<NWidgetCore>(WID_SDF_BIGGER_BUTTON_THREE);
		bigger_three->SetDataTip(step_size_labels[5], STR_DATE_DECREASE_TOOLTIP);
	}

	/** This function sets both date and callback of this window to the given values.  The idea is that if
	 *  a user hits Choose and Next, the parent window processes the results, and then calls this function
	 *  to prepare the dialog for choosing the next date.
	 */
	void SetData(Date date, const char* caption_text, SetDateFastCallback *callback)
	{
		this->date = date;
		/* Deliberately make a copy of the String, as it might be overwritten outside this class */
		strecpy(this->caption_text, caption_text, lastof(this->caption_text));
		this->callback = callback;
		this->InvalidateData();
	}
/*
	virtual Point OnInitialPosition(const WindowDesc *desc, int16 sm_width, int16 sm_height, int window_number)
	{
		Point pt = { this->parent->left + this->parent->width / 2 - sm_width / 2, this->parent->top + this->parent->height / 2 - sm_height / 2 };
		return pt;
	}
*/

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_SDF_SMALLER_BUTTON_THREE: {
				this->date = Clamp(this->date + step_sizes[0], this->min_date, this->max_date);
				this->InvalidateData();
				break;
			}
			case WID_SDF_SMALLER_BUTTON_TWO: {
				this->date = Clamp(this->date + step_sizes[1], this->min_date, this->max_date);
				this->InvalidateData();
				break;
			}
			case WID_SDF_SMALLER_BUTTON_ONE: {
				this->date = Clamp(this->date + step_sizes[2], this->min_date, this->max_date);
				this->InvalidateData();
				break;
			}
			case WID_SDF_BIGGER_BUTTON_ONE: {
				this->date = Clamp(this->date + step_sizes[3], this->min_date, this->max_date);
				this->InvalidateData();
				break;
			}
			case WID_SDF_BIGGER_BUTTON_TWO: {
				this->date = Clamp(this->date + step_sizes[4], this->min_date, this->max_date);
				this->InvalidateData();
				break;
			}
			case WID_SDF_BIGGER_BUTTON_THREE: {
				this->date = Clamp(this->date + step_sizes[5], this->min_date, this->max_date);
				this->InvalidateData();
				break;
			}
			case WID_SDF_CHOOSE_AND_CLOSE_BUTTON: {
					if (this->callback != NULL) this->callback(this->parent, this->date, false);
					delete this;
					break;
			}
			case WID_SDF_CHOOSE_AND_NEXT_BUTTON: {
					if (this->callback != NULL) this->callback(this->parent, this->date, true);
					break;
			}
		}
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_SDF_CAPTION: {
				SetDParamStr(0, this->caption_text);
				break;
			}
		}
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case WID_SDF_DATE_PANEL: {
				int y = r.top + WD_FRAMERECT_TOP;
				SetDParam(0, this->date);
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_JUST_DATE_LONG, TC_BLACK, SA_HOR_CENTER);
			}
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case WID_SDF_CAPTION: {
				SetDParamStr(0, this->caption_text);
				Dimension d = GetStringBoundingBox(STR_JUST_RAW_STRING);
				size->width = d.width + 25;
				break;
			}

			case WID_SDF_DATE_PANEL: {
				SetDParam(0, this->date);
				Dimension d = GetStringBoundingBox(STR_JUST_DATE_LONG);
				size->width = d.width + 10;
				resize->height = FONT_HEIGHT_NORMAL;
				size->height = WD_FRAMERECT_TOP + resize->height + WD_FRAMERECT_BOTTOM;
				break;
			}

			case WID_SDF_CHOOSE_AND_CLOSE_BUTTON: {
				Dimension d = GetStringBoundingBox(STR_DATE_CHOOSE_AND_CLOSE_BUTTON_CAPTION);
				size->width = d.width + 20;
				break;
			}

			case WID_SDF_CHOOSE_AND_NEXT_BUTTON: {
				Dimension d = GetStringBoundingBox(STR_DATE_CHOOSE_AND_NEXT_BUTTON_CAPTION);
				size->width = d.width + 20;
				break;
			}
		}
	}
};

/** Widgets for the date setting window. */
static const NWidgetPart _nested_set_date_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_SDF_CAPTION), SetDataTip(STR_DATE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(NWID_VERTICAL), SetPIP(6, 6, 6),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(6, 6, 6),
				/* NOTE: I use STR_TIMETABLE_DATE_MINUS_THREE here for all buttons.  In fact the string given here is irrelevant, as we override it
				 * in the constructor anyway, i.e. you will never see the string given here.  However, if I choose STR_NULL, 
				 * the widget engine makes the buttons ways to big.  Thus I choose a string with representative size here. */
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SDF_SMALLER_BUTTON_THREE), SetMinimalSize(50, 12), SetDataTip(STR_TIMETABLE_DATE_MINUS_THREE, STR_DATE_DECREASE_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SDF_SMALLER_BUTTON_TWO), SetMinimalSize(50, 12), SetDataTip(STR_TIMETABLE_DATE_MINUS_THREE, STR_DATE_DECREASE_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SDF_SMALLER_BUTTON_ONE), SetMinimalSize(50, 12), SetDataTip(STR_TIMETABLE_DATE_MINUS_THREE, STR_DATE_DECREASE_TOOLTIP),
				NWidget(WWT_PANEL, COLOUR_GREY, WID_SDF_DATE_PANEL), SetMinimalSize(80, 12), SetResize(0, 0), SetDataTip(STR_NULL, STR_NULL), EndContainer(),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SDF_BIGGER_BUTTON_ONE), SetMinimalSize(50, 12), SetDataTip(STR_TIMETABLE_DATE_MINUS_THREE, STR_DATE_INCREASE_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SDF_BIGGER_BUTTON_TWO), SetMinimalSize(50, 12), SetDataTip(STR_TIMETABLE_DATE_MINUS_THREE, STR_DATE_INCREASE_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SDF_BIGGER_BUTTON_THREE), SetMinimalSize(50, 12), SetDataTip(STR_TIMETABLE_DATE_MINUS_THREE, STR_DATE_INCREASE_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_SPACER), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SDF_CHOOSE_AND_CLOSE_BUTTON), SetMinimalSize(100, 12),
							  SetDataTip(STR_DATE_CHOOSE_AND_CLOSE_BUTTON_CAPTION, STR_DATE_CHOOSE_AND_CLOSE_BUTTON_TOOLTIP),
  				NWidget(NWID_SPACER), SetFill(1, 0), SetMinimalSize(20, 12),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SDF_CHOOSE_AND_NEXT_BUTTON), SetMinimalSize(100, 12),
							  SetDataTip(STR_DATE_CHOOSE_AND_NEXT_BUTTON_CAPTION, STR_DATE_CHOOSE_AND_NEXT_BUTTON_TOOLTIP),
				NWidget(NWID_SPACER), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer()
};

/** Description of the date setting window. */
static WindowDesc _set_date_desc(
	WDP_CENTER, NULL, 0, 0,
	WC_SET_DATE_FAST, WC_NONE,
	0,
	_nested_set_date_widgets, lengthof(_nested_set_date_widgets)
);

void ShowSetDateFastWindow(Window *parent, int window_number, Date initial_date, Date min_date, Date max_date, const char* caption_text,
						   const int step_sizes[], const StringID step_size_labels[], SetDateFastCallback *callback)
{
	DeleteWindowByClass(WC_SET_DATE_FAST);
	new SetDateFastWindow(&_set_date_desc, window_number, parent, initial_date, min_date, max_date, caption_text, step_sizes, step_size_labels, callback);
}

void UpdateSetDateFastWindow(Date date, const char* caption_text, SetDateFastCallback *callback)
{
	SetDateFastWindow *window = (SetDateFastWindow*)FindWindowByClass(WC_SET_DATE_FAST);
	if (window != NULL) {
		window->SetData(date, caption_text, callback);
	}
}
