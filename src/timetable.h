/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timetable.h Functions related to time tabling. */

#ifndef TIMETABLE_H
#define TIMETABLE_H

#include "date_type.h"
#include "vehicle_type.h"

struct Order;
struct OrderList;

void ShowTimetableWindow(const Vehicle *v);

void UpdateSharedVehiclesTimetableData(OrderList *o);
void UpdateVehicleStartTimes(Vehicle *vehicle);

bool IsOrderTimetableValid(const Vehicle *vehicle, const Order *order);

#endif /* TIMETABLE_H */
