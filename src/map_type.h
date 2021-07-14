/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file map_type.h Types related to maps. */

#ifndef MAP_TYPE_H
#define MAP_TYPE_H

#include "core/bitmath_func.hpp"
#include "tile_type.h"

/**
 * Data that is stored per tile. Also used TileExtended for this.
 * Look at docs/landscape.html for the exact meaning of the members.
 */
struct Tile {
	byte   type;        ///< The type (bits 4..7), bridges (2..3), rainforest/desert (0..1)
	byte   height;      ///< The height of the northern corner.
	uint16 m2;          ///< Primarily used for indices to towns, industries and stations
	byte   m1;          ///< Primarily used for ownership information
	byte   m3;          ///< General purpose
	byte   m4;          ///< General purpose
	byte   m5;          ///< General purpose
};

static_assert(sizeof(Tile) == 8);

/**
 * Data that is stored per tile. Also used Tile for this.
 * Look at docs/landscape.html for the exact meaning of the members.
 */
struct TileExtended {
	byte m6;   ///< General purpose
	byte m7;   ///< Primarily used for newgrf support
	uint16 m8; ///< General purpose
};

/**
 * An offset value between to tiles.
 *
 * This value is used for the difference between
 * to tiles. It can be added to a tileindex to get
 * the resulting tileindex of the start tile applied
 * with this saved difference.
 *
 * @see TileDiffXY(int, int)
 */
typedef int32 TileIndexDiff;

/**
 * A pair-construct of a TileIndexDiff.
 *
 * This can be used to save the difference between to
 * tiles as a pair of x and y value.
 */
struct TileIndexDiffC {
	int16 x;        ///< The x value of the coordinate
	int16 y;        ///< The y value of the coordinate
};


struct VirtualElevatedTile {
    Tile tile;
    TileExtended ext;
};


typedef byte Height;
//static const Height GROUND_HEIGHT = -1;

enum ElevatedFlags {
	EL_GROUND = 0,
	EL_ELEVATED,
	EL_TUNNEL,
};

struct ExtendedTileIndex {
    TileIndex index;
    Height height;
	ElevatedFlags flags;


    ExtendedTileIndex(TileIndex ground_index = INVALID_TILE);
	ExtendedTileIndex(TileIndex ground_index, Height height_param);
	ExtendedTileIndex(TileIndex ground_index, Height height_param, ElevatedFlags flags_p) 
		: index(ground_index), height(height_param), flags(flags_p) {}

	//void operator=(TileIndex ground_index)

	inline ExtendedTileIndex operator+(TileIndexDiff diff) const { return ExtendedTileIndex(this->index + diff, this->height, this->flags); }
	inline ExtendedTileIndex operator-(TileIndexDiff diff) const { return ExtendedTileIndex(this->index - diff, this->height, this->flags); }

	inline void operator+=(TileIndexDiff diff) { this->index += diff; }
	inline void operator-=(TileIndexDiff diff) { this->index -= diff; }

	bool operator==(ExtendedTileIndex other_tile) const; ///< Checks equality considering equal two ground tiles with different height
	inline bool operator!=(ExtendedTileIndex other_tile) const { return !(*this == other_tile); }

	bool IsValid() const;

	bool MoveByDiagDir(DiagDirection dir);
	

	/**
	 * Packs an ExtendedTileIndex in the first 32+8+2=42 bits of an uint64
	 * @return the packed ExtendedTileIndex
	 */
	uint64 Pack() const
	{
		return ((uint64) index) | (((uint64) height) << 32) | ((uint64) flags << 40);
	}

	/**
	 * Unpacks an ExtendedTileIndex from the first 32+8=40 bits of an uint64
	 * @return the unpacked ExtendedTileIndex
	 */
	static ExtendedTileIndex Unpack(uint64 packed) 
	{
		return ExtendedTileIndex(GB(packed, 0, 32), GB(packed, 32, 8), (ElevatedFlags) GB(packed, 40, 2));
	}
};


static const ExtendedTileIndex INVALID_EXTENDED_TILE = ExtendedTileIndex(INVALID_TILE, 0, EL_GROUND);


/** Minimal and maximal map width and height */
static const uint MIN_MAP_SIZE_BITS = 6;                      ///< Minimal size of map is equal to 2 ^ MIN_MAP_SIZE_BITS
static const uint MAX_MAP_SIZE_BITS = 12;                     ///< Maximal size of map is equal to 2 ^ MAX_MAP_SIZE_BITS
static const uint MIN_MAP_SIZE      = 1 << MIN_MAP_SIZE_BITS; ///< Minimal map size = 64
static const uint MAX_MAP_SIZE      = 1 << MAX_MAP_SIZE_BITS; ///< Maximal map size = 4096

/**
 * Approximation of the length of a straight track, relative to a diagonal
 * track (ie the size of a tile side).
 *
 * \#defined instead of const so it can
 * stay integer. (no runtime float operations) Is this needed?
 * Watch out! There are _no_ brackets around here, to prevent intermediate
 * rounding! Be careful when using this!
 * This value should be sqrt(2)/2 ~ 0.7071
 */
#define STRAIGHT_TRACK_LENGTH 7071/10000

/** Argument for CmdLevelLand describing what to do. */
enum LevelMode {
	LM_LEVEL, ///< Level the land.
	LM_LOWER, ///< Lower the land.
	LM_RAISE, ///< Raise the land.
};

#endif /* MAP_TYPE_H */
