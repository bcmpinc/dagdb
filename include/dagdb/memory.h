/*
    DagDB - A lightweight structured database system.
    Copyright (C) 2011  B.J. Conijn <bcmpinc@users.sourceforge.net>

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

#ifndef DAGDB_MEMORY_H
#define DAGDB_MEMORY_H

/** @file
 * @brief Defines functions for constructing DagDB items in memory.
 * Pretty much every part of the DagDB library requires some DagDB object
 * as a parameter. This header file defines the methods that can be used 
 * to create these initially.
 */

#include <map>
#include <string>
#include "interface.h"

namespace dagdb { //
namespace memory { //

Data create_data(const void *buffer, size_t length);
Data create_data(std::string s);
Record create_record(const std::map<Element,Element> &entries);
std::string read_data(Data d);

};
};

#endif
