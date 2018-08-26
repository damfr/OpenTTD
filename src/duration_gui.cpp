/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file duration_gui.cpp Graphical selection of a duration. */

#include "stdafx.h"
#include "debug.h"
#include "string_func.h"
#include "strings_func.h"
#include "date_func.h"
#include "window_func.h"
#include "window_gui.h"
#include "duration_gui.h"
#include "core/geometry_func.hpp"
#include "textbuf_type.h"

#include "widgets/dropdown_type.h"
#include "widgets/duration_widget.h"


void SetDurationWindow::ShowUnitDropDown()
{
	int selected;
	DropDownList list;
	list.emplace_back(new DropDownListStringItem(STR_DURATION_DAYS_UNIT, UDDI_DAYS, false));
	list.emplace_back(new DropDownListStringItem(STR_DURATION_MONTHS_UNIT, UDDI_MONTHS, false));
	list.emplace_back(new DropDownListStringItem(STR_DURATION_YEARS_UNIT, UDDI_YEARS, false));

	if (this->duration.IsInDays()) {
		selected = UDDI_DAYS;
	} else if (this->duration.IsInMonths()) {
		selected = UDDI_MONTHS;
	} else if (this->duration.IsInYears()) {
		selected = UDDI_YEARS;
	} else {
		/* Especially if the duration is invalid, show days by default */
		selected = UDDI_DAYS;
	}

	ShowDropDownList(this, std::move(list), selected, WID_SDU_UNIT_DROPDOWN);
}

void SetDurationWindow::ParseEditBox()
{
	const char* edit_str_buf = query_string.GetText();
	uint64 length = StrEmpty(edit_str_buf) ? this->min_value : strtoul(edit_str_buf, NULL, 10);
	/* We set MAX_LENGTH_LENGTH_INPUT to a sufficiently small number, so this cast can never overflow */
	this->duration.SetLength(length >= this->min_value ? (uint32)length : this->min_value);
	DEBUG(misc, 9, "Parsed length %i", this->duration.GetLength());
}

void SetDurationWindow::ProcessChoose()
{
	ParseEditBox();

	if (this->callback != NULL) this->callback(this->parent, this->duration);
	delete this;
}

void SetDurationWindow::CopyLengthIntoEditbox()
{
	/* Initialize the text edit widget */
	if (this->duration.IsInDays() || this->duration.IsInMonths() || this->duration.IsInYears()) {
		this->query_string.text.Print("%i", this->duration.GetLength());
	} else {
		this->query_string.text.Assign("");
	}
}

void SetDurationWindow::AdjustUnitDropDown()
{
	NWidgetCore *dropdown_widget = this->GetWidget<NWidgetCore>(WID_SDU_UNIT_DROPDOWN);
	if (this->duration.IsInDays()) {
		dropdown_widget->widget_data = STR_DURATION_DAYS_UNIT;
	} else if (this->duration.IsInMonths()) {
		dropdown_widget->widget_data = STR_DURATION_MONTHS_UNIT;
	} else if (this->duration.IsInYears()) {
		dropdown_widget->widget_data = STR_DURATION_YEARS_UNIT;
	} else {
		dropdown_widget->widget_data = STR_DURATION_DAYS_UNIT;
	}
}


SetDurationWindow::SetDurationWindow(WindowDesc *desc, WindowNumber window_number, Window *parent, Duration initial_duration, bool allow_zero, StringID caption, SetDurationCallback *callback)
			: Window(desc), query_string(MAX_LENGTH_LENGTH_INPUT * MAX_CHAR_LENGTH, MAX_LENGTH_LENGTH_INPUT)
{
	this->parent = parent;
	this->caption = caption;
	this->callback = callback;
	this->duration = initial_duration;
	this->min_value = allow_zero ? 0 : 1;

	if (!this->duration.IsInDays() && !this->duration.IsInMonths() && !this->duration.IsInYears()) {
		this->duration.SetUnit(DU_DAYS);
	}

	this->InitNested(window_number);
	this->querystrings[WID_SDU_LENGTH_EDITBOX] = &this->query_string;
	this->query_string.ok_button = WID_SDU_CHOOSE_BUTTON;
	this->query_string.cancel_button = QueryString::ACTION_CLEAR;
	this->query_string.text.afilter = CS_NUMERAL;

	NWidgetCore *caption_widget = this->GetWidget<NWidgetCore>(WID_SDU_CAPTION);
	caption_widget->SetDataTip(caption, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS);

	this->CopyLengthIntoEditbox();
	this->AdjustUnitDropDown();

	this->SetFocusedWidget(WID_SDU_LENGTH_EDITBOX);
	this->LowerWidget(WID_SDU_LENGTH_EDITBOX);
}

