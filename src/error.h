#ifndef DAGDB_ERROR_H
#define DAGDB_ERROR_H

/** Used to perform sanity checks during compilation. */
#define STATIC_ASSERT(COND,MSG) typedef char static_assertion_##MSG[(COND)?1:-1]
#define dagdb_report(  ...) dagdb_report_int(  __func__, __VA_ARGS__)
#define dagdb_report_p(...) dagdb_report_p_int(__func__, __VA_ARGS__)
#define UNREACHABLE ({fprintf(stderr,"Unreachable state reached in '%s'\n", __func__); abort();})

typedef enum {
	DAGDB_ERROR_NONE=0,
	DAGDB_ERROR_OTHER,
	DAGDB_ERROR_BAD_ARGUMENT,
	DAGDB_ERROR_INVALID_DB,
	DAGDB_ERROR_DB_TOO_LARGE,
	DAGDB_ERROR_MAGIC,
} dagdb_error_code;

extern dagdb_error_code dagdb_errno;

void dagdb_report_int  (const char * function, const char * format, ...) __attribute__ ((format (printf, 2, 3)));
void dagdb_report_p_int(const char * function, const char * format, ...) __attribute__ ((format (printf, 2, 3)));
const char * dagdb_last_error();

#endif
