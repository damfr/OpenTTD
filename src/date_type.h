/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file date_type.h Types related to the dates in OpenTTD. */

#ifndef DATE_TYPE_H
#define DATE_TYPE_H


typedef int32  Date;      ///< The type to store our dates in
typedef uint16 DateFract; ///< The fraction of a date we're in, i.e. the number of ticks since the last date changeover
typedef int32  Ticks;     ///< The type to store ticks in

typedef int32  Year;  ///< Type for the year, note: 0 based, i.e. starts at the year 0.
typedef uint8  Month; ///< Type for the month, note: 0 based, i.e. 0 = January, 11 = December.
typedef uint8  Day;   ///< Type for the day of the month, note: 1 based, first day of a month is 1.

/**
 * 1 day is 74 ticks; _date_fract used to be uint16 and incremented by 885. On
 *                    an overflow the new day begun and 65535 / 885 = 74.
 * 1 tick is approximately 30 ms.
 * 1 day is thus about 2 seconds (74 * 30 = 2220) on a machine that can run OpenTTD normally
 */
static const int DAY_TICKS         =  74; ///< ticks per day
static const int DAYS_IN_YEAR      = 365; ///< days per year
static const int DAYS_IN_LEAP_YEAR = 366; ///< sometimes, you need one day more...

static const int STATION_RATING_TICKS     = 185; ///< cycle duration for updating station rating
static const int STATION_ACCEPTANCE_TICKS = 250; ///< cycle duration for updating station acceptance
static const int STATION_LINKGRAPH_TICKS  = 504; ///< cycle duration for cleaning dead links
static const int CARGO_AGING_TICKS        = 185; ///< cycle duration for aging cargo
static const int INDUSTRY_PRODUCE_TICKS   = 256; ///< cycle duration for industry production
static const int TOWN_GROWTH_TICKS        = 70;  ///< cycle duration for towns trying to grow. (this originates from the size of the town array in TTD
static const int INDUSTRY_CUT_TREE_TICKS  = INDUSTRY_PRODUCE_TICKS * 2; ///< cycle duration for lumber mill's extra action


/*
 * ORIGINAL_BASE_YEAR, ORIGINAL_MAX_YEAR and DAYS_TILL_ORIGINAL_BASE_YEAR are
 * primarily used for loading newgrf and savegame data and returning some
 * newgrf (callback) functions that were in the original (TTD) inherited
 * format, where '_date == 0' meant that it was 1920-01-01.
 */

/** The minimum starting year/base year of the original TTD */
static const Year ORIGINAL_BASE_YEAR = 1920;
/** The original ending year */
static const Year ORIGINAL_END_YEAR  = 2051;
/** The maximum year of the original TTD */
static const Year ORIGINAL_MAX_YEAR  = 2090;

/** The unit of a Duration, e.g. 4 Days. */
typedef uint8 DurationUnit;
static const uint8 DU_TICKS = 0;
static const uint8 DU_DAYS = 1;
static const uint8 DU_MONTHS = 2;
static const uint8 DU_YEARS = 3;
static const uint8 DU_INVALID = 255;

/* TODO (Issue 7069): Originally the above typedef was an enum.  Unfortunately, I didn´t succeeded in making the Saveload consistency check work with that enum.

enum DurationUnit {
	DU_TICKS = 0,
	DU_DAYS = 1,
	DU_MONTHS = 2,
	DU_YEARS = 3,
	DU_INVALID = 255,
};
*/

/** A Duration, i.e. a time interval given with some unit.
 *  Examples: 751 Ticks, 48 Days, 4 Months, 2 Years.
 *  The idea behind Durations is that they enable calculating with Dates and time intervals without
 *  introducing precision errors due to variable length months.  E.g. 23rd February + 1 Month = 23rd March.
 */
struct Duration {

private:
	int32 GetLengthInOurUnit(const Duration &d) const;

public:
	static const int DAYS_PER_MONTH = 30;
	static const int DAYS_PER_YEAR = 365;
	static const int MONTHS_PER_YEAR = 12;

