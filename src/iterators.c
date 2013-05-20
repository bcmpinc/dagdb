/*
    DagDB - A lightweight structured database system.
    Copyright (C) 2012  B.J. Conijn <bcmpinc@users.sourceforge.net>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdint.h>
#include <stdlib.h>

#include "api.h"
#include "base.h"
#include "error.h"

struct dagdb_iterator {
	int32_t location;
	int32_t depth;
	dagdb_pointer tries[DAGDB_KEY_LENGTH*2];
};

dagdb_iterator* dagdb_iterator_create(dagdb_handle src)
{
	if (dagdb_get_pointer_type(src) != )
	dagdb_iterator * r = (dagdb_iterator)malloc(sizeof(dagdb_iterator));
	if (!r) return NULL;
	r->location = 0;
	r->depth = 0;
	r->tries[0] = src;
}
