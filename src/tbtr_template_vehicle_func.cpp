/* $Id: build_vehicle_gui.cpp 23792 2012-01-12 19:23:00Z yexo $ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tbtr_template_vehicle_func.cpp Various setup and utility functions around template trains. */

#include <map>
#include "stdafx.h"

#include "articulated_vehicles.h"
#include "autoreplace_func.h"
#include "cargoaction.h"
#include "command_func.h"
#include "company_func.h"
#include "core/geometry_type.hpp"
#include "debug.h"
#include "depot_base.h"
#include "gfx_func.h"
#include "newgrf.h"
#include "spritecache.h"
#include "strings_func.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "train.h"
#include "vehicle_func.h"
#include "vehicle_gui.h"
#include "window_func.h"
#include "window_gui.h"

#include "tbtr_template_vehicle.h"
#include "tbtr_template_vehicle_func.h"

Vehicle *vhead, *vtmp;
static const uint MAX_ARTICULATED_PARTS = 100;

/*
 * forward declaration, from train_cmd.cpp
 */
CommandCost CmdSellRailWagon(DoCommandFlag, Vehicle*, uint16, uint32);

/**
 * Fill the given GUITemplateList with all template vehicles defined for the given owner, which are the head of a chain.
 */
void BuildTemplateGuiList(GUITemplateList *list, Scrollbar *vscroll, Owner oid, RailType railtype)
{
	list->Clear();
	const TemplateVehicle *tv;

	FOR_ALL_TEMPLATES(tv) {
		if (tv->owner == oid && (tv->IsPrimaryVehicle() || tv->IsFreeWagonChain()) && TemplateVehicleContainsEngineOfRailtype(tv, railtype))
			*list->Append() = tv;

	}

	list->RebuildDone();
	if (vscroll) vscroll->SetCount(list->Length());
}

/**
 * Calculate the total Money-value of a given template vehicle chain.
 * @param tv: The given template vehicle is treated as the head of a chain. The value is only summed up for this vehicle and all following vehicles in the chain.
 */
Money CalculateOverallTemplateCost(const TemplateVehicle *tv)
{
	Money val = 0;

	for (; tv; tv = tv->Next())
		val += (Engine::Get(tv->engine_type))->GetCost();
	return val;
}

/**
 * Draw a given template vehicle by its stored sprite id and a given position.
 */
void DrawTemplate(const TemplateVehicle *tv, int left, int right, int y)
{
	if ( !tv ) return;

	const TemplateVehicle *t = tv;
	int offset=left;

	while (t) {
		PaletteID pal = GetEnginePalette(t->engine_type, _current_company);
		DrawSprite(t->cur_image, pal, offset, y+12);

		offset += t->image_width;
		t = t->Next();
	}
}

/**
 * Copy relevant values from a (virtual) train onto a template vehicle.
 * @param virt: The train from which to copy the values.
 * @param tmpl_veh: The template for which the values are to be setthat receives the values
 * @param prev: The previous template vehicle in a chain. Used to chain the current template vehicle to it's precedessor.
 */
inline void SetupTemplateVehicleFromVirtual(Train* virt, TemplateVehicle* tmpl_veh, TemplateVehicle* prev)
{
	if (prev) {
		prev->SetNext(tmpl_veh);
		tmpl_veh->SetPrev(prev);
		tmpl_veh->SetFirst(prev->First());
	}
	tmpl_veh->railtype = virt->railtype;
	tmpl_veh->owner = virt->owner;
	tmpl_veh->value = virt->value;

	// set the subtype but also clear the virtual flag while doing it
	tmpl_veh->subtype = virt->subtype & ~(1 << GVSF_VIRTUAL);
	// set the cargo type and capacity
	tmpl_veh->cargo_type = virt->cargo_type;
	tmpl_veh->cargo_subtype = virt->cargo_subtype;
	tmpl_veh->cargo_cap = virt->cargo_cap;

	const GroundVehicleCache *gcache = virt->GetGroundVehicleCache();
	tmpl_veh->max_speed = virt->GetDisplayMaxSpeed();
	tmpl_veh->power = gcache->cached_power;
	tmpl_veh->weight = gcache->cached_weight;
	tmpl_veh->max_te = gcache->cached_max_te / 1000;

	tmpl_veh->spritenum = virt->spritenum;
	VehicleSpriteSeq seq;
	virt->GetImage(DIR_W, EIT_PURCHASE, &seq);
	tmpl_veh->cur_image = seq.seq[0].sprite;
	Point *p = new Point();
	tmpl_veh->image_width = virt->GetDisplayImageWidth(p);
}

/*
 * Create a Virtual Train as a clone from a Train object.
 */