	/* The saveload code expects these fields public. */
	int32 length;       ///< Length of the Duration, measured in unit
	DurationUnit unit;  ///< Unit of the Duration

	Duration() { this->length = 0; this->unit = DU_INVALID; }
	Duration(int32 length, DurationUnit unit) { this->length = length;	this->unit = unit; }

	bool operator< (const Duration &d) const { return length < GetLengthInOurUnit(d); }
	bool operator<= (const Duration &d) const {return length <= GetLengthInOurUnit(d); }
	bool operator== (const Duration &d) const { return length == GetLengthInOurUnit(d); }
	bool operator>= (const Duration &d) const {return length >= GetLengthInOurUnit(d); }
	bool operator> (const Duration &d) const { return length > GetLengthInOurUnit(d); }
	Duration operator- () { return Duration(-length, unit); }

	int32 GetLengthInTicks() const;
	Date GetLengthAsDate() const;

	inline void Add(Duration d) { this->length += GetLengthInOurUnit(d); }
	inline void AddLength(int32 length) { this->length += length; }
	inline void Subtract(Duration d) { this->length -= GetLengthInOurUnit(d); }
	inline void SubtractLength(int32 length) { this->length -= length; }

	inline void SetLength(int32 length) { this->length = length; }
	inline int32 GetLength() const { return this->length; }
	inline void SetUnit(DurationUnit unit) { this->unit = unit; }
	inline DurationUnit GetUnit() const { return this->unit; }

	inline bool IsInTicks() const { return this->unit == DU_TICKS; }
	inline bool IsInDays() const { return this->unit == DU_DAYS; }
	inline bool IsInMonths() const { return this->unit == DU_MONTHS; }
	inline bool IsInYears() const { return this->unit == DU_YEARS; }
	inline bool IsInvalid() const { return this->unit == DU_INVALID; }

	void PrintToDebug(int level, const char *prefix, const char *postfix = "");
};

/**
 * Calculate the number of leap years till a given year.
 *
 * Each passed leap year adds one day to the 'day count'.
 *
 * A special case for the year 0 as no year has been passed,
 * but '(year - 1) / 4' does not yield '-1' to counteract the
 * '+1' at the end of the formula as divisions round to zero.
 *
 * @param year the year to get the leap years till.
 * @return the number of leap years.
 */
#define LEAP_YEARS_TILL(year) ((year) == 0 ? 0 : ((year) - 1) / 4 - ((year) - 1) / 100 + ((year) - 1) / 400 + 1)

/**
 * Calculate the date of the first day of a given year.
 * @param year the year to get the first day of.
 * @return the date.
 */
#define DAYS_TILL(year) (DAYS_IN_YEAR * (year) + LEAP_YEARS_TILL(year))

/**
 * The offset in days from the '_date == 0' till
 * 'ConvertYMDToDate(ORIGINAL_BASE_YEAR, 0, 1)'
 */
#define DAYS_TILL_ORIGINAL_BASE_YEAR DAYS_TILL(ORIGINAL_BASE_YEAR)

/** The absolute minimum & maximum years in OTTD */
static const Year MIN_YEAR = 0;

/** The default starting year */
static const Year DEF_START_YEAR = 1950;

/**
 * MAX_YEAR, nicely rounded value of the number of years that can
 * be encoded in a single 32 bits date, about 2^31 / 366 years.
 */
static const Year MAX_YEAR  = 5000000;

/** The number of days till the last day */
#define MAX_DAY (DAYS_TILL(MAX_YEAR + 1) - 1)

/**
 * Data structure to convert between Date and triplet (year, month, and day).
 * @see ConvertDateToYMD(), ConvertYMDToDate()
 */
struct YearMonthDay {
	Year  year;   ///< Year (0...)
	Month month;  ///< Month (0..11)
	Day   day;    ///< Day (1..31)
};

static const Year  INVALID_YEAR  = -1; ///< Representation of an invalid year
static const Date  INVALID_DATE  = -1; ///< Representation of an invalid date
static const Ticks INVALID_TICKS = -1; ///< Representation of an invalid number of ticks

#endif /* DATE_TYPE_H */
