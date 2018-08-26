/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file order_gui.h Order GUI related symbols to be exported to other files, in particular those used by timetable_gui.cpp and order_gui.cpp */

#ifndef ORDER_GUI_H
#define ORDER_GUI_H

#include "strings_type.h"

/** Definition of "Go To" dropdown for all vehicles except aircrafts, used
 *  both in order_gui and timetable_gui.
 */
static const StringID _order_goto_dropdown[] = {
	STR_ORDER_GO_TO,
	STR_ORDER_GO_TO_NEAREST_DEPOT,
	STR_ORDER_CONDITIONAL,
	STR_ORDER_SHARE,
	INVALID_STRING_ID
};

/** Definition of "Go To" dropdown for aircrafts, used both in order_gui
 *  and timetable_gui.
 */
static const StringID _order_goto_dropdown_aircraft[] = {
	STR_ORDER_GO_TO,
	STR_ORDER_GO_TO_NEAREST_HANGAR,
	STR_ORDER_CONDITIONAL,
	STR_ORDER_SHARE,
	INVALID_STRING_ID
};

Order GetOrderCmdFromTile(const Vehicle *v, TileIndex tile);

#endif
