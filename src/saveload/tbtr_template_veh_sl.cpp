/* $Id: build_vehicle_gui.cpp 23792 2012-01-12 19:23:00Z yexo $ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tbtr_template_veh_sl.cpp Save and load template vehicles. */

#include "../stdafx.h"

#include "../tbtr_template_vehicle.h"

#include "saveload.h"

const SaveLoad* GTD() {

	static const SaveLoad _template_veh_desc[] = {
		SLE_REF(TemplateVehicle, next, 		REF_TEMPLATE_VEHICLE),

		SLE_VAR(TemplateVehicle, reuse_depot_vehicles, SLE_UINT8),
		SLE_VAR(TemplateVehicle, keep_remaining_vehicles, SLE_UINT8),
		SLE_VAR(TemplateVehicle, refit_as_template, SLE_UINT8),

		SLE_VAR(TemplateVehicle, owner, SLE_UINT32),
		SLE_VAR(TemplateVehicle, owner_b, SLE_UINT8),

		SLE_VAR(TemplateVehicle, engine_type, SLE_UINT16),
		SLE_VAR(TemplateVehicle, cargo_type, SLE_UINT8),
		SLE_VAR(TemplateVehicle, cargo_cap, SLE_UINT16),
		SLE_VAR(TemplateVehicle, cargo_subtype, SLE_UINT8),

		SLE_VAR(TemplateVehicle, subtype, SLE_UINT8),
		SLE_VAR(TemplateVehicle, railtype, SLE_UINT8),

		SLE_VAR(TemplateVehicle, index, SLE_UINT32),

		SLE_VAR(TemplateVehicle, real_consist_length, SLE_UINT16),

		SLE_VAR(TemplateVehicle, max_speed, SLE_UINT16),
		SLE_VAR(TemplateVehicle, power, SLE_UINT32),
		SLE_VAR(TemplateVehicle, weight, SLE_UINT32),
		SLE_VAR(TemplateVehicle, max_te, SLE_UINT32),

		SLE_VAR(TemplateVehicle, spritenum, SLE_UINT8),
		SLE_VAR(TemplateVehicle, cur_image, SLE_UINT32),
		SLE_VAR(TemplateVehicle, image_width, SLE_UINT32),

		SLE_END()
	};

	static const SaveLoad * const _ret[] = {
		_template_veh_desc,
	};

	return _ret[0];
}

static void Save_TMPLS()
{
	TemplateVehicle *tv;

	FOR_ALL_TEMPLATES(tv) {
		SlSetArrayIndex(tv->index);
		SlObject(tv, GTD());
	}
}

static void Load_TMPLS()
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		TemplateVehicle *tv = new (index) TemplateVehicle(); //TODO:check with veh sl code
		SlObject(tv, GTD());
	}
}

static void Ptrs_TMPLS()
{
	TemplateVehicle *tv;
	FOR_ALL_TEMPLATES(tv) {
		SlObject(tv, GTD());
	}
}

void AfterLoadTemplateVehicles()
{
	TemplateVehicle *tv;

	FOR_ALL_TEMPLATES(tv) {
		/* Reinstate the previous pointer */
		if (tv->next != NULL) tv->next->previous = tv;
		tv->first =NULL;
	}
	FOR_ALL_TEMPLATES(tv) {
		/* Fill the first pointers */
		if (tv->previous == NULL) {
			for (TemplateVehicle *u = tv; u != NULL; u = u->Next()) {
				u->first = tv;
			}
		}
	}
}

extern const ChunkHandler _template_vehicle_chunk_handlers[] = {
	{'TMPL', Save_TMPLS, Load_TMPLS, Ptrs_TMPLS, NULL, CH_ARRAY | CH_LAST},
};
