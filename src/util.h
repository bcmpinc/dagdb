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