/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tunnelbridge_map.h Functions that have tunnels and bridges in common */

#ifndef TUNNELBRIDGE_MAP_H
#define TUNNELBRIDGE_MAP_H

#include "bridge_map.h"
#include "tunnel_map.h"


/**
 * Get the direction pointing to the other end.
 *
 * Tunnel: Get the direction facing into the tunnel
 * Bridge: Get the direction pointing onto the bridge
 * @param t The tile to analyze
 * @pre IsTileType(t, MP_TUNNELBRIDGE)
 * @return the above mentioned direction
 */
static inline DiagDirection GetTunnelBridgeDirection(ExtendedTileIndex t)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	return (DiagDirection)GB(GetElevatedTile(t).m5, 0, 2);
}

/**
 * Tunnel: Get the transport type of the tunnel (road or rail)
 * Bridge: Get the transport type of the bridge's ramp
 * @param t The tile to analyze
 * @pre IsTileType(t, MP_TUNNELBRIDGE)
 * @return the transport type in the tunnel/bridge
 */
static inline TransportType GetTunnelBridgeTransportType(ExtendedTileIndex t)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	return (TransportType)GB(GetElevatedTile(t).m5, 2, 2);
}

/**
 * Tunnel: Is this tunnel entrance in a snowy or desert area?
 * Bridge: Does the bridge ramp lie in a snow or desert area?
 * @param t The tile to analyze
 * @pre IsTileType(t, MP_TUNNELBRIDGE)
 * @return true if and only if the tile is in a snowy/desert area
 */
static inline bool HasTunnelBridgeSnowOrDesert(TileIndex t)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	return HasBit(_me[t].m7, 5);
}

/**
 * Tunnel: Places this tunnel entrance in a snowy or desert area, or takes it out of there.
 * Bridge: Sets whether the bridge ramp lies in a snow or desert area.
 * @param t the tunnel entrance / bridge ramp tile
 * @param snow_or_desert is the entrance/ramp in snow or desert (true), when
 *                       not in snow and not in desert false
 * @pre IsTileType(t, MP_TUNNELBRIDGE)
 */
static inline void SetTunnelBridgeSnowOrDesert(TileIndex t, bool snow_or_desert)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	SB(_me[t].m7, 5, 1, snow_or_desert);
}

/**
 * Determines type of the wormhole and returns its other end
 * @param t one end
 * @pre IsTileType(t, MP_TUNNELBRIDGE)
 * @return other end
 */
static inline TileIndex GetOtherTunnelBridgeEnd(TileIndex t)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	return IsTunnel(t) ? GetOtherTunnelEnd(t) : GetOtherBridgeEnd(t);
}

/**
 * Determines the tile following the ramp in the direction @dir
 * The tile returned is adjacent in DiagDirection @a dir, and its height is 
 * either the same (case of flat bridge ramp or tunnel head) or 1 height level above/below (inclined bridge ramp)
 * @param ramp the ramp to check (also works for tunnel heads)
 * @param dir the direction to travel along the ramp
 * @pre IsTileType(ramp, MP_TUNNELBRIDGE)
 * @pre ramp's direction is compatible with @a dir
 * @return the ExtendedTileIndex of the next tile
 */
static inline ExtendedTileIndex GetElevatedRampNextTile(ExtendedTileIndex ramp, DiagDirection dir)
{
	assert(IsTileType(ramp, MP_TUNNELBRIDGE));
	DiagDirection ramp_dir = GetTunnelBridgeDirection(ramp);

	uint ramp_offset = 0;
	if (IsBridgeTile(ramp) && !HasBridgeFlatRamp(ramp)) ramp_offset = 1;

	ExtendedTileIndex next_tile(ramp.index + TileOffsByDiagDir(dir), ramp.height, EL_GROUND);

	if (ramp_dir == dir) {
		/* We are going on the ramp, possibly going up */
		next_tile.height += ramp_offset;
		next_tile.flags = EL_ELEVATED; //TODO elevated tunnels
	} else if (ramp_dir == ReverseDiagDir(dir)) {
		/* We are leaving a ramp, possibly going down */
		next_tile.height -= ramp_offset;
		if (IsIndexGroundTile(next_tile)) next_tile.flags = EL_GROUND;
	} else {
		NOT_REACHED();
	}
	return next_tile;
}


/**
 * Get the reservation state of the rail ramp/tunnel head
 * @pre IsTileType(t, MP_TUNNELBRIDGE) && GetTunnelBridgeTransportType(t) == TRANSPORT_RAIL
 * @param t the tile
 * @return reservation state
 */
static inline bool HasTunnelBridgeReservation(ExtendedTileIndex t)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	assert(GetTunnelBridgeTransportType(t) == TRANSPORT_RAIL);
	return HasBit(GetElevatedTile(t).m5, 4);
}

/**
 * Set the reservation state of the rail ramp/tunnel head
 * @pre IsTileType(t, MP_TUNNELBRIDGE) && GetTunnelBridgeTransportType(t) == TRANSPORT_RAIL
 * @param t the tile
 * @param b the reservation state
 */
static inline void SetTunnelBridgeReservation(ExtendedTileIndex t, bool b)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	assert(GetTunnelBridgeTransportType(t) == TRANSPORT_RAIL);
	SB(GetElevatedTile(t).m5, 4, 1, b ? 1 : 0);
}

/**
 * Get the reserved track bits for a rail tunnel/bridge
 * @pre IsTileType(t, MP_TUNNELBRIDGE) && GetTunnelBridgeTransportType(t) == TRANSPORT_RAIL
 * @param t the tile
 * @return reserved track bits
 */
static inline TrackBits GetTunnelBridgeReservationTrackBits(ExtendedTileIndex t)
{
	return HasTunnelBridgeReservation(t) ? DiagDirToDiagTrackBits(GetTunnelBridgeDirection(t)) : TRACK_BIT_NONE;
}

#endif /* TUNNELBRIDGE_MAP_H */
