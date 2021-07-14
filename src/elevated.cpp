/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file elevated.cpp */

#include "stdafx.h"

#include "elevated.h"
#include "cmd_helper.h"
#include "command_func.h"
#include "company_func.h"
#include "company_gui.h"
#include "rail.h"
#include "direction_func.h"
#include "company_base.h"
#include "viewport_func.h"
#include "pathfinder/yapf/yapf_cache.h"
#include "bridge_map.h"
#include "tunnelbridge_map.h"


//TODO elevated : move this to a more sensible location
/**
 * Build an ExtendedTileIndex from a ground tile index, setting height at the height of the tile
 * @param ground_index index of the ground tile
 */
ExtendedTileIndex::ExtendedTileIndex(TileIndex ground_index) 
	:index(ground_index), height(0), flags(EL_GROUND)
{
	if (ground_index < MapSize()) this->height = TileHeight(ground_index);
}

ExtendedTileIndex::ExtendedTileIndex(TileIndex ground_index, Height height_param)
	:  index(ground_index), height(height_param), flags(EL_GROUND)
{
	if (ground_index < MapSize()) {
		if (this->height > GetTileMaxZ(this->index)) {
			this->flags = EL_ELEVATED;
		} else if (this->height < GetTileZ(this->index)) {
			this->flags = EL_TUNNEL;
		}
	}
}

/**
 * Checks wether a given height is a ground tile or not
 */
static inline bool IsGroundHeight(TileIndex tile, Height height)
{
	return IsInsideMM(height, GetTileZ(tile), GetTileMaxZ(tile)+1);
}

bool ExtendedTileIndex::operator==(ExtendedTileIndex other_tile) const
{
	if (this->index != other_tile.index) return false;
	if (this->index == INVALID_TILE) {
		return true;
	}
	if (IsIndexGroundTile(*this)) {
		return IsGroundHeight(this->index, other_tile.height);
	} else {
		return this->height == other_tile.height;
	}
}

bool ExtendedTileIndex::IsValid() const
{
	if (!(this->index < MapSize())) return false;
	if (this->flags == EL_GROUND) return true;
	else {
		return HasElevatedTrackAtHeight(this->index, this->height);
	}
}


/**
 * Move an ExtendedTileIndex by one tile in the given DiagDirection
 * @param dir the direction to move into
 * @return true if the tile we moved to exists, false otherwise (case of a non-existing elevated tile)
 */