Train* CloneVirtualTrainFromTrain(const Train *clicked)
{
	if ( !clicked ) return 0;
	CommandCost c;
	Train *tmp, *head, *tail;

	head = CmdBuildVirtualRailVehicle(clicked->engine_type);
	if ( !head ) return 0;

	tail = head;
	clicked = clicked->GetNextUnit();
	while ( clicked ) {
		tmp = CmdBuildVirtualRailVehicle(clicked->engine_type);
		if ( tmp ) {
			tmp->cargo_type = clicked->cargo_type;
			tmp->cargo_subtype = clicked->cargo_subtype;
			CmdMoveRailVehicle(0, DC_EXEC, (1<<21) | tmp->index, tail->index, 0);
			tail = tmp;
		}
		clicked = clicked->GetNextUnit();
	}
	return head;
}

/*
 * Create a Template Train as a clone from a Train object.
 */
TemplateVehicle* CloneTemplateVehicleFromTrain(const Train *t)
{
	Train *clicked = Train::Get(t->index);
	if ( !clicked )
		return 0;

	Train *init_clicked = clicked;

	int len = CountVehiclesInChain(clicked);
	if ( !TemplateVehicle::CanAllocateItem(len) )
		return 0;

	TemplateVehicle *tmp, *prev=0;
	for ( ; clicked; clicked=clicked->Next() ) {
		tmp = new TemplateVehicle(clicked->engine_type);
		SetupTemplateVehicleFromVirtual(clicked, tmp, prev);
		prev = tmp;
	}

	tmp->First()->SetRealLength(CeilDiv(init_clicked->gcache.cached_total_length * 10, TILE_SIZE));
	return tmp->First();
}

/*
 * Create a new Template Train as a clone from a Virtual Train.
 * The new template will contain all necessary details that can be extracted
 * from the virtual train.
 */
TemplateVehicle* TemplateVehicleFromVirtualTrain(Train *virt)
{
	if ( !virt )
		return 0;

	Train *init_virt = virt;

	int len = CountVehiclesInChain(virt);
	if ( !TemplateVehicle::CanAllocateItem(len) )
		return 0;

	TemplateVehicle *tmp, *prev=0;
	for ( ; virt; virt=virt->Next() ) {
		tmp = new TemplateVehicle(virt->engine_type);
		SetupTemplateVehicleFromVirtual(virt, tmp, prev);
		prev = tmp;
	}

	tmp->First()->SetRealLength(CeilDiv(init_virt->gcache.cached_total_length * 10, TILE_SIZE));
	return tmp->First();
}

/*
 * Create a Virtual Train corresponding to a given Template Vehicle.
 */
Train* VirtualTrainFromTemplateVehicle(TemplateVehicle *tv)
{
	if ( !tv ) return 0;
	CommandCost c;
	Train *tmp, *head, *tail;

	head = CmdBuildVirtualRailVehicle(tv->engine_type);
	if ( !head ) return 0;

	tail = head;
	tv = tv->GetNextUnit();
	while ( tv ) {
		tmp = CmdBuildVirtualRailVehicle(tv->engine_type);
		if ( tmp ) {
			tmp->cargo_type = tv->cargo_type;
			tmp->cargo_subtype = tv->cargo_subtype;
			CmdMoveRailVehicle(INVALID_TILE, DC_EXEC, (1<<21) | tmp->index, tail->index, 0);
			tail = tmp;
		}
		tv = tv->GetNextUnit();
	}
	return head;
}

/*
 * Return the last part of a Template Vehicle chain
 * This will return really the last part, i.e. even the last part of an
 * articulated vehicle, if chain ends with such an articulated vehicle.
 */
inline TemplateVehicle* Last(TemplateVehicle *chain) {
	if ( !chain ) return 0;
	while ( chain->Next() ) chain = chain->Next();
	return chain;
}

/*
 * Return the last part of a Train
 * This will return really the last part, i.e. even the last part of an
 * articulated vehicle, if chain ends with such an articulated vehicle.
 */
inline Train* Last(Train *chain) {
	if ( !chain ) return 0;
	while ( chain->GetNextUnit() ) chain = chain->GetNextUnit();
	return chain;
}

TemplateVehicle *DeleteTemplateVehicle(TemplateVehicle *todel)
{
	if ( !todel )
		return 0;
	TemplateVehicle *cur = todel;
	delete todel;
	return cur;
}

Train* DeleteVirtualTrain(Train *chain, Train *to_del) {
	if ( chain != to_del ) {
		CmdSellRailWagon(DC_EXEC, to_del, 0, 0);
		return chain;
	}
	else {
		chain = chain->GetNextUnit();
		CmdSellRailWagon(DC_EXEC, to_del, 0, 0);
		return chain;
	}
}

// retrieve template vehicle from templatereplacement that belongs to the given group
/*
 * Find the Template Vehicle for a given vehicle group.
 * The template is looked up by finding the template replacement that is
 * currently set for the given group, if any is set.
 * @param: gid	the ID of the group for which to find the currently used template
 * @return		pointer to the corresponding template vechicle, NULL if no
 * template replacement is defined for the given group.
 */
