/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file base_consist.h Properties for front vehicles/consists. */

#ifndef BASE_CONSIST_H
#define BASE_CONSIST_H

#include "order_type.h"
#include "date_type.h"
#include <string>

/** Various front vehicle properties that are preserved when autoreplacing, using order-backup or switching front engines within a consist. */
struct BaseConsist {
	std::string name;                   ///< Name of vehicle

	/* Used for timetabling. */
	uint32 current_order_time;          ///< How many ticks have passed since this order started.
	int32 lateness_counter;             ///< How many days late (or early if negative) this vehicle is.
	Date timetable_start;               ///< NOSAVE: Start of the timetable, calculated as result of timetable start plus vehicle offset
	Date timetable_end;                 ///< NOSAVE: End of the timetable, i.e. the date timetable_start + timetable_length.
	Duration timetable_offset;          ///< The desired offset of the vehicle relative to the absolute start time of its timetable

	uint16 service_interval;            ///< The interval for (automatic) servicing; either in days or %.

	VehicleOrderID cur_real_order_index;///< The index to the current real (non-implicit) order
	VehicleOrderID cur_implicit_order_index;///< The index to the current implicit order
	VehicleOrderID autofill_start_order_index; ///< If autofill for this vehicle is currently active (see vehicle_flags), the order where it started

	uint16 vehicle_flags;               ///< Used for gradual loading and other miscellaneous things (@see VehicleFlags enum)

	virtual ~BaseConsist() {}

	void CopyConsistPropertiesFrom(const BaseConsist *src);

	inline void SetTimetableOffset(Duration offset)	{ this->timetable_offset = offset; }
	inline Duration GetTimetableOffset() const { return this->timetable_offset; }
	inline void SetTimetableStart(Date timetable_start) { this->timetable_start = timetable_start; }
	inline Date GetTimetableStart() const { return this->timetable_start; }
	inline void SetTimetableEnd(Date timetable_end) { this->timetable_end = timetable_end; }
	inline Date GetTimetableEnd() const { return this->timetable_end; }
};

#endif /* BASE_CONSIST_H */
