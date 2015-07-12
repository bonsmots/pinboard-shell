/*
 * pinboard-shell
 *
 * Downloads, parses and prints JSON data from pinboard.in
 *
 * Uses libcurl, YAJL
 *
 */

/*
 * NOTE:
 * YAJL needs cmake to build cd yayl/ ./configure make
 *
 * -I yajl/build/yajl-2.1.1/include -L yajl/build/yajl-2.1.1/lib
 */

/*
 * BUGS: '^' is used as a delimiter so this character in any input will mess
 * up the parsing TODO: search for ^ and replace with something else before
 * parsing, then replace back
 */

/*
 * libcurl
 */

/*
 * This source code makes use of the libcurl library, which is released under
 * a a MIT/X derivative license. More info on YAJL can be found at
 * <http://curl.haxx.se/libcurl/>
 */

/*
 * libcurl copyright (c) 1996 - 2014, Daniel Stenberg, daniel@haxx.se.
 *
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any purpose
 * with or without fee is hereby granted, provided that the above copyright
 * notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY
 * RIGHTS. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of a copyright holder shall not
 * be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization of the
 * copyright holder.
 */

/*
 * YAJL
 */

/*
 * This software makes use of the YAJL library, which is released under the
 * ISC license. More info on YAJL can be found at
 * <https://github.com/lloyd/yajl>
 */

/*
 * YAJL library Copyright (c) 2007-2014, Lloyd Hilaiel <me@lloyd.io>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Headers -- global
 */

#include <stdio.h> // usual
#include <stdlib.h> //abort, getenv
#include <curl/curl.h> // curl
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h> // open
#include <unistd.h> // close, getopt, getpass
#include <string.h> // strstr, strdup, strtok
#include <stdbool.h> // C99 includes bool type
#include <time.h>

/*
 * Headers -- local
 */

#include "yajl/yajl_parse.h"
#include "yajl/yajl_gen.h"
#include "yajl/yajl_tree.h"

/*
 * Macros
 */

// TODO: clean these up a bit, duplicated functionality

#define Trace(...) \
{ \
	if (opt_trace) { \
	fprintf(stdout, "Trace: "); \
	fprintf(stdout, __VA_ARGS__); \
	fprintf(stdout, "\n"); \
	} \
}

#define Dbg(...) \
{ \
	if (opt_debug) { \
	fprintf(stderr, "Debug message: "); \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr, "\n"); \
	} \
}

#define Dbgnl(...) \
{ \
	if (opt_debug) { \
	fprintf(stderr, "Debug message: "); \
	fprintf(stderr, __VA_ARGS__); \
	} \
}

#define V(...) \
{ \
	if (opt_verbose) { \
	fprintf(stderr, "Verbose message: "); \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr, "\n"); \
	} \
}

#define Se(...) \
{ \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr, "\n"); \
}

#define Ftl(...) \
{ \
	Dbg(__VA_ARGS__); \
	abort(); \
}