TemplateVehicle* GetTemplateVehicleByGroupID(GroupID gid) {
	TemplateReplacement *tr;
	// first try to find a templatereplacement issued for the given groupid
	FOR_ALL_TEMPLATE_REPLACEMENTS(tr) {
		if ( tr->Group() == gid )
			return TemplateVehicle::GetIfValid(tr->Template());		// there can be only one
	}
	// if that didn't work, try to find a templatereplacement for ALL_GROUP
	if ( gid != ALL_GROUP )
		FOR_ALL_TEMPLATE_REPLACEMENTS(tr) {
			if ( tr->Group() == ALL_GROUP )
				return TemplateVehicle::GetIfValid(tr->Template());
		}
	// if all failed, just return null
	return 0;
}

/**
 * Check, if a given template consist contains any engine of the given railtype
 */
bool TemplateVehicleContainsEngineOfRailtype(const TemplateVehicle *tv, RailType type)
{
	/* For standard rail engines, allow only those */
	if ( type == RAILTYPE_BEGIN || type == RAILTYPE_RAIL ) {
		while ( tv ) {
		if ( tv->railtype != type )
			return false;
		tv = tv->GetNextUnit();
		}
		return true;
	}
	/* For electrified rail engines, standard wagons or engines are allowed to be included */
	while ( tv ) {
		if ( tv->railtype == type )
			return true;
		tv = tv->GetNextUnit();
	}
	return false;
}

/*
 * Check, whether a given Train object contains another Train
 * The maybe-contained train is not check for complete inclusion, it is rather
 * treated as if it was a one-vehicle chain
 */
bool ChainContainsVehicle(Train *chain, Train *mem) {
	for (; chain; chain=chain->Next())
		if ( chain == mem )
			return true;
	return false;
}

/*
 * Checks, if any vehicle in a given Train contains has a given EngineID
 */
Train* ChainContainsEngine(Train* chain, EngineID eid) {
	for (; chain; chain=chain->GetNextUnit())
		if (chain->engine_type == eid)
			return chain;
	return 0;
}

/*
 * Check, if any train in a given Depot contains a given EngineID
 * @param tile:     the tile of the depot
 * @param eid:      the EngineID to look up
 * @param not_in    this Train will be ignored during the check
 */
Train* DepotContainsEngine(TileIndex tile, EngineID eid, Train *not_in=0) {
	Train *t;
	FOR_ALL_TRAINS(t) {
		// conditions: v is stopped in the given depot, has the right engine and if 'not_in' is given v must not be contained within 'not_in'
 		// if 'not_in' is NULL, no check is needed
		if ( t->tile==tile
				// If the veh belongs to a chain, wagons will not return true on IsStoppedInDepot(), only primary vehicles will
				// in case of t not a primary veh, we demand it to be a free wagon to consider it for replacement
				&& ((t->IsPrimaryVehicle() && t->IsStoppedInDepot()) || t->IsFreeWagon())
				&& t->engine_type==eid
				&& (not_in==0 || ChainContainsVehicle(not_in, t)==0))
			return t;
	}
	return 0;
}

/*
 * Copy some details of one Train onto another
 */
void CopyStatus(Train *from, Train *to) {
	DoCommand(to->tile, from->group_id, to->index, DC_EXEC, CMD_ADD_VEHICLE_GROUP);
	to->cargo_type = from->cargo_type;
	to->cargo_subtype = from->cargo_subtype;

	// swap names
	char *tmp = to->name;
	to->name = from->name;
	from->name = tmp;
}

/*
 * Reset a train's group and name
 */
void NeutralizeStatus(Train *t) {
	DoCommand(t->tile, DEFAULT_GROUP, t->index, DC_EXEC, CMD_ADD_VEHICLE_GROUP);

	t->name = 0;
}

/*
 * Check, if a given train fully matches a template train's engine ids
 * Both chains must have the exact same number of members all of which have the
 * same engine ids.
 */
bool TrainMatchesTemplate(const Train *t, TemplateVehicle *tv) {
	while ( t && tv ) {
		if ( t->engine_type != tv->engine_type )
			return false;
		t = t->GetNextUnit();
		tv = tv->GetNextUnit();
	}
	if ( (t && !tv) || (!t && tv) )
		return false;
	return true;
}

/*
 * Check, if a given train completely matches a template's refit settings
 * This check will still succeed if any of the two given chains has more
 * vehicles than the other.
 */
bool TrainMatchesTemplateRefit(const Train *t, TemplateVehicle *tv)
{
	if ( !tv->refit_as_template )
		return true;

	while ( t && tv ) {
		if ( t->cargo_type != tv->cargo_type || t->cargo_subtype != tv->cargo_subtype )
			return false;
		t = t->GetNextUnit();
		tv = tv->GetNextUnit();
	}
	return true;
}

/*
 * Break up the remainders of a chain after template replacement
 * All primary vehicles in the given chain are moved to form a new chain on the
 * same tile (presumably inside a depot). All following wagons after a train
 * engine are kept with the engine in order to create only as many new chains
 * as necessary.
 */
