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
 * YAJL needs cmake to build cd yayl/ ./configure make
 * 
 * -I yajl/build/yajl-2.1.1/include -L yajl/build/yajl-2.1.1/lib
 */

#include "yajl/yajl_parse.h"
#include "yajl/yajl_gen.h"
#include "yajl/yajl_tree.h"

#include <stdio.h>
#include <stdlib.h> //abort, getenv
#include <curl/curl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h> // open
#include <unistd.h> // close, getopt
#include <string.h> // strstr
#include <stdbool.h> // C99 advancedness

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

#define Charrep(f, r, string, len) \
{ \
	char *pointer = string; \
	for (int j = 0; j < len; j++) { \
		if (*pointer == f) \
			*pointer = r; \
		pointer++; \
	} \
}

#define Null_bookmarks(array, items) \
{ \
	for (int counter = 0; counter < items; counter++) { \
		array[counter].hash = NULL;\
		array[counter].href = NULL;\
		array[counter].desc = NULL;\
		array[counter].tags = NULL;\
	}\
}

// Not elegent.Should also add reset.
char           *clr[] =
{
	"\e[0;34m", //+
	"\e[0;36m", //+
	"\e[0;35m", //+
	"\e[1;37m" // +
};

int 
strval(char *str)
{
	if (str)
		return strlen(str);
	else
		return 0;
}

/*
 * Stopif -- macro from 21stC C by Ben Klemens
 */

/** Set this to \c 's' to stop the program on an error.
 * Otherwise, functions return a value on failure.*/
char		error_mode;
/** To where should I write errors? If this is \c NULL, write to \c stderr. */
FILE           *error_log;
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

/*
 * YAJL callbacks
 */

static int
reformat_null(void *ctx)
{
	yajl_gen	g = (yajl_gen) ctx;
	return yajl_gen_status_ok == yajl_gen_null(g);
}

static int
reformat_boolean(void *ctx, int boolean)
{
	yajl_gen	g = (yajl_gen) ctx;
	return yajl_gen_status_ok == yajl_gen_bool(g, boolean);
}

static int
reformat_number(void *ctx, const char *s, size_t l)
{
	yajl_gen	g = (yajl_gen) ctx;
	return yajl_gen_status_ok == yajl_gen_number(g, s, l);
}

static int
reformat_string(void *ctx, const unsigned char *stringVal,
		size_t stringLen)
{
	yajl_gen	g = (yajl_gen) ctx;
	return yajl_gen_status_ok == yajl_gen_string(g, stringVal, stringLen);
}

static int
reformat_map_key(void *ctx, const unsigned char *stringVal,
		 size_t stringLen)
{
	yajl_gen	g = (yajl_gen) ctx;
	return yajl_gen_status_ok == yajl_gen_string(g, stringVal, stringLen);
}

static int
reformat_start_map(void *ctx)
{
	yajl_gen	g = (yajl_gen) ctx;
	return yajl_gen_status_ok == yajl_gen_map_open(g);
}


static int
reformat_end_map(void *ctx)
{
	yajl_gen	g = (yajl_gen) ctx;
	return yajl_gen_status_ok == yajl_gen_map_close(g);
}

static int
reformat_start_array(void *ctx)
{
	yajl_gen	g = (yajl_gen) ctx;
	return yajl_gen_status_ok == yajl_gen_array_open(g);
}

static int
reformat_end_array(void *ctx)
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

/* Struct to hold tag and value pairs from the JSON */

typedef struct {
	char           *tag;
	char           *value;
}		pair;

typedef struct {
	char           *hash;
	char           *href;
	char           *desc;
	char           *tags;
}		bookmark;
/*
 * > Does the folder exist? > No: create > Yes with wrong permissions: fatal
 * complain > Yes with right permissions: continue
 */

