#if !defined(MACROS_H)
#define MACROS_H
#endif

#include <stdbool.h> // C99 bool type

/*
 * More option globals
 * Note: not in the options struct for ease of use w/ macros
 */

bool opt_debug;
bool opt_super_debug;
bool opt_verbose;
bool opt_trace;

/*
 * Macros
 */

/*
 * Stopif -- macro from 21stC C by Ben Klemens
 */

/** Set this to \c 's' to stop the program on an error.
 * Otherwise, functions return a value on failure.*/
char error_mode = 'z';
/** To where should I write errors? If this is \c NULL, write to \c stderr. */
FILE *error_log;

#define Stopif(assertion, error_action, ...)                  \
	if (assertion)                                            \
	{                                                         \
		fprintf(error_log ? error_log : stderr, __VA_ARGS__); \
		fprintf(error_log ? error_log : stderr, "\n");        \
		if (error_mode == 's')                                \
			abort();                                          \
		else                                                  \
		{                                                     \
			error_action;                                     \
		}                                                     \
	}

/* So we can easily send stderr to stdout */
/* We have to make it NULL here to avoid compiler errors */

FILE *se = NULL;
/* FILE * se = stderr; */

// TODO: clean these up a bit, duplicated functionality

#define Trace()                                                              \
	{                                                                        \
		if (opt_trace)                                                       \
		{                                                                    \
			fprintf(se, "%s:%s:%i", __FILE__, __FUNCTION__, __LINE__); \
			fprintf(se, "\n");                                           \
		}                                                                    \
	}

#define Dbg(...)                                         \
	{                                                    \
		if (opt_debug)                                   \
		{                                                \
			fprintf(se, "Debug at line %i: ", __LINE__); \
			fprintf(se, __VA_ARGS__);                    \
			fprintf(se, "\n");                           \
		}                                                \
	}

#define Dbgnl(...)                                       \
	{                                                    \
		if (opt_debug)                                   \
		{                                                \
			fprintf(se, "Debug at line %i: ", __LINE__); \
			fprintf(se, __VA_ARGS__);                    \
		}                                                \
	}

#define V(...)                                                    \
	{                                                             \
		if (opt_verbose)                                          \
		{                                                         \
			fprintf(se, "Verbose (%s:%i): ", __FILE__, __LINE__); \
			fprintf(se, __VA_ARGS__);                             \
			fprintf(se, "\n");                                    \
		}                                                         \
	}

#define Se(...)                   \
	{                             \
		fprintf(se, __VA_ARGS__); \
		fprintf(se, "\n");        \
	}

#define Ftl(...)          \
	{                     \
		Dbg(__VA_ARGS__); \
		abort();          \
	}

#define Print(...)                            \
	{                                         \
		fprintf(stdout, "At %i: ", __LINE__); \
		fprintf(stdout, __VA_ARGS__);         \
		fprintf(stdout, "\n");                \
	}

// Replaces f with r in string
#define Charrep(f, r, string, len)    \
	{                                 \
		char *pointer = string;       \
		for (int j = 0; j < len; j++) \
		{                             \
			if (*pointer == f)        \
				*pointer = r;         \
			pointer++;                \
		}                             \
	}

// Initialise bookmarks array as NULLs
#define Null_bookmarks(array, items)                      \
	{                                                     \
		for (int counter = 0; counter < items; counter++) \
		{                                                 \
			array[counter].hash = NULL;                   \
			array[counter].href = NULL;                   \
			array[counter].desc = NULL;                   \
			array[counter].tags = NULL;                   \
		}                                                 \
	}

// Zero char buffer
#define Zero_char(array, items)                           \
	{                                                     \
		for (int counter = 0; counter < items; counter++) \
		{                                                 \
			array[counter] = 0;                           \
		}                                                 \
	}

// Safe free
#define Safe_free(buf)       \
	{                    \
		if (buf != NULL) \
			free(buf);   \
	}