void BreakUpRemainders(Train *t) {
	while ( t ) {
		Train *move;
		if ( HasBit(t->subtype, GVSF_ENGINE) ) {
			move = t;
			t = t->Next();
			DoCommand(move->tile, move->index, INVALID_VEHICLE, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
			NeutralizeStatus( move );
		}
		else
			t = t->Next();
	}
}

/*
 * Count and return the number of engines in a given train
 */
short CountEnginesInChain(Train *t)
{
	short count = 0;
	for ( ; t; t=t->GetNextUnit() )
		if ( HasBit(t->subtype, GVSF_ENGINE) )
			count++;
	return count;
}

/*
 * Count and return the number of vehicles with a spcific engine id in a train
 */
int countOccurrencesInTrain(Train *t, EngineID eid) {
	int count = 0;
	Train *tmp = t;
	for ( ; tmp; tmp=tmp->GetNextUnit() )
		if ( tmp->engine_type == eid )
			count++;
	return count;
}

/*
 * Count the number of occurrences of a specific engine id in a depot
 * @param tile:     the tile of the depot
 * @param eid:      the engine id to count
 * @param not_in:   ignore this train for the count
 */
int countOccurrencesInDepot(TileIndex tile, EngineID eid, Train *not_in=0) {
	int count = 0;
	Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		// conditions: v is stopped in the given depot, has the right engine and if 'not_in' is given v must not be contained within 'not_in'
 		// if 'not_in' is NULL, no check is needed
		if ( v->tile==tile && v->IsStoppedInDepot() && v->engine_type==eid &&
				(not_in==0 || ChainContainsVehicle(not_in, (Train*)v)==0))
			count++;
	}
	return count;
}

// basically does the same steps as CmdTemplateReplaceVehicle but without actually moving things around
CommandCost CalculateTemplateReplacementCost(Train *incoming) {
	TileIndex tile = incoming->tile;
	TemplateVehicle *tv = GetTemplateVehicleByGroupID(incoming->group_id);
	CommandCost estimate(EXPENSES_NEW_VEHICLES);

	// count for each different eid in the incoming train
	std::map<EngineID, short> unique_eids;
	for ( TemplateVehicle *tmp=tv; tmp; tmp=tmp->GetNextUnit() )
		unique_eids[tmp->engine_type]++;
	std::map<EngineID, short>::iterator it = unique_eids.begin();
	for ( ; it!=unique_eids.end(); it++ ) {
		it->second -= countOccurrencesInTrain(incoming, it->first);
		it->second -= countOccurrencesInDepot(incoming->tile, it->first, incoming);
		if ( it->second < 0 ) it->second = 0;
	}

	// get overall buying cost
	for ( it=unique_eids.begin(); it!=unique_eids.end(); it++ ) {
		for ( int j=0; j<it->second; j++ ) {
			estimate.AddCost(DoCommand(tile, it->first, 0, DC_NONE, CMD_BUILD_VEHICLE));
		}
	}

	return estimate;
}

/*
 * Copy the status of a single template wagon onto a Train wagon
 * Both inputs are treated as singular vehicles.
 */
void CopyWagonStatus(TemplateVehicle *from, Train *to) {
	to->cargo_type = from->cargo_type;
	to->cargo_subtype = from->cargo_subtype;
}

/*
 * Count and return the number of Trains that currently need template replacement
 */
int NumTrainsNeedTemplateReplacement(GroupID g_id, TemplateVehicle *tv)
{
	int count = 0;
	if ( !tv ) return count;

	const Train *t;
	FOR_ALL_TRAINS(t) {
		if ( t->IsPrimaryVehicle() && t->group_id == g_id && (!TrainMatchesTemplate(t, tv) || !TrainMatchesTemplateRefit(t, tv)) )
			count++;
	}
	return count;
}

/*
 * Copy the refit status from a complete Template train onto a complete Train
 */
static void RefitTrainFromTemplate(Train *t, TemplateVehicle *tv)
{
	while ( t && tv ) {
		// refit t as tv
		uint32 cb = GetCmdRefitVeh(t);

		DoCommandP(t->tile, t->index, tv->cargo_type | tv->cargo_subtype << 8 | 1 << 16 , cb);

		// next
		t = t->GetNextUnit();
		tv = tv->GetNextUnit();
	}
}

/*
 * Return the total cost of all parts of a Template train
 */
CommandCost TestBuyAllTemplateVehiclesInChain(TemplateVehicle *tv, TileIndex tile)
{
	CommandCost cost(EXPENSES_NEW_VEHICLES);

	for ( ; tv; tv=tv->GetNextUnit() )
		cost.AddCost( DoCommand(tile, tv->engine_type, 0, DC_NONE, CMD_BUILD_VEHICLE) );

	return cost;
}


