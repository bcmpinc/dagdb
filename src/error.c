#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "error.h"

/**
 * Buffer to contain error message.
 */
static char errormsg[256] = "dagdb: No error.";

/**
 * Variable to contain the error code of the last error.
 */
dagdb_error_code dagdb_errno;


/**
 * Set a new error message.
 */
void dagdb_report_int(const char * function, const char * format, ...) {
	va_list args;
	va_start(args, format);
	uint_fast32_t l=0;
	                       l +=  snprintf(errormsg+l, sizeof(errormsg)-l, "%s:", function);
	if(l<sizeof(errormsg)) l += vsnprintf(errormsg+l, sizeof(errormsg)-l, format, args);
	if(l<sizeof(errormsg)) l +=  snprintf(errormsg+l, sizeof(errormsg)-l, ".");
	va_end(args);
}

/**
 * Set a new error message using the description provided by the standard library.
 */
void dagdb_report_p_int(const char * function, const char * format, ...) { 
	va_list args;
	va_start(args, format);
	uint_fast32_t l=0;
	                       l +=  snprintf(errormsg+l, sizeof(errormsg)-l, "%s:", function);
	if(l<sizeof(errormsg)) l += vsnprintf(errormsg+l, sizeof(errormsg)-l, format, args);
	if(l<sizeof(errormsg)) l +=  snprintf(errormsg+l, sizeof(errormsg)-l, ":");
	if(l<sizeof(errormsg)) strerror_r(errno, errormsg + l, sizeof(errormsg) - l); 
	va_end(args);
} 


/**
 * Returns a pointer to the static buffer that contains an explanation for the most recent error.
 * If no error has happened, this still returns a pointer to a valid string.
 */
const char* dagdb_last_error() {
	return errormsg;
}