int
check_dir(char *dirpath)
{
	struct stat	buffer;
	int		status;
	int		fildes;

	fildes = open(dirpath, O_RDONLY | O_DIRECTORY);
	status = fstat(fildes, &buffer);
	close(fildes);

	if (status == -1) {
		Dbg("Failed to open folder %s, likley it doesn't exist.", dirpath);
		Dbg("Making folder %s", dirpath);
		status = mkdir(dirpath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		/*
		 * S_IRWXU read, write, execute/search by owner S_IRWXG read,
		 * write, execute/search by group S_IROTH read permission,
		 * others S_IXOTH execute/search permission, others
		 */
		Stopif(status != 0, return -1, "Make folder %s failed.", dirpath);
	}
	fildes = open(dirpath, O_RDONLY | O_DIRECTORY);
	status = fstat(fildes, &buffer);
	close(fildes);

	/* Check directory, readable, writeable */
	if (S_ISDIR(buffer.st_mode) && ((S_IRUSR & buffer.st_mode) == S_IRUSR) && ((S_IWUSR & buffer.st_mode) == S_IWUSR)) {
		Dbg("We are in business");
	} else {
		Dbg("Not a directory or not read/writable");
	}

	return 0;
}

int
make_file(char *filepath)
{
	/*
	 * > Does the file exist? > No: create > Yes with wrong permissions:
	 * fatal complain > Yes with right permissions: continue
	 */

	struct stat	buffer;
	int		status;
	int		fildes;

	fildes = open(filepath, O_RDONLY);
	status = fstat(fildes, &buffer);
	close(fildes);

	if (status == -1) {
		Dbg("Failed to open file %s, likley it doesn't exist.", filepath);
		Dbg("Making file %s", filepath);
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

int
test_colours()
{
	int		colour_count = 15;
	char           *colours[] =
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

	for (int i = 0; i < (colour_count - 1); i++)
		printf("%s%s\n", colours[i], colour_labels[i]);

	return 0;
}

/* From example at http://curl.haxx.se/libcurl/c/simple.html */
/* TODO: add check for 401 response */

int
curl_grab(char *url, char *filepath)
{
	CURL           *curl;
	CURLcode	res;
	FILE           *fp;

	fp = fopen(filepath, "w");
	if (!fp)
		return -1;

	curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
		/*
		 * example.com is redirected, so we tell libcurl to follow
		 * redirection
		 */
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

		/* Perform the request, res will get the return code */
		res = curl_easy_perform(curl);
		/* Check for errors */
		if (res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n",
				curl_easy_strerror(res));

		/* always cleanup */
		curl_easy_cleanup(curl);
	}
	fclose(fp);

	return 0;
}

/* From example at http://curl.haxx.se/libcurl/c/simplessl.html */

int
curl_grab_ssl(char *url, char *buffer_file)
{
	int		i;
	CURL           *curl;
	CURLcode	res;
	FILE           *headerfile;
	const char     *pPassphrase = NULL;

	static const char *pCertFile = "testcert.pem";
	static const char *pCACertFile = "cacert.pem";

	const char     *pKeyName;
	const char     *pKeyType;

	const char     *pEngine;

#ifdef USE_ENGINE
	pKeyName = "rsa_test";
	pKeyType = "ENG";
	pEngine = "chil";	/* for nChiper HSM... */
#else
	pKeyName = "testkey.pem";
	pKeyType = "PEM";
	pEngine = NULL;
#endif

	headerfile = fopen("dumpit", "w");

	curl_global_init(CURL_GLOBAL_DEFAULT);

	curl = curl_easy_init();
	if (curl) {
		/* what call to write: */
		curl_easy_setopt(curl, CURLOPT_URL, "https://api.pinboard.in/v1/posts/all");
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, headerfile);

		for (i = 0; i < 1; i++) {	/* single-iteration loop,
						 * just to break out from */
			if (pEngine) {	/* use crypto engine */
				if (curl_easy_setopt(curl, CURLOPT_SSLENGINE, pEngine) != CURLE_OK) {	/* load the crypto
													 * engine */
					fprintf(stderr, "can't set crypto engine\n");
					break;
				}
				if (curl_easy_setopt(curl, CURLOPT_SSLENGINE_DEFAULT, 1L) != CURLE_OK) {	/* set the crypto engine
														 * as default */
					/*
					 * only needed for the first time you
					 * load a engine in a curl object...
					 */
					fprintf(stderr, "can't set crypto engine as default\n");
					break;
				}
			}
			/* cert is stored PEM coded in file... */
			/* since PEM is default, we needn't set it for PEM */
			curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");

			/* set the cert for client authentication */
			curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);

			/*
			 * sorry, for engine we must set the passphrase (if
			 * the key has one...)
			 */
			if (pPassphrase)
				curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

			/*
			 * if we use a key stored in a crypto engine, we must
			 * set the key type to "ENG"
			 */
			curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

			/* set the private key (file or ID in engine) */
			curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

			/* set the file with the certs vaildating the server */
			curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);

			/* disconnect if we can't validate server's cert */
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

			/* Perform the request, res will get the return code */
			res = curl_easy_perform(curl);
			/* Check for errors */
			if (res != CURLE_OK)
				fprintf(stderr, "curl_easy_perform() failed: %s\n",
					curl_easy_strerror(res));

			/* we are done... */
		}
		/* always cleanup */
		curl_easy_cleanup(curl);
	}
	curl_global_cleanup();

	return 0;
}