/** Transfer as much cargo from a given (single train) vehicle onto a chain of vehicles.
 *  I.e., iterate over the chain from head to tail and use all available cargo capacity (w.r.t. cargo type of course)
 *  to store the cargo from the given single vehicle.
 *  @param old_veh:		ptr to the single vehicle, which's cargo shall be moved
 *  @param new_head:	ptr to the head of the chain, which shall obtain old_veh's cargo
 *  @return:			amount of moved cargo
 */
void TransferCargoForTrain(Train *old_veh, Train *new_head)
{
	assert(new_head->IsPrimaryVehicle());

	CargoID _cargo_type = old_veh->cargo_type;
	byte _cargo_subtype = old_veh->cargo_subtype;

	// how much cargo has to be moved (if possible)
	uint remainingAmount = old_veh->cargo.TotalCount();
	// each vehicle in the new chain shall be given as much of the old cargo as possible, until none is left
	for (Train *tmp=new_head; tmp!=NULL && remainingAmount>0; tmp=tmp->GetNextUnit())
	{
		if (tmp->cargo_type == _cargo_type && tmp->cargo_subtype == _cargo_subtype)
		{
			// calculate the free space for new cargo on the current vehicle
			uint curCap = tmp->cargo_cap - tmp->cargo.TotalCount();
			uint moveAmount = std::min(remainingAmount, curCap);
			// move (parts of) the old vehicle's cargo onto the current vehicle of the new chain
			if (moveAmount > 0)
			{
				old_veh->cargo.Shift(moveAmount, &tmp->cargo);
				remainingAmount -= moveAmount;
			}
		}
	}

	// from autoreplace_cmd.cpp : 121
	/* Any left-overs will be thrown away, but not their feeder share. */
	//if (src->cargo_cap < src->cargo.TotalCount()) src->cargo.Truncate(src->cargo.TotalCount() - src->cargo_cap);

	/* Update train weight etc., the old vehicle will be sold anyway */
	new_head->ConsistChanged(ConsistChangeFlags::CCF_LOADUNLOAD);
}

