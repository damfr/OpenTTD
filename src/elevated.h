/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file elevated.h Header file for elevated track */

#ifndef ELEVATED_H
#define ELEVATED_H

#include "stdafx.h"
#include "core/multimap.hpp"
#include "core/pool_func.hpp"
#include "landscape.h"
#include "tunnelbridge_map.h"
#include "map_type.h"

// typedef uint32 ElevatedNetworkID;

// struct ElevatedNetwork;
// typedef Pool<ElevatedNetwork, ElevatedNetworkID, 64, 0xFF0000> ElevatedNetworkPool;
// extern ElevatedNetworkPool _elevated_network_pool;

// /** Elevated network of tracks data structure. */
// struct ElevatedNetwork : ElevatedNetworkPool::PoolItem<&_elevated_network_pool> {
// 	BridgeType type;    ///< Type of the bridge
// 	Date build_date;    ///< Date of construction
// 	Town *town;         ///< Town the bridge is built in
// 	uint16 random;      ///< Random bits
// 	//TileIndex heads[2]; ///< Tile where the bridge ramp is located


// 	/** Make sure the bridge isn't zeroed. */
// 	ElevatedNetwork() {}
// 	/** Make sure the right destructor is called as well! */
// 	~ElevatedNetwork() {}

	
// };


typedef MultiMap<TileIndex, VirtualElevatedTile> ElevatedIndex;

std::pair<ElevatedIndex::iterator, ElevatedIndex::iterator> GetElevatedTrackIterator(TileIndex tile);

ElevatedIndex::iterator GetElevatedTrackAtHeight(TileIndex tile, Height height);





#endif /* ELEVATED_H */