int
pretty_json_output(char *filename)
{
	yajl_handle	hand;
	static unsigned char fileData[65536];
	/* generator config */
	yajl_gen	g;
	yajl_status	stat;
	size_t		rd;
	int		retval = 0;
	FILE           *fp;

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
	return 0;
}


int
parse_handoff(unsigned char *buf, size_t len)
{
	/*
	 * buf of size len is provided from YAJL and then passed in via
	 * argument
	 */

	char           *loc = NULL;
	bool		between_quotes = 0;
	int		between_quotes_n = 0;
	char		out_buffer[len + 1];
	char           *out = out_buffer;
	char           *spare = out_buffer;
	char		sep = 94;

	/* Zero our output buffer */
	for (int j = 0; j < len; j++) {
		*spare = '\0';
		spare++;
	}

	spare = out;

	loc = buf;

	Dbg("Len is %zu", len);

	/*
	 * The below removes everyting except tag and value, adding a colon
	 * between the two by way of delimiter
	 */

	for (int j = 0; j < len; j++) {
		if (*loc == '"') {
			between_quotes = !between_quotes;

			if (between_quotes)
				between_quotes_n++;

			if (!between_quotes) {
				/*
				 * Hack to avoid empty fields and have a
				 * space instead
				 */
				if (*(out - 1) == sep) {
					*out = ' ';
					out++;
				}
				*out = sep;
				out++;
			}
		} else {
			if (between_quotes) {
				*out = *loc;
				out++;
			}
		}

		loc++;
	}

	/* out_buffer now contains all our buffer to tokenise */
	Dbg("Pre tokenising:");
	Dbg("%s", spare);

	char           *cp = NULL;
	char           *dp = NULL;
	char		tokens    [] = {sep, '\0'};
	pair		pairs     [between_quotes_n];
	//2 x as big as needed ? Not worried as essentially a buffer value
		bookmark bm[between_quotes_n];
	//Same comment
		int		count = 0;
	int		bm_count = 0;
	int		i = 0;
	bool		tag = true;
	bool		colour = true;

	cp = strtok(spare, tokens);
	Null_bookmarks(bm, between_quotes_n);

	/* TODO: remove strtok as depreciated */
	while (1) {
		if (cp == NULL)
			break;

		dp = strdup(cp);
		if (dp == NULL) {
			Ftl("Failed strdup");
		}
		if (tag) {
			pairs[count].tag = dp;
		} else {
			pairs[count].value = dp;
			count++;
		}

		tag = !tag;	/* Toggle tag and value */
		cp = strtok(NULL, tokens);
	}

	Dbg("Post tokenising:");
	for (int j = 0; j < count; j++) {
		if (strstr(pairs[j].tag, "href") || strstr(pairs[j].tag, "description") || strstr(pairs[j].tag, "tags") || strstr(pairs[j].tag, "hash")) {
			Dbgnl("%s:%s\n", pairs[j].tag, pairs[j].value);

			//Put the values in
				if (strstr(pairs[j].tag, "hash"))
				bm[i].hash = pairs[j].value;
			if (strstr(pairs[j].tag, "href"))
				bm[i].href = pairs[j].value;
			if (strstr(pairs[j].tag, "description"))
				bm[i].desc = pairs[j].value;
			if (strstr(pairs[j].tag, "tags"))
				bm[i].tags = pairs[j].value;

			if (strval(bm[i].hash) && strval(bm[i].href) && strval(bm[i].desc) && strval(bm[i].tags)) {
				i++;
				bm_count++;
			}
		}
	}

	Dbg("In bookmark array:");
	for (int j = 0; j < bm_count; j++) {
		printf("%s", clr[0]);
		printf("%s\n", bm[j].hash);
		printf("%s", clr[2]);
		printf("%s\n", bm[j].desc);
		printf("%s", clr[3]);
		printf("%s\n", bm[j].tags);
		printf("%s", clr[1]);
		printf("%s", bm[j].href);
		putchar('\n');
		putchar('\n');
	}

	/* Free bookmarks */

	for (int j = 0; j < count; j++) {
		free(pairs[j].tag);
		free(pairs[j].value);
	}

	return 0;
}

int
output(char *filename)
{
	yajl_handle	hand;
	static unsigned char fileData[65536];
	/* generator config */
	yajl_gen	g;
	yajl_status	stat;
	size_t		rd;
	int		retval = 0;
	int		a = 1;
	FILE           *fp;

	fp = fopen(filename, "r");
	if (!fp)
		return -1;
	else
		Dbg("Output open suceeded");

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
			Dbg("Handing off");
			parse_handoff(buf, len);
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
}