// if exec==DC_EXEC, test first and execute if sucessful
CommandCost CmdTemplateReplaceVehicle(Train *incoming, bool stayInDepot, DoCommandFlag flags) {
	Train	*new_chain=0,
			*remainder_chain=0,
			*tmp_chain=0;
	TileIndex tile = incoming->tile;
	TemplateVehicle *tv = GetTemplateVehicleByGroupID(incoming->group_id);
	EngineID eid = tv->engine_type;

	CommandCost buy(EXPENSES_NEW_VEHICLES);
	CommandCost move_cost(EXPENSES_NEW_VEHICLES);
	CommandCost tmp_result(EXPENSES_NEW_VEHICLES);


	/* first some tests on necessity and sanity */
	if ( !tv )
		return buy;
	bool need_replacement = !TrainMatchesTemplate(incoming, tv);
	bool need_refit = !TrainMatchesTemplateRefit(incoming, tv);
	bool use_refit = tv->refit_as_template;
	CargoID store_refit_ct = CT_INVALID;
	short store_refit_csubt = 0;
	// if a train shall keep its old refit, store the refit setting of its first vehicle
	if ( !use_refit ) {
		for ( Train *getc=incoming; getc; getc=getc->GetNextUnit() )
			if ( getc->cargo_type != CT_INVALID ) {
				store_refit_ct = getc->cargo_type;
				break;
			}
	}

	if ( !need_replacement ) {
		if ( !need_refit || !use_refit ) {
			/* before returning, release incoming train first if 2nd param says so */
			if ( !stayInDepot ) incoming->vehstatus &= ~VS_STOPPED;
			return buy;
		}
	} else {
		CommandCost buyCost = TestBuyAllTemplateVehiclesInChain(tv, tile);
		if ( !buyCost.Succeeded() || !CheckCompanyHasMoney(buyCost) ) {
			if ( !stayInDepot ) incoming->vehstatus &= ~VS_STOPPED;
			return buy;
		}
	}

	/* define replacement behaviour */
	bool reuseDepot = tv->IsSetReuseDepotVehicles();
	bool keepRemainders = tv->IsSetKeepRemainingVehicles();

	if ( need_replacement ) {
		/// step 1: generate primary for newchain and generate remainder_chain
			// 1. primary of incoming might already fit the template
				// leave incoming's primary as is and move the rest to a free chain = remainder_chain
			// 2. needed primary might be one of incoming's member vehicles
			// 3. primary might be available as orphan vehicle in the depot
			// 4. we need to buy a new engine for the primary
			// all options other than 1. need to make sure to copy incoming's primary's status
		if ( eid == incoming->engine_type ) {													// 1
			new_chain = incoming;
			remainder_chain = incoming->GetNextUnit();
			if ( remainder_chain )
				move_cost.AddCost(CmdMoveRailVehicle(tile, flags, remainder_chain->index|(1<<20), INVALID_VEHICLE, 0));
		}
		else if ( (tmp_chain = ChainContainsEngine(incoming, eid)) && tmp_chain!=NULL )	{		// 2
			// new_chain is the needed engine, move it to an empty spot in the depot
			new_chain = tmp_chain;
			move_cost.AddCost(DoCommand(tile, new_chain->index, INVALID_VEHICLE, flags,CMD_MOVE_RAIL_VEHICLE));
			remainder_chain = incoming;
		}
		else if ( reuseDepot && (tmp_chain = DepotContainsEngine(tile, eid, incoming)) && tmp_chain!=NULL ) {	// 3
			new_chain = tmp_chain;
			move_cost.AddCost(DoCommand(tile, new_chain->index, INVALID_VEHICLE, flags, CMD_MOVE_RAIL_VEHICLE));
			remainder_chain = incoming;
		}
		else {																				// 4
			tmp_result = DoCommand(tile, eid, 0, flags, CMD_BUILD_VEHICLE);
			/* break up in case buying the vehicle didn't succeed */
			if ( !tmp_result.Succeeded() )
				return tmp_result;
			buy.AddCost(tmp_result);
			new_chain = Train::Get(_new_vehicle_id);
			/* make sure the newly built engine is not attached to any free wagons inside the depot */
			move_cost.AddCost ( DoCommand(tile, new_chain->index, INVALID_VEHICLE, flags, CMD_MOVE_RAIL_VEHICLE) );
			/* prepare the remainder chain */
			remainder_chain = incoming;
		}
		// If we bought a new engine or reused one from the depot, copy some parameters from the incoming primary engine
		if ( incoming != new_chain && flags == DC_EXEC) {
			CopyHeadSpecificThings(incoming, new_chain, flags);
			NeutralizeStatus(incoming);
			// additionally, if we don't want to use the template refit, refit as incoming
			// the template refit will be set further down, if we use it at all
			if ( !use_refit ) {
				uint32 cb = GetCmdRefitVeh(new_chain);
				DoCommandP(new_chain->tile, new_chain->index, store_refit_ct | store_refit_csubt << 8 | 1 << 16 , cb);
			}

		}

		/// step 2: fill up newchain according to the template
			// foreach member of template (after primary):
				// 1. needed engine might be within remainder_chain already
				// 2. needed engine might be orphaned within the depot (copy status)
				// 3. we need to buy (again)						   (copy status)
		TemplateVehicle *cur_tmpl = tv->GetNextUnit();
		Train *last_veh = new_chain;
		while (cur_tmpl) {
			// 1. engine contained in remainder chain
			if ( (tmp_chain = ChainContainsEngine(remainder_chain, cur_tmpl->engine_type)) && tmp_chain!=NULL )	{
				// advance remainder_chain (if necessary) to not lose track of it
				if ( tmp_chain == remainder_chain )
					remainder_chain = remainder_chain->GetNextUnit();
				move_cost.AddCost(CmdMoveRailVehicle(tile, flags, tmp_chain->index, last_veh->index, 0));
			}
			// 2. engine contained somewhere else in the depot
			else if ( reuseDepot && (tmp_chain = DepotContainsEngine(tile, cur_tmpl->engine_type, new_chain)) && tmp_chain!=NULL ) {
				move_cost.AddCost(CmdMoveRailVehicle(tile, flags, tmp_chain->index, last_veh->index, 0));
			}
			// 3. must buy new engine
			else {
				tmp_result = DoCommand(tile, cur_tmpl->engine_type, 0, flags, CMD_BUILD_VEHICLE);
				if ( !tmp_result.Succeeded() )
					return tmp_result;
				buy.AddCost(tmp_result);
				tmp_chain = Train::Get(_new_vehicle_id);
				move_cost.AddCost(CmdMoveRailVehicle(tile, flags, tmp_chain->index, last_veh->index, 0));
			}
			if ( need_refit && flags == DC_EXEC ) {
				if ( use_refit ) {
					uint32 cb = GetCmdRefitVeh(tmp_chain);
					DoCommandP(tmp_chain->tile, tmp_chain->index, cur_tmpl->cargo_type | cur_tmpl->cargo_subtype << 8 | 1 << 16 , cb);
				} else {
					uint32 cb = GetCmdRefitVeh(tmp_chain);
					DoCommandP(tmp_chain->tile, tmp_chain->index, store_refit_ct | store_refit_csubt << 8 | 1 << 16 , cb);
				}
			}
			cur_tmpl = cur_tmpl->GetNextUnit();
			last_veh = tmp_chain;
		}
	}
	/* no replacement done */
	else {
		new_chain = incoming;
	}
	/// step 3: reorder and neutralize the remaining vehicles from incoming
		// wagons remaining from remainder_chain should be filled up in as few freewagonchains as possible
		// each locos might be left as singular in the depot
		// neutralize each remaining engine's status

	// refit, only if the template option is set so
	if ( use_refit && (need_refit || need_replacement) ) {
		RefitTrainFromTemplate(new_chain, tv);
	}

	if ( new_chain && remainder_chain )
		for ( Train *ct=remainder_chain; ct; ct=ct->GetNextUnit() )
			TransferCargoForTrain(ct, new_chain);

	// point incoming to the newly created train so that starting/stopping from the calling function can be done
	incoming = new_chain;
	if ( !stayInDepot && flags == DC_EXEC )
		new_chain->vehstatus &= ~VS_STOPPED;

	if ( remainder_chain && keepRemainders && flags == DC_EXEC )
		BreakUpRemainders(remainder_chain);
	else if ( remainder_chain ) {
		buy.AddCost(DoCommand(tile, remainder_chain->index | (1<<20), 0, flags, CMD_SELL_VEHICLE));
	}
	return buy;
}

