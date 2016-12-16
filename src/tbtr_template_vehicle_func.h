/* $Id: build_vehicle_gui.cpp 23792 2012-01-12 19:23:00Z yexo $ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tbtr_template_vehicle_func.h Various setup and utility functions around template trains. */

#ifndef TEMPLATE_VEHICLE_FUNC_H
#define TEMPLATE_VEHICLE_FUNC_H

#include "stdafx.h"

#include "window_gui.h"

#include "tbtr_template_vehicle.h"

//void DrawTemplateVehicle(TemplateVehicle*, int, const Rect&);
void DrawTemplateVehicle(const TemplateVehicle*, int, int, int, VehicleID, int, VehicleID);

void BuildTemplateGuiList(GUITemplateList*, Scrollbar*, Owner, RailType);

Money CalculateOverallTemplateCost(const TemplateVehicle*);

void DrawTemplateTrain(const TemplateVehicle*, int, int, int);

SpriteID GetSpriteID(EngineID, bool);

void DrawTemplate(const TemplateVehicle*, int, int, int);

int GetTemplateDisplayImageWidth(EngineID);

TemplateVehicle *CreateNewTemplateVehicle(EngineID);

void setupVirtTrain(const TemplateVehicle*, Train*);

TemplateVehicle* TemplateVehicleFromVirtualTrain(Train*);

Train* VirtualTrainFromTemplateVehicle(TemplateVehicle*);

inline TemplateVehicle* Last(TemplateVehicle*);

TemplateVehicle *DeleteTemplateVehicle(TemplateVehicle*);

Train* DeleteVirtualTrain(Train*, Train *);

CommandCost CmdTemplateReplaceVehicle(TileIndex, DoCommandFlag, uint32, uint32, char const*);
CommandCost CmdTemplateReplaceVehicle(Train*, bool, DoCommandFlag);

TemplateVehicle* GetTemplateVehicleByGroupID(GroupID);
bool ChainContainsVehicle(Train*, Train*);
Train* ChainContainsEngine(Train*, EngineID);
Train* DepotContainsEngine(TileIndex, EngineID, Train*);

int NumTrainsNeedTemplateReplacement(GroupID, TemplateVehicle*);

CommandCost TestBuyAllTemplateVehiclesInChain(Train*);
CommandCost CalculateTemplateReplacementCost(Train*);

short CountEnginesInChain(Train*);

bool TemplateVehicleContainsEngineOfRailtype(const TemplateVehicle*, RailType);

Train* CloneVirtualTrainFromTrain(const Train *);
TemplateVehicle* CloneTemplateVehicleFromTrain(const Train *);

void TransferCargoForTrain(Train*, Train*);

#endif
