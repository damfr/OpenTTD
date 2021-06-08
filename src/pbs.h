/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file pbs.h PBS support routines */

#ifndef PBS_H
#define PBS_H

#include "tile_type.h"
#include "direction_type.h"
#include "track_type.h"
#include "vehicle_type.h"

TrackBits GetReservedTrackbits(ExtendedTileIndex t);

void SetRailStationPlatformReservation(ExtendedTileIndex start, DiagDirection dir, bool b);

bool TryReserveRailTrack(ExtendedTileIndex tile, Track t, bool trigger_stations = true);
void UnreserveRailTrack(ExtendedTileIndex tile, Track t);

/** This struct contains information about the end of a reserved path. */
struct PBSTileInfo {
	ExtendedTileIndex tile;      ///< Tile the path ends, INVALID_TILE if no valid path was found.
	Trackdir  trackdir;  ///< The reserved trackdir on the tile.
	bool      okay;      ///< True if tile is a safe waiting position, false otherwise.

	/**
	 * Create an empty PBSTileInfo.
	 */
	PBSTileInfo() : tile(INVALID_EXTENDED_TILE), trackdir(INVALID_TRACKDIR), okay(false) {}

	/**
	 * Create a PBSTileInfo with given tile, track direction and safe waiting position information.
	 * @param _t The tile where the path ends.
	 * @param _td The reserved track dir on the tile.
	 * @param _okay Whether the tile is a safe waiting point or not.
	 */
	PBSTileInfo(ExtendedTileIndex _t, Trackdir _td, bool _okay) : tile(_t), trackdir(_td), okay(_okay) {}
};

PBSTileInfo FollowTrainReservation(const Train *v, Vehicle **train_on_res = nullptr);
bool IsSafeWaitingPosition(const Train *v, ExtendedTileIndex tile, Trackdir trackdir, bool include_line_end, bool forbid_90deg = false);
bool IsWaitingPositionFree(const Train *v, ExtendedTileIndex tile, Trackdir trackdir, bool forbid_90deg = false);

Train *GetTrainForReservation(ExtendedTileIndex tile, Track track);

/**
 * Check whether some of tracks is reserved on a tile.
 *
 * @param tile the tile
 * @param tracks the tracks to test
 * @return true if at least on of tracks is reserved
 */
static inline bool HasReservedTracks(ExtendedTileIndex tile, TrackBits tracks)
{
	return (GetReservedTrackbits(tile) & tracks) != TRACK_BIT_NONE;
}

#endif /* PBS_H */
