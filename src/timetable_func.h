/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timetable_func.h Functions related to the timetable GUIs */

#ifndef TIMETABLE_FUNC_H
#define TIMETABLE_FUNC_H

#include "depot_type.h"
#include "station_type.h"
#include "vehicle_type.h"
#include "window_type.h"

void ShowStationTimetableWindow(StationID station_id, bool departure, bool arrival);
void ShowWaypointTimetableWindow(StationID station_id, bool departure, bool arrival);
void ShowDepotTimetableWindow(DepotID depot_id, bool departure, bool arrival, VehicleType vehicle_type);

#endif /* TIMETABLE_FUNC_H */