void SetDurationWindow::OnClick(Point pt, int widget, int click_count)
{
	switch (widget) {
		case WID_SDU_FAST_SMALLER_BUTTON: {
			if (this->duration.GetLength() >= FAST_STEP_SIZE + this->min_value) {
				this->duration.SetLength(this->duration.GetLength() - FAST_STEP_SIZE);
			} else {
				this->duration.SetLength(this->min_value);
			}
			CopyLengthIntoEditbox();
			this->SetFocusedWidget(WID_SDU_LENGTH_EDITBOX);
			this->LowerWidget(WID_SDU_LENGTH_EDITBOX);
			this->InvalidateData();
			break;
		}
		case WID_SDU_SMALLER_BUTTON: {
			if (this->duration.GetLength() >= SLOW_STEP_SIZE + this->min_value) {
				this->duration.SetLength(this->duration.GetLength() - SLOW_STEP_SIZE);
			} else {
				this->duration.SetLength(this->min_value);
			}
			CopyLengthIntoEditbox();
			this->SetFocusedWidget(WID_SDU_LENGTH_EDITBOX);
			this->LowerWidget(WID_SDU_LENGTH_EDITBOX);
			this->InvalidateData();
			break;
		}
		case WID_SDU_BIGGER_BUTTON: {
			if (this->duration.GetLength() <= MAX_LENGTH - SLOW_STEP_SIZE) {
				this->duration.SetLength(this->duration.GetLength() + SLOW_STEP_SIZE);
			} else {
				this->duration.SetLength(MAX_LENGTH);
			}
			CopyLengthIntoEditbox();
			this->SetFocusedWidget(WID_SDU_LENGTH_EDITBOX);
			this->LowerWidget(WID_SDU_LENGTH_EDITBOX);
			this->InvalidateData();
			break;
		}
		case WID_SDU_FAST_BIGGER_BUTTON: {
			if (this->duration.GetLength() <= MAX_LENGTH - FAST_STEP_SIZE) {
				this->duration.SetLength(this->duration.GetLength() + FAST_STEP_SIZE);
			} else {
				this->duration.SetLength(MAX_LENGTH);
			}
			CopyLengthIntoEditbox();
			this->SetFocusedWidget(WID_SDU_LENGTH_EDITBOX);
			this->LowerWidget(WID_SDU_LENGTH_EDITBOX);
			this->InvalidateData();
			break;
		}

		case WID_SDU_UNIT_DROPDOWN:
			ShowUnitDropDown();
			break;

		case WID_SDU_CHOOSE_BUTTON:
			ProcessChoose();
			break;
		case WID_SDU_LENGTH_EDITBOX: {
			this->SetFocusedWidget(WID_SDU_LENGTH_EDITBOX);
			this->LowerWidget(WID_SDU_LENGTH_EDITBOX);
			break;
		}
	}
}

void SetDurationWindow::OnDropdownSelect(int widget, int index)
{
	switch (widget) {
		case WID_SDU_UNIT_DROPDOWN:
		    if (index == UDDI_DAYS) {
				this->duration.SetUnit(DU_DAYS);
			} else if (index == UDDI_MONTHS) {
				this->duration.SetUnit(DU_MONTHS);
			} else if (index == UDDI_YEARS) {
				this->duration.SetUnit(DU_YEARS);
			} else {
				/* If this happens, we have filled some invalid value into the dropdown => We have a bug! */
				assert(false);
			}
			break;
	}
	AdjustUnitDropDown();
	this->SetFocusedWidget(WID_SDU_LENGTH_EDITBOX);
	this->LowerWidget(WID_SDU_LENGTH_EDITBOX);
	this->SetDirty();
}