bool ExtendedTileIndex::MoveByDiagDir(DiagDirection dir)
{
	if (IsTileType(*this, MP_TUNNELBRIDGE)) {
		if (IsTunnel(*this) ) {

		}
	} 
		/* We are on a bridge inclined ramp. Change height */

	this->index += TileOffsByDiagDir(dir);
	if (this->flags == EL_GROUND) {
		return true;
	} else {
		if (IsGroundHeight(this->index, this->height)) {
			/* We have reached ground. Check for a flat bridge ramp or tunnel head */
			if (IsTileType(this->index, MP_TUNNELBRIDGE) && GetTunnelBridgeDirection(this->index) == ReverseDiagDir(dir)
					&& (IsTunnel(this->index) || HasBridgeFlatRamp(this->index))) {
				/* We have a correctly aligned flat bridge ramp or tunnel head.
				   We are now on the ground */
				this->flags = EL_GROUND;
				return true;
			} else {
				/* We reached ground without a ramp : not a valid move */
				return false;
			}
		} else {
			/* We are still not on the ground after moving */
			if (HasElevatedTrackAtHeight(this->index, this->height)) {
				/* We can carry on straight to the next tile, no ramp */
				return true;
			} else {
				/* 
			}
		}
	}
}


ElevatedIndex _elevated_index; ///< A multimap indexed by TileIndex holding elevated tile data


/**
 * Get the range of elevated tracks abobe at a specific tile
 * @param tile the direction to search in
 */
std::pair<ElevatedIndex::iterator, ElevatedIndex::iterator> GetElevatedTrackIterator(TileIndex tile)
{
    return _elevated_index.equal_range(tile);
}

ElevatedIndex::iterator GetElevatedTrackAtHeight(TileIndex tile, Height height)
{
    std::pair<ElevatedIndex::iterator, ElevatedIndex::iterator> range = GetElevatedTrackIterator(tile);
    for (ElevatedIndex::iterator it = range.first; it != range.second; ++it) {
        if (it->tile.height == height)
            return it;
    }
	NOT_REACHED();
}


bool HasElevatedTrack(TileIndex tile)
{
	auto pair = _elevated_index.equal_range(tile);
	return pair.first != pair.second; //TODO better implementation
	/*
    std::pair<ElevatedIndex::iterator, ElevatedIndex::iterator> range = GetElevatedTrackIterator(tile.index);
    for (ElevatedIndex::iterator it = range.first; it != range.second; ++it) {
        if (it->tile.height == tile.height)
            return true;
    }
    return false;*/
}

bool HasElevatedTrackAtHeight(TileIndex tile, Height height)
{
	auto pair = _elevated_index.equal_range(tile);

    std::pair<ElevatedIndex::iterator, ElevatedIndex::iterator> range = GetElevatedTrackIterator(tile);
    for (ElevatedIndex::iterator it = range.first; it != range.second; ++it) {
        if (it->tile.height == height)
            return true;
    }
    return false;
}

/**
 * Get a reference to the Tile struct at a given height : 
 * if tile.height == GROUND_HEIGHT, get the tile reference from the tile array
 * otherwise get it from the multimap
 */
Tile& GetElevatedTile(ExtendedTileIndex tile)
{
    if (IsIndexGroundTile(tile)) //TODO elevated : improve performance 
        return _m[tile.index];
    else {
        return GetElevatedTrackAtHeight(tile.index, tile.height)->tile;
    }
}

/**
 * Get a reference to the TileExtended struct at a given height : 
 * if height == GROUND_HEIGHT, get the tile reference from the tile array
 * otherwise get it from the multimap
 */
TileExtended& GetElevatedTileExt(ExtendedTileIndex tile)
{
    if (IsIndexGroundTile(tile)) //TODO elevated : improve performance 
        return _me[tile.index];
    else {
        return GetElevatedTrackAtHeight(tile.index, tile.height)->ext;
    }
}

/**
 * Insert into the mutimap a given elevated tile
 */
void InsertElevatedTile(ExtendedTileIndex tile)
{
    VirtualElevatedTile tile_data;
    tile_data.tile.height = tile.height;
    _elevated_index.Insert(tile.index, tile_data);
}



/**
 * Gets an adjacent ExtendedTileIndex moved alon DiagDirection dir
 * If @a tile is a ground tile, then update the height to still be on the ground.
 * Otherwise (if @a tile is elevated or underground) stay at the same height
 * @param tile the start tile
 * @param dir the DiagDirection to move along
 * @return the new ExtendedTileIndex
 */
ExtendedTileIndex ExtendedTileAddByDiagDirFollowGround(ExtendedTileIndex tile, DiagDirection dir)
{
	//if (IsIndexGroundTile(tile)) {
		//ExtendedTileIndex new_tile(tile.index + TileOffsByDiagDir(dir));
		//new_tile.height = TileHeight(tile.index);
	//} else {
	//	return ExtendedTileIndex(tile.index + TileOffsByDiagDir(dir), tile.height);
	//}
	if (tile.flags == EL_GROUND) {
		return ExtendedTileIndex(tile.index + TileOffsByDiagDir(dir));
	} else {
		ExtendedTileIndex new_tile = ExtendedTileIndex(tile.index + TileOffsByDiagDir(dir), tile.height, tile.flags);
		if ((IsInsideMM(new_tile.height, GetTileZ(new_tile.index), GetTileMaxZ(new_tile.index)+1)))
			new_tile.flags = EL_GROUND;
		return new_tile;
	}
}

/**
 * Build an elevated ramp
 * @param tile tile to build the ramp on
 * @param flags type of operation
 * @param p1 railtype
 * @param p2 direction
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdBuildElevatedRamp(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
    CompanyID company = _current_company;

	RailType railtype = INVALID_RAILTYPE;
	RoadType roadtype = INVALID_ROADTYPE;

    BridgeType bridge_type = 1; //TODO

	/* unpack parameters */

	TransportType transport_type = TRANSPORT_RAIL; // TODO : Extract<TransportType, 15, 2>(p2);

	/* type of bridge */
	switch (transport_type) {
		/*case TRANSPORT_ROAD:
			roadtype = Extract<RoadType, 8, 6>(p2);
			if (!ValParamRoadType(roadtype)) return CMD_ERROR;
			break;
        */
		case TRANSPORT_RAIL:
			railtype = Extract<RailType, 0, 6>(p1);
			if (!ValParamRailtype(railtype)) return CMD_ERROR;
			break;
        /*
		case TRANSPORT_WATER:
			break;
        */
		default:
			/* Airports don't have bridges. */
			return CMD_ERROR;
	}

    DiagDirection dir = Extract<DiagDirection, 0, 2>(p2);
    //Axis axis = DiagDirToAxis(dir);



	int z_tile;
	Slope tileh = GetTileSlope(tile, &z_tile);


	CommandCost terraform_cost = CheckBridgeSlope(BRIDGE_PIECE_NORTH, DiagDirToAxis(dir), &tileh, &z_tile);

	CommandCost cost(EXPENSES_CONSTRUCTION);
	Owner owner;
	bool is_new_owner;
	RoadType road_rt = INVALID_ROADTYPE;
	RoadType tram_rt = INVALID_ROADTYPE;

    if (false) {} //TODO Replace existing ramp
    else {
		/* Build a new ramp. */

		bool allow_on_slopes = (_settings_game.construction.build_on_slopes && transport_type != TRANSPORT_WATER);

		/* Try and clear the start landscape */
		CommandCost ret = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
		if (ret.Failed()) return ret;
		cost = ret;

		if (terraform_cost.Failed() || (terraform_cost.GetCost() != 0 && !allow_on_slopes)) return_cmd_error(STR_ERROR_LAND_SLOPED_IN_WRONG_DIRECTION);
		cost.AddCost(terraform_cost);

        /* TODO
		if (IsBridgeAbove(tile)) {
            TileIndex north_head = GetNorthernBridgeEnd(tile);

            if (direction == GetBridgeAxis(tile)) return_cmd_error(STR_ERROR_MUST_DEMOLISH_BRIDGE_FIRST);

            if (z_start + 1 == GetBridgeHeight(north_head)) {
                return_cmd_error(STR_ERROR_MUST_DEMOLISH_BRIDGE_FIRST);
            }
		} */
		

		owner = company;
		is_new_owner = true;
	}


    //TODO road
	bool hasroad = false; //road_rt != INVALID_ROADTYPE;
	bool hastram = false; // tram_rt != INVALID_ROADTYPE;
	/*
    if (transport_type == TRANSPORT_ROAD) {
		if (RoadTypeIsRoad(roadtype)) road_rt = roadtype;
		if (RoadTypeIsTram(roadtype)) tram_rt = roadtype;
	}*/

	/* do the drill? */
	if (flags & DC_EXEC) {
		Company *c = Company::GetIfValid(company);
		switch (transport_type) {
			case TRANSPORT_RAIL:
				/* Add to company infrastructure count if required. */
				if (is_new_owner && c != nullptr) c->infrastructure.rail[railtype] += TUNNELBRIDGE_TRACKBIT_FACTOR;
				MakeRailBridgeRamp(tile, owner, bridge_type, dir, railtype);
				
				//SetTunnelBridgeReservation(tile, false); //TODO PBS reservations
				break;

            //TODO Not implemented
			case TRANSPORT_ROAD: 
			case TRANSPORT_WATER:
			default:
				NOT_REACHED();
		}

		/* Mark all tiles dirty */
		//MarkBridgeDirty(tile_start, tile_end, AxisToDiagDir(direction), z_start);
        MarkTileDirtyByTile(tile, z_tile + 1);
		DirtyCompanyInfrastructureWindows(company);
	}

	if ((flags & DC_EXEC) && transport_type == TRANSPORT_RAIL) {
		Track track = DiagDirToDiagTrack(dir);
		//AddSideToSignalBuffer(tile, INVALID_DIAGDIR, company);
		YapfNotifyTrackLayoutChange(tile, track);
	}

	return cost;
}