// TODO this contains the exact content of the old Cmd function above
CommandCost cmd_helper_func(Train *incoming, bool stayInDepot, DoCommandFlag flags) {
	Train	*new_chain=0,
			*remainder_chain=0,
			*tmp_chain=0;
	TileIndex tile = incoming->tile;
	TemplateVehicle *tv = GetTemplateVehicleByGroupID(incoming->group_id);
	EngineID eid = tv->engine_type;

	CommandCost buy(EXPENSES_NEW_VEHICLES);
	CommandCost move_cost(EXPENSES_NEW_VEHICLES);
	CommandCost tmp_result(EXPENSES_NEW_VEHICLES);


	/* first some tests on necessity and sanity */
	if ( !tv )
		return buy;
	bool need_replacement = !TrainMatchesTemplate(incoming, tv);
	bool need_refit = !TrainMatchesTemplateRefit(incoming, tv);
	bool use_refit = tv->refit_as_template;
	CargoID store_refit_ct = CT_INVALID;
	short store_refit_csubt = 0;
	// if a train shall keep its old refit, store the refit setting of its first vehicle
	if ( !use_refit ) {
		for ( Train *getc=incoming; getc; getc=getc->GetNextUnit() )
			if ( getc->cargo_type != CT_INVALID ) {
				store_refit_ct = getc->cargo_type;
				break;
			}
	}

	if ( !need_replacement ) {
		if ( !need_refit || !use_refit ) {
			/* before returning, release incoming train first if 2nd param says so */
			if ( !stayInDepot ) incoming->vehstatus &= ~VS_STOPPED;
			return buy;
		}
	} else {
		CommandCost buyCost = TestBuyAllTemplateVehiclesInChain(tv, tile);
		if ( !buyCost.Succeeded() || !CheckCompanyHasMoney(buyCost) ) {
			if ( !stayInDepot ) incoming->vehstatus &= ~VS_STOPPED;
			return buy;
		}
	}

	/* define replacement behaviour */
	bool reuseDepot = tv->IsSetReuseDepotVehicles();
	bool keepRemainders = tv->IsSetKeepRemainingVehicles();

	if ( need_replacement ) {
		/// step 1: generate primary for newchain and generate remainder_chain
			// 1. primary of incoming might already fit the template
				// leave incoming's primary as is and move the rest to a free chain = remainder_chain
			// 2. needed primary might be one of incoming's member vehicles
			// 3. primary might be available as orphan vehicle in the depot
			// 4. we need to buy a new engine for the primary
			// all options other than 1. need to make sure to copy incoming's primary's status
		if ( eid == incoming->engine_type ) {													// 1
			new_chain = incoming;
			remainder_chain = incoming->GetNextUnit();
			if ( remainder_chain )
				move_cost.AddCost(CmdMoveRailVehicle(tile, flags, remainder_chain->index|(1<<20), INVALID_VEHICLE, 0));
		}
		else if ( (tmp_chain = ChainContainsEngine(incoming, eid)) && tmp_chain!=NULL )	{		// 2
			// new_chain is the needed engine, move it to an empty spot in the depot
			new_chain = tmp_chain;
			move_cost.AddCost(DoCommand(tile, new_chain->index, INVALID_VEHICLE, flags,CMD_MOVE_RAIL_VEHICLE));
			remainder_chain = incoming;
		}
		else if ( reuseDepot && (tmp_chain = DepotContainsEngine(tile, eid, incoming)) && tmp_chain!=NULL ) {	// 3
			new_chain = tmp_chain;
			move_cost.AddCost(DoCommand(tile, new_chain->index, INVALID_VEHICLE, flags, CMD_MOVE_RAIL_VEHICLE));
			remainder_chain = incoming;
		}
		else {																				// 4
			tmp_result = DoCommand(tile, eid, 0, flags, CMD_BUILD_VEHICLE);
			/* break up in case buying the vehicle didn't succeed */
			if ( !tmp_result.Succeeded() )
				return tmp_result;
			buy.AddCost(tmp_result);
			new_chain = Train::Get(_new_vehicle_id);
			/* make sure the newly built engine is not attached to any free wagons inside the depot */
			move_cost.AddCost ( DoCommand(tile, new_chain->index, INVALID_VEHICLE, flags, CMD_MOVE_RAIL_VEHICLE) );
			/* prepare the remainder chain */
			remainder_chain = incoming;
		}
		// If we bought a new engine or reused one from the depot, copy some parameters from the incoming primary engine
		if ( incoming != new_chain && flags == DC_EXEC) {
			CopyHeadSpecificThings(incoming, new_chain, flags);
			NeutralizeStatus(incoming);
			// additionally, if we don't want to use the template refit, refit as incoming
			// the template refit will be set further down, if we use it at all
			if ( !use_refit ) {
				uint32 cb = GetCmdRefitVeh(new_chain);
				DoCommandP(new_chain->tile, new_chain->index, store_refit_ct | store_refit_csubt << 8 | 1 << 16 , cb);
			}

		}

		/// step 2: fill up newchain according to the template
			// foreach member of template (after primary):
				// 1. needed engine might be within remainder_chain already
				// 2. needed engine might be orphaned within the depot (copy status)
				// 3. we need to buy (again)						   (copy status)
		TemplateVehicle *cur_tmpl = tv->GetNextUnit();
		Train *last_veh = new_chain;
		while (cur_tmpl) {
			// 1. engine contained in remainder chain
			if ( (tmp_chain = ChainContainsEngine(remainder_chain, cur_tmpl->engine_type)) && tmp_chain!=NULL )	{
				// advance remainder_chain (if necessary) to not lose track of it
				if ( tmp_chain == remainder_chain )
					remainder_chain = remainder_chain->GetNextUnit();
				move_cost.AddCost(CmdMoveRailVehicle(tile, flags, tmp_chain->index, last_veh->index, 0));
			}
			// 2. engine contained somewhere else in the depot
			else if ( reuseDepot && (tmp_chain = DepotContainsEngine(tile, cur_tmpl->engine_type, new_chain)) && tmp_chain!=NULL ) {
				move_cost.AddCost(CmdMoveRailVehicle(tile, flags, tmp_chain->index, last_veh->index, 0));
			}
			// 3. must buy new engine
			else {
				tmp_result = DoCommand(tile, cur_tmpl->engine_type, 0, flags, CMD_BUILD_VEHICLE);
				if ( !tmp_result.Succeeded() )
					return tmp_result;
				buy.AddCost(tmp_result);
				tmp_chain = Train::Get(_new_vehicle_id);
				move_cost.AddCost(CmdMoveRailVehicle(tile, flags, tmp_chain->index, last_veh->index, 0));
			}
			if ( need_refit && flags == DC_EXEC ) {
				if ( use_refit ) {
					uint32 cb = GetCmdRefitVeh(tmp_chain);
					DoCommandP(tmp_chain->tile, tmp_chain->index, cur_tmpl->cargo_type | cur_tmpl->cargo_subtype << 8 | 1 << 16 , cb);
				} else {
					uint32 cb = GetCmdRefitVeh(tmp_chain);
					DoCommandP(tmp_chain->tile, tmp_chain->index, store_refit_ct | store_refit_csubt << 8 | 1 << 16 , cb);
				}
			}
			cur_tmpl = cur_tmpl->GetNextUnit();
			last_veh = tmp_chain;
		}
	}
	/* no replacement done */
	else {
		new_chain = incoming;
	}
	/// step 3: reorder and neutralize the remaining vehicles from incoming
		// wagons remaining from remainder_chain should be filled up in as few freewagonchains as possible
		// each locos might be left as singular in the depot
		// neutralize each remaining engine's status

	// refit, only if the template option is set so
	if ( use_refit && (need_refit || need_replacement) ) {
		RefitTrainFromTemplate(new_chain, tv);
	}

	if ( new_chain && remainder_chain )
		for ( Train *ct=remainder_chain; ct; ct=ct->GetNextUnit() )
			TransferCargoForTrain(ct, new_chain);

	// point incoming to the newly created train so that starting/stopping from the calling function can be done
	incoming = new_chain;
	if ( !stayInDepot && flags == DC_EXEC )
		new_chain->vehstatus &= ~VS_STOPPED;

	if ( remainder_chain && keepRemainders && flags == DC_EXEC )
		BreakUpRemainders(remainder_chain);
	else if ( remainder_chain ) {
		buy.AddCost(DoCommand(tile, remainder_chain->index | (1<<20), 0, flags, CMD_SELL_VEHICLE));
	}
	return buy;
}
CommandCost CmdTemplateReplaceVehicle(TileIndex ti, DoCommandFlag flags, uint32 p1, uint32 p2, char const* msg) {
	// TODO this parameter must be replaced by an id that can be looked up
	Train* incoming = (Train*)p1;

	bool stayInDepot = false;

	return cmd_helper_func(incoming, stayInDepot, flags);
}

