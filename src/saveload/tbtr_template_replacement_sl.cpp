/* $Id: build_vehicle_gui.cpp 23792 2012-01-12 19:23:00Z yexo $ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tbtr_template_replacement_sl.cpp Save and load template replacement instances. */

#include "../stdafx.h"

#include "../tbtr_template_vehicle.h"

#include "saveload.h"

static const SaveLoad _template_replacement_desc[] = {
	SLE_VAR(TemplateReplacement, sel_template, SLE_UINT16),
	SLE_VAR(TemplateReplacement, group, SLE_UINT16),
	SLE_END()
};

static void Save_TMPL_RPLS()
{
	TemplateReplacement *tr;

	FOR_ALL_TEMPLATE_REPLACEMENTS(tr) {
		SlSetArrayIndex(tr->index);
		SlObject(tr, _template_replacement_desc);
	}
}

static void Load_TMPL_RPLS()
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		TemplateReplacement *tr = new (index) TemplateReplacement();
		SlObject(tr, _template_replacement_desc);
	}
}

extern const ChunkHandler _template_replacement_chunk_handlers[] = {
	{'TRPL', Save_TMPL_RPLS, Load_TMPL_RPLS, NULL, NULL, CH_ARRAY | CH_LAST},
};
