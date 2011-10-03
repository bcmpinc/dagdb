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

#ifndef UTIL_H
#define UTIL_H

// String builder.
template<typename T>
void buildstring(std::stringstream &s, const T &t) {
	s << t;
}

template<typename T, typename... Args>
void buildstring(std::stringstream &s, const T &t, const Args &... args) {
	s << t;
	buildstring(s, args...);
}

/**
 * Builds a string containing the concatenation of the arguments.
 */
template<typename... Args>
std::string buildstring(const Args &... args) {
	std::stringstream s;
	buildstring(s, args...);
	return s.str();
}


#endif