EventState SetDurationWindow::OnHotkey(int hotkey)
{
	switch (hotkey) {
		default:
			return ES_NOT_HANDLED;
	}

	return ES_HANDLED;
}

void SetDurationWindow::OnEditboxChanged(int widget)
{
	this->ParseEditBox();
}

void SetDurationWindow::OnPaint()
{
	this->DrawWidgets();
}

void SetDurationWindow::UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
{
	switch (widget) {
		case WID_SDU_CAPTION: {
			SetDParam(0, this->caption);
			Dimension d = GetStringBoundingBox(STR_JUST_STRING);
			size->width = d.width + padding.width;
			break;
		}

		case WID_SDU_UNIT_DROPDOWN: {
			Dimension d = GetStringBoundingBox(STR_DURATION_DAYS_UNIT);
			d = maxdim(d, GetStringBoundingBox(STR_DURATION_MONTHS_UNIT));
			d = maxdim(d, GetStringBoundingBox(STR_DURATION_YEARS_UNIT));
			size->width = d.width + padding.width;
			break;
		}
	}
}

/** Widgets for the date setting window. */
static const NWidgetPart _nested_set_duration_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_SDU_CAPTION), SetDataTip(STR_DURATION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(NWID_VERTICAL), SetPIP(6, 6, 6),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(6, 6, 6),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SDU_FAST_SMALLER_BUTTON), SetMinimalSize(20, 12), SetDataTip(STR_DURATION_FAST_SMALLER_BUTTON_CAPTION, STR_DURATION_FAST_SMALLER_BUTTON_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SDU_SMALLER_BUTTON), SetMinimalSize(20, 12), SetDataTip(STR_DURATION_SMALLER_BUTTON_CAPTION, STR_DURATION_SMALLER_BUTTON_TOOLTIP),
				NWidget(WWT_EDITBOX, COLOUR_GREY, WID_SDU_LENGTH_EDITBOX), SetMinimalSize(80, 12), SetResize(1, 0), SetFill(1, 0), SetPadding(2, 2, 2, 2),
					SetDataTip(STR_DURATION_ENTER_LENGTH_OSKTITLE, STR_DURATION_ENTER_LENGTH_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SDU_BIGGER_BUTTON), SetMinimalSize(20, 12), SetDataTip(STR_DURATION_BIGGER_BUTTON_CAPTION, STR_DURATION_BIGGER_BUTTON_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SDU_FAST_BIGGER_BUTTON), SetMinimalSize(20, 12), SetDataTip(STR_DURATION_FAST_BIGGER_BUTTON_CAPTION, STR_DURATION_FAST_BIGGER_BUTTON_TOOLTIP),
				NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_SDU_UNIT_DROPDOWN), SetFill(1, 0), SetDataTip(STR_EMPTY, STR_DURATION_UNIT_DROPDOWN_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_SPACER), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SDU_CHOOSE_BUTTON), SetMinimalSize(80, 12), SetDataTip(STR_DURATION_CHOOSE_DURATION_BUTTON_CAPTION, STR_DURATION_CHOOSE_DURATION_BUTTON_TOOLTIP),
				NWidget(NWID_SPACER), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer()
};

/** Description of the date setting window. */
static WindowDesc _set_duration_desc(
	WDP_CENTER, NULL, 0, 0,
	WC_SET_DURATION, WC_NONE,
	0,
	_nested_set_duration_widgets, lengthof(_nested_set_duration_widgets)
);

/**
 * Create the new 'set date' window
 * @param window_number number for the window
 * @param parent the parent window, i.e. if this closes we should close too
 * @param initial_duration the initial duration to show
 * @param allow_zero if true, the window allows the input of values >= zero days/months/years, 
 *                   if false, it allows the input of values >= one day/month/year
 * @param min_year the minimum year to show in the year dropdown
 * @param max_year the maximum year (inclusive) to show in the year dropdown
 * @param callback the callback to call once a date has been selected
 */
void ShowSetDurationWindow(Window *parent, int window_number, Duration initial_duration, bool allow_zero, StringID caption, SetDurationCallback *callback)
{
	DeleteWindowByClass(WC_SET_DURATION);
	new SetDurationWindow(&_set_duration_desc, window_number, parent, initial_duration, allow_zero, caption, callback);
}