int
main(int argc, char *argv[])
{
	char           *filepath;
	char		colour_arg[] = "-c";
	char		msg_warn  [] =
	"\n"
	"NOTE: CURRENTLY STILL IN DEVELOPMENT\n"
	"Do not use.\n"
	"\n";

	char		msg_usage [] =
	"______________\n"
	"pinboard-shell\n"
	"______________\n"
	"\n"
	"Usage:\n"
	"-o output\n"
	"-c test colours\n"
	"-g get updated JSON file\n"
	"-t test JSON parser on already downloaded file\n"
	"-d turn debug mode on\n"
	"-u username arg\n"
	"-p password arg\n"
	"\n";

	error_mode = 's';	/* Makes Stopif use abort() */

	/* Quick and dirty arg check */
	/*
	 * for (int j = 0; j < argc; j++) if (strstr(argv[j], colour_arg) !=
	 * NULL) { test_colours(); return 0; }
	 */

	/*
	 * getopt loop --
	 * http://pubs.opengroup.org/onlinepubs/009696799/functions/getopt.htm
	 * l
	 */

	int		c;
	int		errflg = 0;
	int		exitflg = 0;
	int		opt_remote = 0;
	int		opt_download = 0;
	int		opt_test = 0;
	int		opt_output = 0;
	int		opt_warn = 1;
	char           *opt_username = NULL;
	char           *opt_password = NULL;
	char           *opt_verb = NULL;
	extern char    *optarg;
	extern int	optind, optopt;

	if (argc == 1) {
		fprintf(stderr, "%s", msg_usage);
		return 2;
	}
	while ((c = getopt(argc, argv, ":wgtdcu:p:o")) != -1) {
		switch (c) {
		case 'w':
			opt_warn = 0;
			break;
		case 'g':
			opt_remote++;
			opt_download++;
			break;
		case 'o':	/* TODO: make argument to an API verb? */
			opt_output++;
			break;
		case 't':
			opt_remote = 0;
			opt_test++;
			break;
		case 'd':
			opt_debug++;
			Dbg("Debug mode on");
			error_mode = 's';	/* Makes Stopif use abort() */
			break;
		case 'c':
			test_colours();
			exitflg++;
			break;
		case 'u':
			/*
			 * TODO: at each asprintf check size of input and
			 * return value
			 */
			asprintf(&opt_username, "%s", optarg);
			break;
		case 'p':
			asprintf(&opt_password, "%s", optarg);
			break;
		case ':':	/* -u or -p without operand */
			fprintf(stderr, "Option -%c requires an operand\n", optopt);
			errflg++;
			break;
		case '?':
			fprintf(stderr, "Unrecognized option: -%c\n", optopt);
			errflg++;
		}
	}
	if (errflg) {
		fprintf(stderr, "%s", msg_usage);
		return 2;
	}
	if (exitflg)		/* We have executed check function, so quit */
		return 0;

	if (opt_username && opt_password) {
		Print("Using:\n"
		      "%s as username\n"
		      "%s as password", opt_username, opt_password);

	} else if (opt_remote && (!opt_username || !opt_password)) {
		Se("You need to supply both a username and password");
		return -1;
	}
	if (opt_warn)
		puts(msg_warn);

	/*
	 * Steps > See if JSON file exists and is not old > If not, download
	 * with libcurl > Parse > Output > Other functions if required
	 */

	/* Look in ~/.pinboard and check we can write to it */

	asprintf(&filepath, "%s/%s", getenv("HOME"), ".pinboard");
	Dbg("Looking in %s", filepath);
	check_dir(filepath);
	free(filepath);

	asprintf(&filepath, "%s/%s", getenv("HOME"), ".pinboard/all.json");

	/* Download the JSON data */

	if (opt_download) {
		char           *url;
		asprintf(&url, "http://%s:%s@api.pinboard.in/v1/posts/all&format=json", opt_username, opt_password);
		/* TODO: check update time */

		if (!remove(filepath))
			Print("Deleted %s", filepath);

		make_file(filepath);

		Print("Using:\n"
		      "%s as filepath\n"
		      "%s as URL", filepath, url);

		curl_grab(url, filepath);
	}
	if (opt_test) {
		Dbg("Pretty output");
		pretty_json_output(filepath);
	}
	if (opt_output) {
		Dbg("Output");
		output(filepath);
	}
	free(filepath);
	return 0;
}
