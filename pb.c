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
 * YAJL needs cmake.
 * To build: cd yayl/ && ./configure && make
 *
 * -I yajl/build/yajl-2.1.1/include -L yajl/build/yajl-2.1.1/lib
 */

/*
 * BUGS: '^' is used as a delimiter so this character in any input will mess
 * up the parsing TODO: search for ^ and replace with something else before
 * parsing, then replace back
 *
 * Descriptions or other fields containing " will break the parser. For this reason \" needs to be replaced with something else
 *
 * Incorrect user/pass not handled - should check for 401 response
 *
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

#if defined(__linux__)
#define _GNU_SOURCE // required on linux for asprintf
#endif

#include <stdio.h> // usual
#include <stdlib.h> //abort, getenv
#include <curl/curl.h> // curl
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h> // open
#include <unistd.h> // close, getopt, getpass
#include <string.h> // strstr, strdup, strtok
#include <stdbool.h> // C99 includes bool type
#include <time.h> // time types

/*
 * Headers -- local
 */

#include "yajl/yajl_parse.h"
#include "yajl/yajl_gen.h"
#include "yajl/yajl_tree.h"

#include "macros.h"
#include "helper.c"

/*
 * Option globals
 */

bool	opt_debug = false;
bool	opt_verbose = false;
bool	opt_trace = true;
bool	opt_tags_only = false;
bool	opt_no_tags = false;
bool 	opt_no_colours = false;
bool 	opt_hashes = false;
bool 	opt_del = false;

/*
 * More option globals
 */

struct {
		bool	opt_remote;
		bool	opt_output;
		bool	opt_sanitise;
		bool	opt_warn;
		bool	opt_check;
		bool	opt_autoupdate;
		bool	opt_force_update;
		char	*opt_username;
		char	*opt_password;
		char	*opt_add_url;
		char	*opt_add_title;
		char	*opt_search_str;
	} options;

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

	Trace(); 

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

bool does_file_exist(char *filepath)
{
	struct stat	buffer;
	int		status;
	int		fildes;

	Trace(); 

	fildes = open(filepath, O_RDONLY);
	status = fstat(fildes, &buffer);
	close(fildes);

	if (status == -1) {
		V("%s does not seem to exist or is not readable", filepath);
		return false;
	} else return true;
}

// Does file exist with sensible permissions? If not, create it.
int make_file(char *filepath)
{
	struct stat	buffer;
	int		status;
	int		fildes;

	Trace(); 

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

	Trace(); 

	for (int i = 0; i < (colour_count - 1); i++)
		printf("%s%s\n", colours[i], colour_labels[i]);

	return 0;
}

/* From example at http://curl.haxx.se/libcurl/c/simple.html */
/* TODO: add check for 401 response */
// Pulls file at URL using CURL, writes to filepath
int curl_grab(char *url, char *filepath, char *username, char *password)
{
	CURL		*curl;
	CURLcode	res;
	FILE		*fp;
	bool		curl_success;

	Trace(); 

	fp = fopen(filepath, "w");
	if (!fp)
		return -1;

	if (!strval(username) || !strval(password))
		return -1;

	curl = curl_easy_init();
	if (curl) {
		curl_success = true;
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
		curl_easy_setopt(curl, CURLOPT_USERNAME, username);
		curl_easy_setopt(curl, CURLOPT_PASSWORD, password);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirect
		res = curl_easy_perform(curl); /* Perform the request, res will get the return code */

		if (res != CURLE_OK)								   /* Check for errors */
			fprintf(se, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

		curl_easy_cleanup(curl); /* always cleanup */
	} else curl_success = false;

	fclose(fp);

	if (curl_success) return 0;
	else return -1;
}

int curl_jam(char *url, char *username, char *password)
{
	CURL		*curl;
	CURLcode	res;
	bool		curl_success;

	Trace(); 

	if (!strval(username) || !strval(password))
		return -1;

	curl = curl_easy_init();
	if (curl) {
		curl_success = true;
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_USERNAME, username);
		curl_easy_setopt(curl, CURLOPT_PASSWORD, password);
		//curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirect
		res = curl_easy_perform(curl); /* Perform the request, res will get the return code */

		if (res != CURLE_OK)								   /* Check for errors */
			fprintf(se, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

		curl_easy_cleanup(curl); /* always cleanup */
	} else curl_success = false;

	if (curl_success) return 0;
	else return -1;
}

// Crudely prints file to stdout
int simple_output(char *filedir, char *verb)
{
	char	*filename;
	char	buf;
	int		fd;

	Trace(); 

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

	Trace(); 

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
				fprintf(se, "error on file read.\n");
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
		fprintf(se, "%s", (const char *)str);
		yajl_free_error(hand, str);
		retval = 1;
	}

	yajl_gen_free(g);
	yajl_free(hand);
	fclose(fp);

	free(filename);
	return retval;
}

