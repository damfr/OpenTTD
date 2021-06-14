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


//TODO elevated : move this to a more sensible location
/**
 * Build an ExtendedTileIndex from a ground tile index, setting height at the height of the tile
 * @param ground_index index of the ground tile
 */
ExtendedTileIndex::ExtendedTileIndex(TileIndex ground_index) 
	:index(ground_index), height(TileHeight(ground_index)) 
{}

bool ExtendedTileIndex::operator==(ExtendedTileIndex other_tile) const
{
	if (this->index != other_tile.index) return;
	if (IsIndexGroundTile(*this)) {
		return IsInsideMM(other_tile.height, GetTileZ(this->index), GetTileMaxZ(this->index)+1);
	} else {
		return this->height == other_tile.height;
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

bool HasElevatedTrack(ExtendedTileIndex tile)
{
    std::pair<ElevatedIndex::iterator, ElevatedIndex::iterator> range = GetElevatedTrackIterator(tile.index);
    for (ElevatedIndex::iterator it = range.first; it != range.second; ++it) {
        if (it->tile.height == tile.height)
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
	if (IsIndexGroundTile(tile)) {
		ExtendedTileIndex new_tile(tile.index + TileOffsByDiagDir(dir));
		new_tile.height = TileHeight(tile.index);
		return new_tile;
	} else {
		return ExtendedTileIndex(tile.index + TileOffsByDiagDir(dir), tile.height);
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



