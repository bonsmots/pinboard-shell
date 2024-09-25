/*
 * Macros
 */

/*
 * Stopif -- macro from 21stC C by Ben Klemens
 */

/** Set this to \c 's' to stop the program on an error.
 * Otherwise, functions return a value on failure.*/
char	error_mode;
/** To where should I write errors? If this is \c NULL, write to \c stderr. */
FILE	*error_log;
#define Stopif(assertion, error_action, ...) \
if (assertion) \
{ \
	fprintf(error_log ? error_log : stderr, __VA_ARGS__); \
	fprintf(error_log ? error_log : stderr, "\n"); \
	if (error_mode=='s') abort(); \
	else {error_action;} \
}

/* So we can easily send stderr to stdout */
/* We have to make it NULL here to avoid compiler errors */
FILE * se = NULL;
/* FILE * se = stderr; */

// TODO: clean these up a bit, duplicated functionality

#define Trace() \
{ \
	if (opt_trace) { \
	fprintf(stdout, "Trace: %s at line %i", __FUNCTION__, __LINE__); \
	fprintf(stdout, "\n"); \
	} \
}

#define Dbg(...) \
{ \
	if (opt_debug) { \
	fprintf(se, "Debug at line %i: ", __LINE__); \
	fprintf(se, __VA_ARGS__); \
	fprintf(se, "\n"); \
	} \
}

#define Dbgnl(...) \
{ \
	if (opt_debug) { \
	fprintf(se, "Debug at line %i: ", __LINE__); \
	fprintf(se, __VA_ARGS__); \
	} \
}

#define V(...) \
{ \
	if (opt_verbose) { \
	fprintf(se, "Verbose (%s:%i): ", __FILE__, __LINE__); \
	fprintf(se, __VA_ARGS__); \
	fprintf(se, "\n"); \
	} \
}

#define Se(...) \
{ \
	fprintf(se, __VA_ARGS__); \
	fprintf(se, "\n"); \
}

#define Ftl(...) \
{ \
	Dbg(__VA_ARGS__); \
	abort(); \
}

#define Print(...) \
{ \
	fprintf(stdout, "At %i: ", __LINE__); \
	fprintf(stdout, __VA_ARGS__); \
	fprintf(stdout, "\n"); \
}

// Replaces f with r in string
#define Charrep(f, r, string, len) \
{ \
	char *pointer = string; \
	for (int j = 0; j < len; j++) { \
		if (*pointer == f) \
			*pointer = r; \
		pointer++; \
	} \
}

// Initialise bookmarks array as NULLs
#define Null_bookmarks(array, items) \
{ \
	for (int counter = 0; counter < items; counter++) { \
		array[counter].hash = NULL;\
		array[counter].href = NULL;\
		array[counter].desc = NULL;\
		array[counter].tags = NULL;\
	}\
}

// Zero char buffer
#define Zero_char(array, items) \
{ \
	for (int counter = 0; counter < items; counter++) { \
		array[counter]= 0;\
	}\
}

// Safe free
#define Sfree(buf) \
{ \
  if (buf != NULL) free(buf); \
}