void del_me(char *url)
{
  printf("Delete!\n");
}

void del_prompt(char *url)
{
  char b1[512];
  puts(url);
  ask_string("Do you want to delete this URL (y/n?) ", b1);
  puts("");
  if (strcasestr(b1,"y")) del_me(url);
}

int parse_handoff(unsigned char *buf, size_t len)
{
	/*
	 * buf of size len is provided from YAJL and then passed in via
	 * argument
	 * Note: relies on opt_tags
	 */

	unsigned char	*loc = NULL;
	bool			between_quotes = 0;
	int				between_quotes_n = 0;
	char			out_buffer[len + 1];
	char			*out = out_buffer;
	char			*spare = out_buffer;
	char			sep = 94; // ^ character

	Trace(); 

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

	Dbg("Post tokenising, count is %i", count);
	for (int j = 0; j < count; j++) {
		/* If any of the tag strings are the strings we are interested in */
		if (strstr(pairs[j].tag, "href") 
				|| strstr(pairs[j].tag, "description") 
				|| strstr(pairs[j].tag, "tags") 
				|| strstr(pairs[j].tag, "hash"))
		{
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

	if (!bm_count) return 0; /* Saves below being executed with 0 */

	/* Index 0 is normal text control sequence */
	Dbg("In bookmark array with %i bookmarks", bm_count);

  /* 
   * OUTPUT LOOP
   */

	for (int j = 0; j < bm_count; j++) {
    // If this is set we are searching, otherwise we are listing
    if (strval(options.opt_search_str))
      {
        if ((strcasestr(bm[j].hash,options.opt_search_str) != NULL)         
          || (strcasestr(bm[j].desc,options.opt_search_str) != NULL)        
          || (strcasestr(bm[j].href,options.opt_search_str) != NULL))
        {
          if (opt_no_colours) {
            if (opt_hashes) printf("%s\n", bm[j].hash);
            printf("%s\n", bm[j].desc);
            printf("%s\n", bm[j].href);
          } else {
            if (opt_hashes) printf("%s%s%s\n", clr[1], bm[j].hash, clr[0]);
            printf("%s%s%s\n", clr[3], bm[j].desc, clr[0]);
            printf("%s%s%s\n", clr[2], bm[j].href, clr[0]);
          }
          if (opt_del)
            del_prompt(bm[j].desc);
        }
      }
      else
      {
      /* No colours version */
      if (opt_no_colours)
      {
        //printf("%s%s%s\n", clr[1], bm[j].hash, clr[0]);
        if (!opt_tags_only) {
          if (opt_hashes) printf("%s\n", bm[j].hash);
          printf("%s\n", bm[j].desc);
          printf("%s\n", bm[j].href);
        }
        /* Note we exclude tag string if short enough to be empty */
        if (!opt_no_tags && (strlen(bm[j].tags) > 1)) 
          printf("%s\n", bm[j].tags);
      } else {
        //printf("%s%s%s\n", clr[1], bm[j].hash, clr[0]);
        if (!opt_tags_only) {
          if (opt_hashes) printf("%s%s%s\n", clr[1], bm[j].hash, clr[0]);
          printf("%s%s%s\n", clr[3], bm[j].desc, clr[0]);
          printf("%s%s%s\n", clr[2], bm[j].href, clr[0]);
        }
        /* Note we exclude tag string if short enough to be empty */
        if (!opt_no_tags && (strlen(bm[j].tags) > 1)) {
          printf("%s%s%s\n", clr[4], bm[j].tags, clr[0]);
       }
      }
		}
    //putchar('\n');
	}

	/* Free strings created with stdup */
	for (int j = 0; j < count; j++) {
		free(pairs[j].tag);
		free(pairs[j].value);
	}

	return 0;
}

// Sets up YAJL then sends to parse_handoff
int output(char *filedir, char *verb)
{
	yajl_handle		hand;
	static unsigned char fileData[65536];
	int				retval = 0;
	/* generator config */
	yajl_gen		g;
	yajl_status		stat;
	size_t			rd;
	FILE			*fp;
	char			*filename;

	Trace(); 

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
				fprintf(se, "error on file read.\n");
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
		fprintf(se, "%s", (const char *)str);
		yajl_free_error(hand, str);
		retval = 1;
	}
	yajl_gen_free(g);
	yajl_free(hand);

	free(filename);
	fclose(fp);
	return retval;
}

// Reads file to freshly malloc'd char*
// The return value will need freeing
char * file_to_mem(char *directory, char *verb, int *size)
{
	char			*filepath;
	char			*data;
	int				status;
	int				fildes;
	int				bytes_read = 1;
	struct	stat	buffer;

	Trace(); 

	asprintf(&filepath, "%s/%s.json", directory, verb);
	V("Opening file %s", filepath);
	fildes = open(filepath, O_RDONLY);

	if (fildes < 0)
		Ftl("Cannot open %s", filepath);

	status = fstat(fildes, &buffer);
	close(fildes);

    *size = buffer.st_size;
    V("%lld bytes size, %d bytes", buffer.st_size, *size);
	if (buffer.st_size > 1000000) {
		//If more than a meg something is going wrong, bail
		Ftl("File is %lld big which doesn't make sense, bailing", buffer.st_size);
	} else {
		data = malloc(buffer.st_size + 1);
	}

	fildes = open(filepath, O_RDONLY);

	while (bytes_read > 0)
		bytes_read = read(fildes, data, buffer.st_size);


	close(fildes);
	free(filepath);

    // NUL terminate
    //*(data+1) = '\0';

	return data;
}

/*
 * Copies a field in data_from to data_to
 *
 * field is the field we want e.g. 1, 2, 3 ... etc
 * delim is the delimiter character
 * data_from is source data
 * date_to is where to copy the field
 * max is sizeof data_from
 */
void simple_parse_field(int field, char delim, char *data_from, char *data_to, int max)
{
	char		*loc, *out;
	bool		in_field = false;
	int			in_field_count = 0;

	Trace(); 

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
 * Takes string in iso8601 format
 * Returns time_t
 * Note: pinboard API seems to always assume the time is GMT, so that is what we do here
 * http://stackoverflow.com/questions/26895428/how-do-i-parse-an-iso-8601-date-with-optional-milliseconds-to-a-struct-tm-in-c
 */

time_t convert_iso8601(char *dt_string)
{
	int y, M, d, h, m;
	float s;
	struct tm time;

	Trace(); 

	sscanf(dt_string, "%d-%d-%dT%d:%d:%fZ", &y, &M, &d, &h, &m, &s);
		
	time.tm_year = y - 1900; // Year since 1900
	time.tm_mon = M - 1;     // 0-11
	time.tm_mday = d;        // 1-31
	time.tm_hour = h;        // 0-23
	time.tm_min = m;         // 0-59
	time.tm_sec = (int)s;    // 0-61 (0-60 in C++11)
	//time.tm_isdst = 0;

	return mktime(&time);
}

int check_update()
{
	Trace(); 
	puts("TODO: check for updates");
	return 0;
}

int sanitise_json(char *directory)
{
	Trace(); 

    char *b = NULL;
    char *b2 = NULL;
    int b_len;

    b = file_to_mem(directory, "all", &b_len);
    V("Read %d bytes from file_to_mem", b_len);
    b2 = b;

    swpch("\\\"", "--", b, b_len);

    char *filename;
	asprintf(&filename, "%s/%s.json", directory, "all_san");

    FILE *fp = fopen(filename, "w");

    if (fp == NULL)
    {
            printf("Error opening file!\n"); exit(1);
    }

    fputs(b2,fp); 
    fclose(fp);

    free(filename);
    free(b);

    return 0;
}

int api_add(char *username, char *password, char *webaddr, char *title)
{
  char		*url;
  char *escaped;
  int retval = -1;
	Trace(); 

  if (!strval(username) 
      || !strval(password)
      || !strval(webaddr)
      || !strval(title))
		return -1;

  CURL		*curl;
	curl = curl_easy_init();
  escaped = curl_easy_escape(curl, webaddr, 0);
  puts(escaped);
  
	asprintf(&url, 
      "https://api.pinboard.in/v1/posts/add?format=json&url=%s&description=%s", 
      escaped, title); 

  Print("Using:\n"
	      "%s as URL\n"
		  "%s/%s as username/pass", url, username, password);

  retval = curl_jam(url, username, password);

	free(url);
  curl_free(escaped);
  curl_easy_cleanup(curl); /* always cleanup */
  return retval;
}

// Wrapper for downloading with the API
int api_download(char *verb, char *username, char *password, char *directory)
{
	char		*url;
	char		*filepath;

	Trace(); 

	asprintf(&url, "https://api.pinboard.in/v1/posts/%s&format=json", verb);
	asprintf(&filepath, "%s/%s.json", directory, verb);

	/* TODO: check update time */

	if (!remove(filepath))
		Print("Deleted %s", filepath);

	make_file(filepath);

	Print("Using:\n"
	      "%s as filepath\n"
	      "%s as URL\n"
		  "%s/%s as username/pass", filepath, url, username, password);

	curl_grab(url, filepath, username, password);
	free(url);
	free(filepath);
	return 0;
}

time_t get_file_last_mod_time(char *filepath)
{
	int				status;
	int				fildes;
	struct	stat	buffer;

	Trace(); 
	V("Opening file %s", filepath);

	fildes = open(filepath, O_RDONLY);
	if (fildes < 0)
		Ftl("Cannot open %s", filepath);
	//	Dbg("Cannot open %s", filepath);

	status = fstat(fildes, &buffer);
	close(fildes);

	return buffer.st_mtime;	
}

/* Get age of file -- returns 'age' (i.e. time since last modified) of file in seconds as a double */
double time_since_last_mod(char *file)
{
	struct tm	file_tm;
	struct tm	current_tm;
	time_t	file_last_mod;
	time_t	current_time;

	char buffer[512];
	double seconds_age;

	Trace(); 

	file_last_mod = get_file_last_mod_time(file);

	localtime_r(&file_last_mod, &file_tm);
	Zero_char(buffer, sizeof(buffer));
	strftime(buffer, sizeof(buffer), "%A %F %T %Z", &file_tm);
	V("Update time %s", buffer);

	current_time = time(NULL);			
	localtime_r(&current_time, &current_tm);
	Zero_char(buffer, sizeof(buffer));
	strftime(buffer, sizeof(buffer), "%A %F %T %Z", &current_tm);
	V("Current time %s", buffer);

	seconds_age = difftime(mktime(&current_tm), mktime(&file_tm));
	V("Delta in seconds %.0f, minutes %.2f, hours %.2f", seconds_age, seconds_age / 60.0, seconds_age / 60.0 / 60.0);

	return seconds_age;
}

bool is_file_more_recent(char *file, struct tm dt_tm)
{
	time_t file_last_mod;
	struct tm	file_tm;
	char buffer[512];
	double delta;

	Trace(); 

	file_last_mod = get_file_last_mod_time(file);
	localtime_r(&file_last_mod, &file_tm);

	Zero_char(buffer, sizeof(buffer));
	strftime(buffer, sizeof(buffer), "%A %F %T %Z", &file_tm);
	V("Update time %s", buffer);

	delta = difftime(mktime(&file_tm), mktime(&dt_tm));
	V("Delta in seconds %.0f, minutes %.2f, hours %.2f", delta, delta / 60.0, delta / 60.0 / 60.0);

	return (delta > 0);
}

// 
int auto_update(char *filepath, char *username, char *password)
{
	char		*d;
	char		b1[1024] = {0};
	char	 	buffer[512];
	double		time_since_update;
    int         d_len;

	time_t		api_update_time;
	struct tm	api_update_time_tm;

	Trace(); 

	/* Check we have all.json */
	Zero_char(buffer, sizeof(buffer));
	snprintf(buffer, sizeof(buffer), "%s/%s", filepath, "all.json");

	if (!does_file_exist(buffer)) {
		V("%s doesn't seem to exist, pulling", buffer);
		api_download("all", username, password, filepath);
	}

	/* Check when update.json last updated */
	Zero_char(buffer, sizeof(buffer));
	snprintf(buffer, sizeof(buffer), "%s/%s", filepath, "update.json");

	if (!does_file_exist(buffer)) {
		V("Downloading")
		api_download("update", username, password, filepath);
	}

	time_since_update = time_since_last_mod(buffer);

	/* D/l if stale */
	if (time_since_update >= (30 * 60)) /* If greater than 30 mins have elapsed */
	{
		V("Looks stale, let's update");
		api_download("update", username, password, filepath);
	} else {
		V("Recent enough, not updating");
	}

	/* See when update.json says we last updated */
	d = file_to_mem(filepath, "update", &d_len);
	simple_parse_field(2, '"', d, b1, strlen(d));
	free(d);

	api_update_time = convert_iso8601(b1);
	localtime_r(&api_update_time, &api_update_time_tm);

	Zero_char(buffer, sizeof(buffer));
	strftime(buffer, sizeof(buffer), "%A %F %T %Z", &api_update_time_tm);
	V("API tells us we last updated on %s (Note this is cached)", buffer);

	/* Check when all.json last updated */
	Zero_char(buffer, sizeof(buffer));
	snprintf(buffer, sizeof(buffer), "%s/%s", filepath, "all.json");
	if (is_file_more_recent(buffer, api_update_time_tm))
	{
		V("Data is more recent than last update according to API");
	}
	else
	{
		V("Data is older than last update according to API, let's update");
		api_download("all", username, password, filepath);
		/* We also delete update.json to force this to be updated next time run */
		Zero_char(buffer, sizeof(buffer));
		snprintf(buffer, sizeof(buffer), "%s/%s", filepath, "update.json");
		if (remove(buffer)) {
			Ftl("Failed to delete %s", buffer);
		} else {
			V("%s deleted", buffer);
		}
	}

	//output(filepath, "all");
	return 0;
}

void force_update(char *filepath, char *username, char *password)
{
	char	 	buffer[512];
	Trace();

	V("Deleting update and all posts cache and re-downloading");

	/* Delete update.json and re-download */
	Zero_char(buffer, sizeof(buffer));
	snprintf(buffer, sizeof(buffer), "%s/%s", filepath, "update.json");
	if (remove(buffer)) {
		V("Failed to delete %s", buffer);
	} else {
		V("%s deleted", buffer);
		api_download("update", username, password, filepath);
	}

	/* Delete all.json and re-download */
	Zero_char(buffer, sizeof(buffer));
	snprintf(buffer, sizeof(buffer), "%s/%s", filepath, "all.json");
	if (remove(buffer)) {
		V("Failed to delete %s", buffer);
	} else {
		V("%s deleted", buffer);
		api_download("all", username, password, filepath);
	}

	return;
}

void check_env_variables()
{
  char * s_user = "PB_USER";
  char * s_pass = "PB_PASS";

  Trace();

  Dbg("getenv %s: %s", s_user, getenv(s_user));
  Dbg("getenv %s: %s", s_pass, getenv(s_pass));

  if (getenv(s_user) != NULL)
    asprintf(&options.opt_username, "%s", getenv(s_user));

  if (getenv(s_user) != NULL)
    asprintf(&options.opt_password, "%s", getenv(s_pass));

  return;
}

int main(int argc, char *argv[])
{
	/* Pointer to string of filepath where working files are located */
	char		*filepath = NULL;

	char		msg_warn[] =
	"NOTE: CURRENTLY STILL IN DEVELOPMENT";

	char		msg_usage[] =
	"______________\n"
	"pinboard-shell\n"
	"______________\n"
	"\n"
	"Usage:\n"
	"ADD:\n"
  "-t Title -u \"https://url.com/\" \n"
	"DELETE:\n"
  "-r \"string\" < Yet to be implimented\n"
  "SEARCH:\n"
  "-z \"string\"\n"
	"LIST:\n"
	"-o flag to list data\n"
	"-w do not output tags\n"
	"-c toggle tags only\n"
	"-p turn off formatting e.g. for redirecting stdout to a file\n"
	"UPDATE:\n"
	"-a auto update: updates if the API says it has updated since last downloaded\n"
	"-f force update: forces update, useful if error\n" /* TODO: check flow control */
  "OTHER:\n"
	"-v toggle verbose\n" 
	"-d turn debug mode on\n"
	"-h this help\n"
	"\n"
	"Example usage:\n"
	"Download bookmarks file from pinboard.in\n"
	"./main -f\n"
	"OR\n"
	"Output only\n"
	"./main -o\n"
	"\n"
	"Further Example usage:\n"
	"CHECK ME:\n" /* TODO: these need checking */
	"List most frequent tags: pb -ocp | sort | awk '{ print $NF }' | uniq -c | sort -nr | less\n"
	"List those tagged with $TAG for export to a file: pb -op | grep --color=never -B2 $TAG > $TAG.tagged\n"
	"\n";

	error_mode = 's'; /* Makes Stopif use abort() */
	int		c;
	bool	errflg = false;
	int		exitflg = 0;
	int		ret = 0;

	
	options.opt_remote = false; /* We are going to be downloading and therefore require username/pass */
	options.opt_output = false;
	options.opt_sanitise = false;
	options.opt_check = false;
	options.opt_autoupdate = false;
	options.opt_username = NULL;
	options.opt_password = NULL;
	options.opt_force_update = false;

	/* Required for getopt */
	extern char	*optarg;
	extern int	optind, optopt;

    se = stdout;
    /* se = stderr; */
    

	/* getopt loop per example at http://pubs.opengroup.org/onlinepubs/009696799/functions/getopt.html */
	// -o -v -d -h -u -p
	while ((c = getopt(argc, argv, ":afcovdhswpu:t:z:r:")) != -1) {
		switch (c) {
      case 'z':
        options.opt_output = true;
        asprintf(&options.opt_search_str, "%s", optarg);
        Dbg("Searching");
        break;
      case 's':
        options.opt_sanitise = true;
        break;
      case 'a':
        options.opt_autoupdate = true;
        options.opt_remote = true;
        Dbg("Autoupdate");
        break;
      case 'f':
        options.opt_force_update = true;
        options.opt_autoupdate = false;
        options.opt_remote = true;
        Dbg("Forced update");
        break;
      case 'p':
        opt_no_colours = true;
        Dbg("Printing with no colours");
        break;
      case 'c':
        opt_tags_only = true;
        Dbg("Tags only");
        break;
      case 'w':
        opt_no_tags = true;
        Dbg("No tags");
        break;
      case 'o':
        options.opt_output = true;
        options.opt_autoupdate = false;
        Dbg("Output");
        break;
      case 'v':
        opt_verbose = !opt_verbose;
        Dbg("Verbose toggled");
        break;
      case 'd':
        opt_verbose = true;
        opt_trace = true;
        opt_debug = true;
        error_mode = 's';							   /* Makes Stopif use * abort() */
        Dbg("Debug mode set");
        break;
      case 'h':
        errflg = true; // i.e. show help
        break;
      case 'r':
        opt_del = true;
        options.opt_output = true;
        asprintf(&options.opt_search_str, "%s", optarg);
        Dbg("Deleting");
        break;
      case 't':
        asprintf(&options.opt_add_title, "%s", optarg);
        break;
      case 'u':
        asprintf(&options.opt_add_url, "%s", optarg);
        break;
      case ':':									   /* Option req argument without argument */
        fprintf(se, "Option -%c requires an operand\n", optopt);
        errflg = true;
        break;
      case '?':
        fprintf(se, "Unrecognized option: -%c\n", optopt);
        errflg = true;
		}
	}

  /* Tell user what is going on */ 
	puts(msg_warn);
  if (opt_verbose) V("Verbose mode on");
  if (opt_trace) V("Tracing mode on");
  if (opt_debug) V("Debugging mode on");

  check_env_variables(); /* Checks env variables for user/pass combo */

	/* Some issue -- print usage message */
	if (errflg) {
		fprintf(se, "%s", msg_usage);
		return 2;
	}

	/* Some issue -- return */
	if (exitflg) return exitflg;
	
	tzset(); /* Set timezone I do believe */

  if (options.opt_username && options.opt_password) {
		Print("Using:\n"
		      "%s as username\n"
		      "%s as password", options.opt_username, options.opt_password);
	} else if (options.opt_remote && (!options.opt_username || !options.opt_password)) {
		char b1[512];
		if (!options.opt_username) { 
			Zero_char(b1, sizeof(b1));
			ask_string("Username: ", b1);
			asprintf(&options.opt_username, "%s", b1);
		}
		if (!options.opt_password) {
			Zero_char(b1, sizeof(b1));
			/* NOTE: getpass is depreciated */
			options.opt_password = getpass("Password: ");
			//asprintf(&opt_password, "%s", b1);
		}
	}

  /* If these are both set we want to add and then quit */
  if (strval(options.opt_add_url) && strval(options.opt_add_title)) {
     api_add(options.opt_username, options.opt_password, options.opt_add_url, options.opt_add_title);
      goto bye;
  }

	/*
	 * Steps > See if JSON file exists and is not old > If not, download
	 * with libcurl > Parse > Output > Other functions if required
	 */

	/* Look in ~/.pinboard and check we can write to it */

	asprintf(&filepath, "%s/%s", getenv("HOME"), ".pinboard");
	V("Looking in %s", filepath);
	check_dir(filepath);

	if (options.opt_force_update && options.opt_remote) {
		force_update(filepath, options.opt_username, options.opt_password);
	}

	if (options.opt_autoupdate && options.opt_remote) {
		auto_update(filepath, options.opt_username, options.opt_password);
	}

	if (options.opt_output) {
		output(filepath, "all");
	}

  if (options.opt_sanitise) {
    puts("Sanitize!\n");
    sanitise_json(filepath);
  }

bye:
	Sfree(filepath);
	Sfree(options.opt_username);
	Sfree(options.opt_password);

	return ret;
}