#define Print(...) \
{ \
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

/*
 * Helper functions
 */

// TODO: obviously not secure
char* ask_string(bool visible, char *message, char *input)
{
	printf("%s", message);
	scanf("%s", input);
	return input;
}

// Returns string length if not NULL
int strval(char *str)
{
	if (str) return strlen(str);
	else return 0;
}

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

/*
 * Option globals
 */

int		opt_debug = 0;
bool	opt_verbose = true;
bool	opt_trace = true;

/*
 * Other globals
 */

// This is the colour pallete that we use -- the first value is the colour reset
char *clr[] =
{
	"\e[0m", //This is the reset code
	"\e[0;34m",
	"\e[0;36m",
	"\e[0;35m",
	"\e[1;37m"
};

/* Struct to hold tag and value pairs from the JSON */
typedef struct {
	char           *tag;
	char           *value;
}	pair;

typedef struct {
	char           *hash;
	char           *href;
	char           *desc;
	char           *tags;
}	bookmark;

/*
 * YAJL callback functions -- these are a lift from the YAJL documentation
 */

static int reformat_null(void *ctx)
{
	yajl_gen	g = (yajl_gen) ctx;
	return yajl_gen_status_ok == yajl_gen_null(g);
}

static int reformat_boolean(void *ctx, int boolean)
{
	yajl_gen	g = (yajl_gen) ctx;
	return yajl_gen_status_ok == yajl_gen_bool(g, boolean);
}

static int reformat_number(void *ctx, const char *s, size_t l)
{
	yajl_gen	g = (yajl_gen) ctx;
	return yajl_gen_status_ok == yajl_gen_number(g, s, l);
}

static int reformat_string(void *ctx, const unsigned char *stringVal,
		size_t stringLen)
{
	yajl_gen	g = (yajl_gen) ctx;
	return yajl_gen_status_ok == yajl_gen_string(g, stringVal, stringLen);
}

static int reformat_map_key(void *ctx, const unsigned char *stringVal,
		 size_t stringLen)
{
	yajl_gen	g = (yajl_gen) ctx;
	return yajl_gen_status_ok == yajl_gen_string(g, stringVal, stringLen);
}

static int reformat_start_map(void *ctx)
{
	yajl_gen	g = (yajl_gen) ctx;
	return yajl_gen_status_ok == yajl_gen_map_open(g);
}


static int reformat_end_map(void *ctx)
{
	yajl_gen	g = (yajl_gen) ctx;
	return yajl_gen_status_ok == yajl_gen_map_close(g);
}

static int reformat_start_array(void *ctx)
{
	yajl_gen	g = (yajl_gen) ctx;
	return yajl_gen_status_ok == yajl_gen_array_open(g);
}

static int reformat_end_array(void *ctx)
{
	yajl_gen	g = (yajl_gen) ctx;
	return yajl_gen_status_ok == yajl_gen_array_close(g);
}

static yajl_callbacks callbacks = {
	reformat_null,
	reformat_boolean,
	NULL,
	NULL,
	reformat_number,
	reformat_string,
	reformat_start_map,
	reformat_map_key,
	reformat_end_map,
	reformat_start_array,
	reformat_end_array
};

// Does directory exist with sensible permissions? If not, create it.
int check_dir(char *dirpath)
{
	struct stat	buffer;
	int		status;
	int		fildes;

	Trace("In %s", __func__); 

	/* Open, get status, close */
	fildes = open(dirpath, O_RDONLY | O_DIRECTORY);
	status = fstat(fildes, &buffer);
	close(fildes);

	if (status == -1) {
		Dbg("Failed to open folder %s, likley it doesn't exist.", dirpath);
		Dbg("Making folder %s", dirpath);
		// TODO: should change this to tighter permissions
		status = mkdir(dirpath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		/*
		 * S_IRWXU read, write, execute/search by owner S_IRWXG read,
		 * write, execute/search by group S_IROTH read permission,
		 * others S_IXOTH execute/search permission, others
		 */
		Stopif(status != 0, return -1, "Make folder %s failed.", dirpath);
	}

	/* Open, get status, close */
	fildes = open(dirpath, O_RDONLY | O_DIRECTORY);
	status = fstat(fildes, &buffer);
	close(fildes);

	/* Check directory, readable, writeable */
	if (S_ISDIR(buffer.st_mode) && ((S_IRUSR & buffer.st_mode) == S_IRUSR) && ((S_IWUSR & buffer.st_mode) == S_IWUSR)) {
		Dbg("We are in business, directory has sensible perminssions");
	} else {
		Dbg("Not a directory or not read/writable");
	}

	return 0;
}

// Does file exist with sensible permissions? If not, create it.
int make_file(char *filepath)
{
	struct stat	buffer;
	int		status;
	int		fildes;

	Trace("In %s", __func__); 

	fildes = open(filepath, O_RDONLY);
	status = fstat(fildes, &buffer);
	close(fildes);

	if (status == -1) {
		Dbg("Failed to open file %s, likley it doesn't exist or is not readable.", filepath);
		Dbg("Making file %s", filepath);
		// TODO: should change this to tighter permissions
		fildes = open(filepath, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

		/*
		 * O_WRONLY write only O_CREAT create O_EXCL fail if file
		 * exists S_IRUSR read permission, owner S_IWUSR write
		 * permission, owner S_IRGRP read permission, group S_IROTH
		 * read permission, others
		 */

		Stopif(fildes == -1, return -1, "Open file %s for writing has failed.", filepath);
		close(fildes);
	}
	fildes = open(filepath, O_RDONLY);
	status = fstat(fildes, &buffer);
	close(fildes);

	/* Check regular file, readable, writeable */
	if (S_ISREG(buffer.st_mode) && ((S_IRUSR & buffer.st_mode) == S_IRUSR) && ((S_IWUSR & buffer.st_mode) == S_IWUSR)) {
		Dbg("We are in business");
	} else {
		Dbg("Not a file or not read/writable");
	}

	close(fildes);

	return 0;
}

// UNUSED
int test_colours()
{
	int		colour_count = 15;
	char	*colours[] =
	{
		"\e[0;30m",
		"\e[0;34m", //+
		"\e[0;32m",
		"\e[0;36m", //+
		"\e[0;31m",
		"\e[0;35m", //+
		"\e[0;33m",
		"\e[0;37m",
		"\e[1;30m",
		"\e[1;34m",
		"\e[1;32m",
		"\e[1;36m",
		"\e[1;31m",
		"\e[1;35m",
		"\e[1;33m",
		"\e[1;37m" // +
	};

	char           *colour_labels[] =
	{
		"Black            ",
		"Blue             ",
		"Green            ",
		"Cyan             ",
		"Red              ",
		"Purple           ",
		"Brown            ",
		"Gray             ",
		"Dark Gray        ",
		"Light Blue       ",
		"Light Green      ",
		"Light Cyan       ",
		"Light Red        ",
		"Light Purple     ",
		"Yellow           ",
		"White            "
	};

	Trace("In %s", __func__); 

	for (int i = 0; i < (colour_count - 1); i++)
		printf("%s%s\n", colours[i], colour_labels[i]);

	return 0;
}

/* From example at http://curl.haxx.se/libcurl/c/simple.html */
/* TODO: add check for 401 response */
/* TODO: set username and pass via curl opts */
// Pulls file at URL using CURL, writes to filepath
int curl_grab(char *url, char *filepath)
{
	CURL		*curl;
	CURLcode	res;
	FILE		*fp;
	bool		curl_success;

	Trace("In %s", __func__); 

	fp = fopen(filepath, "w");
	if (!fp)
		return -1;

	curl = curl_easy_init();
	if (curl) {
		curl_success = true;
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirect
		res = curl_easy_perform(curl); /* Perform the request, res will get the return code */

		if (res != CURLE_OK)								   /* Check for errors */
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

		curl_easy_cleanup(curl); /* always cleanup */
	} else curl_success = false;

	fclose(fp);

	if (curl_success) return 0;
	else return -1;
}

// Crudely prints file to stdout
int simple_output(char *filedir, char *verb)
{
	char	*filename;
	char	buf;
	int		fd;

	Trace("In %s", __func__); 

	asprintf(&filename, "%s/%s.json", filedir, verb);

	fd = open(filename, O_RDONLY);
	if (!fd)
		return -1;

	while (read(fd, &buf, 1))
		putchar(buf);

	close(fd);
	free(filename);

	return 0;
}

// Outputs file to stdout, nicely formatted
int pretty_json_output(char *filedir, char *verb)
{
	yajl_handle		hand;
	static unsigned char fileData[65536];
	/* generator config */
	yajl_gen		g;
	yajl_status		stat;
	size_t			rd;
	int				retval = 0;
	FILE			*fp;
	char			*filename;

	Trace("In %s", __func__); 

	asprintf(&filename, "%s/%s.json", filedir, verb);

	fp = fopen(filename, "r");
	if (!fp)
		return -1;

	g = yajl_gen_alloc(NULL);
	yajl_gen_config(g, yajl_gen_beautify, 1);
	yajl_gen_config(g, yajl_gen_validate_utf8, 1);

	/* ok.  open file.  let's read and parse */
	hand = yajl_alloc(&callbacks, NULL, (void *)g);
	/* and let's allow comments by default */
	yajl_config(hand, yajl_allow_comments, 1);

	for (;;) {
		rd = fread((void *)fileData, 1, sizeof(fileData) - 1, fp);

		if (rd == 0) {
			if (!feof(fp)) {
				fprintf(stderr, "error on file read.\n");
				retval = 1;
			}
			break;
		}
		fileData[rd] = 0;

		stat = yajl_parse(hand, fileData, rd);

		if (stat != yajl_status_ok)
			break;

		{
			const unsigned char *buf;
			size_t		len;

			yajl_gen_get_buf(g, &buf, &len);
			fwrite(buf, 1, len, stdout);
			yajl_gen_clear(g);
		}
	}

	stat = yajl_complete_parse(hand);

	if (stat != yajl_status_ok) {
		unsigned char  *str = yajl_get_error(hand, 1, fileData, rd);
		fprintf(stderr, "%s", (const char *)str);
		yajl_free_error(hand, str);
		retval = 1;
	}

	yajl_gen_free(g);
	yajl_free(hand);
	fclose(fp);

	free(filename);
	return retval;
}

int parse_handoff(unsigned char *buf, size_t len)
{
	/*
	 * buf of size len is provided from YAJL and then passed in via
	 * argument
	 */

	unsigned char	*loc = NULL;
	bool			between_quotes = 0;
	int				between_quotes_n = 0;
	char			out_buffer[len + 1];
	char			*out = out_buffer;
	char			*spare = out_buffer;
	char			sep = 94; // ^ character

	Trace("In %s", __func__); 

	Dbg("Len is %zu", len);

	/* Zero our output buffer */
	for (int j = 0; j < len; j++) {
		*spare = '\0';
		spare++;
	}

	spare = out;
	loc = buf;

	/*
	 * The below removes everyting except tag and value, adding sep
	 * between the two by way of delimiter
	 */

	for (int j = 0; j < len; j++) {
		if (*loc == '"') {
			/* When we hit the double quote char, toggle the bool to reflect the fact that we are entering or leaving a quoted string */
			between_quotes = !between_quotes;
			/* If entering a quoted string, increment the count of quoted strings */
			if (between_quotes) between_quotes_n++;

			/* Out is a pointer to output buffer so we can do pointer arithmatic and therefore move about */
			if (!between_quotes) {
				/* Replace empty fields with space */
				if (*(out - 1) == sep) {
					*out = ' ';
					out++;
				}
				*out = sep;
				out++;
			}
		} else {
			if (between_quotes) {
				/* Below copies char at *loc to *out, thus writing to out_buffer */ 
				*out = *loc;
				out++;
			}
		}

		/* Move along by incrementing pointer */
		loc++;
	}
	/* out_buffer now contains all our buffer to tokenise */

	char		*cp = NULL;
	char		*dp = NULL;
	char		tokens[] = {sep, '\0'};
	pair		pairs[between_quotes_n]; // TODO: 2 x as big as needed ? Not worried as essentially a buffer value
	bookmark	bm[between_quotes_n]; // Same comment
	int			count = 0;
	int			bm_count = 0;
	int			i = 0;
	bool		tag = true;

	cp = strtok(spare, tokens); /* Breaks string into tokens -- first token */
	Null_bookmarks(bm, between_quotes_n);

	/* TODO: remove strtok as depreciated */
	while (1) {
		if (cp == NULL) break;

		dp = strdup(cp); /* strdup returns pointed to malloc'd copy, thus dp is a new string */
		if (dp == NULL) {
			Ftl("Failed strdup, likely malloc issue");
		}

		/* If we are not in tag we are in value -- point the tag or value pointer to the new string */
		if (tag) {
			pairs[count].tag = dp;
		} else {
			pairs[count].value = dp;
			count++;
		}

		tag = !tag;	/* Toggle tag and value */
		cp = strtok(NULL, tokens) /* Breaks string into tokens -- subsequent tokens */
;
	}

	Dbg("Post tokenising:");
	for (int j = 0; j < count; j++) {
		/* If any of the tag strings are the strings we are interested in */
		if (strstr(pairs[j].tag, "href") 
				|| strstr(pairs[j].tag, "description") 
				|| strstr(pairs[j].tag, "tags") 
				|| strstr(pairs[j].tag, "hash"))
		{
			Dbgnl("%s:%s\n", pairs[j].tag, pairs[j].value);

			/* Put the values in */
			/* strstr() returns the pointer to first occurance of substring in the sring */
			/* Point the bm struct pointers to the values in the pair structs */
			if (strstr(pairs[j].tag, "hash"))
				bm[i].hash = pairs[j].value;
			if (strstr(pairs[j].tag, "href"))
				bm[i].href = pairs[j].value;
			if (strstr(pairs[j].tag, "description"))
				bm[i].desc = pairs[j].value;
			if (strstr(pairs[j].tag, "tags"))
				bm[i].tags = pairs[j].value;

			/* If now all non-null values, increment */
			/* bm_count used when we print to screen so know when done */
			if (strval(bm[i].hash) && strval(bm[i].href) && strval(bm[i].desc) && strval(bm[i].tags)) {
				i++;
				bm_count++;
			}
		}
	}

	/* Index 0 is normal text control sequence */
	Dbg("In bookmark array:");
	for (int j = 0; j < bm_count; j++) {
		printf("%s%s%s\n", clr[1], bm[j].hash, clr[0]);
		printf("%s%s%s\n", clr[3], bm[j].desc, clr[0]);
		printf("%s%s%s\n", clr[4], bm[j].tags, clr[0]);
		printf("%s%s%s\n", clr[2], bm[j].href, clr[0]);
		putchar('\n');
	}

	/* Free strings created with stdup */
	for (int j = 0; j < count; j++) {
		free(pairs[j].tag);
		free(pairs[j].value);
	}

	return 0;
}

/* OLD BOOKMARK */
// Sets up YAJL then sends to parse_handoff
int output(char *filedir, char *verb)
{
	yajl_handle		hand;
	static unsigned char fileData[65536];

	/* generator config */
	yajl_gen		g;
	yajl_status		stat;
	size_t			rd;
	int				retval = 0;
	FILE			*fp;
	char			*filename;

	Trace("In %s", __func__); 

	asprintf(&filename, "%s/%s.json", filedir, verb);

	fp = fopen(filename, "r");
	if (!fp)
		return -1;
	else
		Dbg("Output open suceeded");

	/* ? */
	g = yajl_gen_alloc(NULL);

	/* Set JAJL options */
	yajl_gen_config(g, yajl_gen_beautify, 1);
	yajl_gen_config(g, yajl_gen_validate_utf8, 1);

	/* ok.  open file.  let's read and parse */
	hand = yajl_alloc(&callbacks, NULL, (void *)g);
	/* and let's allow comments by default */
	yajl_config(hand, yajl_allow_comments, 1);


	for (;;) {
		rd = fread((void *)fileData, 1, sizeof(fileData) - 1, fp);

		if (rd == 0) {
			if (!feof(fp)) {
				fprintf(stderr, "error on file read.\n");
				retval = 1;
			}
			break;
		}

		fileData[rd] = 0;

		stat = yajl_parse(hand, fileData, rd);

		if (stat != yajl_status_ok)
			break;

		{
			const unsigned char *buf;
			size_t		len;

			yajl_gen_get_buf(g, &buf, &len);
			Dbg("Handing off");
			parse_handoff((unsigned char *)buf, len);
			yajl_gen_clear(g);
		}
	}

	stat = yajl_complete_parse(hand);
	if (stat != yajl_status_ok) {
		unsigned char  *str = yajl_get_error(hand, 1, fileData, rd);
		fprintf(stderr, "%s", (const char *)str);
		yajl_free_error(hand, str);
		retval = 1;
	}
	yajl_gen_free(g);
	yajl_free(hand);

	free(filename);
	fclose(fp);
	return 0;
}

// Reads file to freshly malloc'd char*
// The return value will need freeing
char * file_to_mem(char *directory, char *verb)
{
	char			*filepath;
	int				status;
	int				fildes;
	int				bytes_read = 1;
	char			*data = NULL;
	struct	stat	buffer;

	Trace("In %s", __func__); 

	asprintf(&filepath, "%s/%s.json", directory, verb);
	V("Opening file %s", filepath);

	fildes = open(filepath, O_RDONLY);

	if (fildes < 0)
		Ftl("Cannot open %s", filepath);

	status = fstat(fildes, &buffer);
	close(fildes);

	if (buffer.st_size > 1024) {
		//If more than a meg something is going wrong, bail
		Ftl("File is more than a meg which doesn't make sense, bailing");
	} else {
		data = malloc(buffer.st_size + 1);
	}

	fildes = open(filepath, O_RDONLY);

	while (bytes_read > 0)
		bytes_read = read(fildes, data, buffer.st_size);

	close(fildes);
	free(filepath);

	return data;
}

//
void simple_parse_field(int field, char delim, char *data_from, char *data_to, int max)
{
	char		*loc, *out;
	bool		in_field = false;
	int			in_field_count = 0;

	Trace("In %s", __func__); 

	loc = data_from;
	out = data_to;

	for (int j = 0; j < max; j++) {
		if (*loc == delim) {
			in_field = !in_field;

			if (in_field)
				in_field_count++;
		} else {
			if (in_field && (in_field_count == field)) {
				*out = *loc;
				out++;
			}
		}

		loc++;
	}

	return;
}

/*
 * http://www.ioncannon.net/programming/33/using-strptime-to-parse-iso-8601-fo
 * rmated-timestamps/
 */

void convert_iso8601(const char *time_string, int ts_len, struct tm *tm_data)
{
	tzset();

	char		temp      [64];
	struct tm	ctime;
	long		ts = mktime(&ctime);

	Trace("In %s", __func__); 

	memset(temp, 0, sizeof(temp));
	strncpy(temp, time_string, ts_len);

	memset(&ctime, 0, sizeof(struct tm));
	strptime(temp, "%FT%T%z", &ctime);
	localtime_r(&ts, tm_data);
}

int check_update()
{
	Trace("In %s", __func__); 
	puts("TODO: check for updates");
	return 0;
}

// Wrapper for downloading with the API
int api_download(char *verb, char *username, char *password, char *directory)
{
	char		*url;
	char		*filepath;

	Trace("In %s", __func__); 

	asprintf(&url, "https://%s:%s@api.pinboard.in/v1/posts/%s&format=json", username, password, verb);
	asprintf(&filepath, "%s/%s.json", directory, verb);

	/* TODO: check update time */

	if (!remove(filepath))
		Print("Deleted %s", filepath);

	make_file(filepath);

	Print("Using:\n"
	      "%s as filepath\n"
	      "%s as URL", filepath, url);

	curl_grab(url, filepath);
	free(url);
	free(filepath);
	return 0;
}

// 
int devel(char *filepath)
{
	char		*d;
	char		b1[1024] = {0};
	struct tm	tms;
	char		buf[128];

	Trace("In %s", __func__); 

	d = file_to_mem(filepath, "update");
	//?
	simple_parse_field(2, '"', d, b1, strlen(d));
	puts(b1);

	memset(&tms, 0, sizeof(struct tm));
	convert_iso8601(b1, strlen(b1), &tms);

	strftime(buf, sizeof(buf), "Date: %a, %d %b %Y %H:%M:%S %Z", &tms);
	printf("%s\n", buf);

	free(d);

	return 0;
}

int main(int argc, char *argv[])
{
	/* Pointer to string of filepath where working files are located */
	char		*filepath;

	char		msg_warn[] =
	"\n"
	"NOTE: CURRENTLY STILL IN DEVELOPMENT\n"
	"\n";

	char		msg_usage[] =
	"______________\n"
	"pinboard-shell\n"
	"______________\n"
	"\n"
	"Usage:\n"
	"-o output\n"
	"-c test colours\n"
	"-g get updated JSON file\n"
	"-s get updated JSON file only if newer than current version NOT YET IMPLIMENTED\n"
	"-t test JSON parser on already downloaded file\n"
	"-d turn debug mode on\n"
	"-u username arg\n"
	"-p password arg\n"
	"\n"
	"Example usage:\n"
	"1. Download bookmarks file from pinboard.in\n"
	"./main -u $USERNAME -p $PASSWORD -g\n"
	"2. Output\n"
	"./main -o\n"
	"\n";

	error_mode = 's'; /* Makes Stopif use abort() */

	int		c;
	bool	errflg = false;
	int		exitflg = 0;
	int		ret = 0;

	/* NEW BOOKMARK */

	struct {
		bool	opt_remote;
		bool 	opt_download;
		bool 	opt_test;
		bool	opt_output;
		bool	opt_warn;
		bool	opt_check;
		bool	opt_devel;
		bool	opt_verbose;
		char	*opt_username;
		char	*opt_password;
	} options;

	options.opt_remote = false;
	options.opt_download = false;
	options.opt_test = false;
	options.opt_output = false;
	options.opt_warn = true;
	options.opt_check = false;
	options.opt_devel = false;
	options.opt_verbose = false;
	options.opt_username = NULL;
	options.opt_password = NULL;

	/* Required for getopt */
	extern char	*optarg;
	extern int	optind, optopt;

	/* If no arguments, print usage message and quit. */
	if (argc == 1) {
		fprintf(stderr, "%s", msg_usage);
		return 2;
	}

	/* getopt loop per example at http://pubs.opengroup.org/onlinepubs/009696799/functions/getopt.html */
	while ((c = getopt(argc, argv, ":wegotdcsvzu:p:")) != -1) {
		switch (c) {
		case 'w':
			options.opt_warn = false;
			break;
		case 'e':
			options.opt_devel = true;
			break;
		case 'g':
			options.opt_remote = true;
			options.opt_download = true;
			break;
		case 'o':									   /* TODO: make argument * to an API verb? */
			options.opt_output = true;
			break;
		case 't':
			options.opt_remote = false;
			options.opt_test = true;
			break;
		case 'd':
			options.opt_devel= true;
			error_mode = 's';							   /* Makes Stopif use * abort() */
			break;
		case 'c':
			test_colours();
			exitflg++;
			break;
		case 'u':
			asprintf(&options.opt_username, "%s", optarg);
			break;
		case 'p':
			asprintf(&options.opt_password, "%s", optarg);
			break;
		case 's':
			options.opt_check = true;
			options.opt_remote = true;
			break;
		case 'v':
			options.opt_verbose = !options.opt_verbose;
			break;
		case 'z':
			options.opt_devel = true;
			break;
		case ':':									   /* -u or -p without operand */
			fprintf(stderr, "Option -%c requires an operand\n", optopt);
			errflg = true;
			break;
		case '?':
			fprintf(stderr, "Unrecognized option: -%c\n", optopt);
			errflg = true;
		}
	}

	/* Some issue -- print usage message */
	if (errflg) {
		fprintf(stderr, "%s", msg_usage);
		return 2;
	}

	/* Some issue -- return */
	if (exitflg) return exitflg;

	if (options.opt_username && options.opt_password) {
		Print("Using:\n"
		      "%s as username\n"
		      "%s as password", options.opt_username, options.opt_password);
	} else if (options.opt_remote && (!options.opt_username || !options.opt_password)) {
		char b1[512];
		if (!options.opt_username) { 
			Zero_char(b1, sizeof(b1));
			ask_string(true, "Username: ", b1);
			asprintf(&options.opt_username, "%s", b1);
		}
		if (!options.opt_password) {
			Zero_char(b1, sizeof(b1));
			/* NOTE: getpass is depreciated */
			options.opt_password = getpass("Password: ");
			//asprintf(&opt_password, "%s", b1);
		}

		Print("Using:\n"
		      "%s as username\n"
		      "%s as password", options.opt_username, options.opt_password);
	}

	if (options.opt_warn) puts(msg_warn);

	/*
	 * Steps > See if JSON file exists and is not old > If not, download
	 * with libcurl > Parse > Output > Other functions if required
	 */

	/* Look in ~/.pinboard and check we can write to it */

	asprintf(&filepath, "%s/%s", getenv("HOME"), ".pinboard");
	V("Looking in %s", filepath);
	check_dir(filepath);

	if (options.opt_download) {
		/* Download the JSON data */
		api_download("all", options.opt_username, options.opt_password, filepath);
	}

	if (options.opt_check) {
		V("Checking");
		api_download("update", options.opt_username, options.opt_password, filepath);
		simple_output(filepath, "update");
	}
	if (options.opt_test) {
		Dbg("Pretty output");
		pretty_json_output(filepath, "all");
	}
	if (options.opt_output) {
		Dbg("Output");
		output(filepath, "all");
	}
	if (options.opt_devel) {
		devel(filepath);
	}

	free(filepath);

	return ret;
}
