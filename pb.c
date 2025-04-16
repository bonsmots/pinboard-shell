/*
 * pb a.k.a. pinboard-shell
 *
 * Lists bookmarks, adds, deletes bookmarks from pinboard.in from the shell
 *
 * Written in C99
 *
 * pinboard.in login required
 *
 * Uses libcurl, YAJL
 *
 * NOTE:
 * YAJL needs cmake.
 *
 * To install cmake on OSX with homebrew: brew cask install cmake
 *
 * To build yajl: cd yajl/ && ./configure && make
 *
 * To then build pb: cd .. && make
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
 * BUG/TODO: Incorrect user/pass not handled - should check for 401 response
 * BUG/TODO: Presupposes 'solarized' colour scheme on the terminal, which probably is not everyone
 * BUG/TODO: -oa and -ao do not result in the same behaviour
 */

/*
 * Headers -- global
 */

#if defined(__linux__)
#define _GNU_SOURCE // required on linux for asprintf
#endif

#include <fcntl.h>   // open
#include <stdbool.h> // C99 bool type
#include <stdio.h>   // stdout
#include <stdlib.h>  // abort, getenv
#include <string.h>  // strstr, strdup, strtok
#include <unistd.h>  // close, getopt, getpass

#include <time.h> // time types

/* Time externs */
extern char *tzname[];
extern long int timezone;
extern int daylight;

#include <curl/curl.h> // curl
#include <sys/stat.h>  // fstat
#include <sys/types.h> // time_t etc

/*
 * Headers -- local
 */

#include "helper.c"

/*
 * Option globals
 */

/*
 * More option globals
 * Note: not in the options struct for ease of use w/ macros
 */

extern bool opt_debug;
extern bool opt_super_debug;
extern bool opt_verbose;
extern bool opt_trace;

/*
 * Main options struct
 */

struct {
  bool remote;
  bool print_output;
  bool autoupdate;
  bool force_update;
  bool del;
  bool add;
  bool hashes;
  bool no_colours;
  bool no_tags;
  bool tags_only;

  char *username;
  char *password;
  char *add_url;
  char *add_title;
  char *search_str;
  int print_max_items;
} options;

/*
 * Other globals
 */

// This is the colour pallete that we use -- the first value is the colour reset
// TODO: maybe add choice of colour palletes, or at least a vanilla alternative
// The first code here is the reset code
char *clr[] = {"\e[0m", "\e[0;34m", "\e[0;36m", "\e[0;35m", "\e[1;37m"};

/* Struct to hold tag and value pairs from the JSON */
typedef struct {
  char *tag;
  char *value;
} pair;

/* Struct for bookmarks */
typedef struct {
  char *hash;
  char *href;
  char *desc;
  char *tags;
} bookmark;

// To keep track of the number of printed items
int printed_items = 0;

/**
 * @brief Check the environment variables for the username and password
 *
 * @return void
 */
void check_env_variables() {
  char *s_user = "PB_USER";
  char *s_pass = "PB_PASS";

  char *s_user_env;
  char *s_pass_env;

  s_user_env = getenv(s_user);
  s_pass_env = getenv(s_pass);

  Trace();

  Dbg("getenv %s: %s", s_user, s_user_env);
  Dbg("getenv %s: %s", s_pass, "****");

  if (s_user_env != NULL) {
    asprintf(&options.username, "%s", s_user_env);
  }

  if (s_pass_env != NULL) {
    asprintf(&options.password, "%s", s_pass_env);
  }

  return;
}

// Does directory exist with sensible permissions? If not, create it.
int check_working_dir(char *dirpath) {
  struct stat buffer;
  int status;
  int fildes;

  // Will print trace if opt_trace is on
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
     * S_IRWXU read, write, execute/search by owner
     * S_IRWXG read, * write, execute/search by group
     * S_IROTH read permission, others
     * S_IXOTH execute/search permission, others
     */

    // Stopif will abort based on a toggle in macros.h
    Stopif(status != 0, return -1, "Make folder %s failed.", dirpath);
  }

  /* Open, get status, close */
  fildes = open(dirpath, O_RDONLY | O_DIRECTORY);
  status = fstat(fildes, &buffer);
  close(fildes);

  /* Check directory, readable, writeable */
  if (S_ISDIR(buffer.st_mode) && ((S_IRUSR & buffer.st_mode) == S_IRUSR) && ((S_IWUSR & buffer.st_mode) == S_IWUSR)) {
    Dbg("Directory %s fine", dirpath);
  } else {
    Dbg("Not a directory OR not read/writable at %s", dirpath);
  }

  return 0;
}

/**
 * @brief Returns true if file exists else false
 *
 * Detailed description of what the function does, any side effects, or
 * implementation details worth mentioning.
 *
 * @param param1 Description of the first parameter.
 * @return Description of the return value.
 */
bool does_file_exist(char *filepath) {
  struct stat buffer;
  int status;
  int fildes;

  Trace();

  fildes = open(filepath, O_RDONLY);
  status = fstat(fildes, &buffer);
  close(fildes);

  if (status == -1) {
    V("%s does not seem to exist or is not readable", filepath);
    return false;
  } else {
    return true;
  }
}

// Does file exist with sensible permissions? If not, create it.
int make_file(char *filepath) {
  struct stat buffer;
  int status;
  int fildes;

  Trace();

  // Open read-only, get info, close
  fildes = open(filepath, O_RDONLY);
  status = fstat(fildes, &buffer);
  close(fildes);

  if (status == -1) {
    Dbg("Failed to open file %s, likley it doesn't exist or is not readable.", filepath);
    Dbg("Making file %s", filepath);

    // See if we can open for writing, this open and close will create(?)
    // TODO: should change this to tighter permissions
    fildes = open(filepath, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    /*
     * O_WRONLY write only
     * O_CREAT create
     * O_EXCL fail if file exists
     * S_IRUSR read permission, owner
     * S_IWUSR write permission, owner
     * S_IRGRP read permission, group
     * S_IROTH read permission, others
     */

    Stopif(fildes == -1, return -1, "Open file %s for writing has failed.", filepath);

    close(fildes);
  }

  // Open read-only again
  fildes = open(filepath, O_RDONLY);
  status = fstat(fildes, &buffer);
  close(fildes);

  /* Check regular file, readable, writeable */
  if (S_ISREG(buffer.st_mode) && ((S_IRUSR & buffer.st_mode) == S_IRUSR) && ((S_IWUSR & buffer.st_mode) == S_IWUSR)) {
    Dbg("We are in business");
  } else {
    Dbg("Not a file or not read/writable");
  }

  return 0;
}

/* From example at http://curl.haxx.se/libcurl/c/simple.html */
/* TODO: add check for 401 response */
/* TODO: check against https://curl.se/libcurl/c/https.html */

// Pulls file at URL using CURL, writes to filepath
int pinboard_api_download(char *url, char *filepath, char *username, char *password) {
  CURL *curl;
  CURLcode res;
  FILE *fp;
  bool curl_success;

  Trace();

  fp = fopen(filepath, "w");
  if (!fp) {
    return -1;
  }

  if (!strval(username) || !strval(password)) {
    return -1;
  }

  curl = curl_easy_init();
  if (curl) {
    curl_success = true;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_USERNAME, username);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, password);
    // Follow redirect
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) { /* Check for errors */
      fprintf(se, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }

    curl_easy_cleanup(curl); /* always cleanup */
  } else {
    curl_success = false;
  }

  fclose(fp);

  if (curl_success) {
    return 0;
  } else {
    return -1;
  }
}

// TODO: ?
int pinboard_api(char *url, char *username, char *password) {
  CURL *curl;
  CURLcode res;
  bool curl_success;

  Trace();

  // if both empty return -1
  if (!strval(username) || !strval(password)) {
    return -1;
  }

  curl = curl_easy_init();
  if (curl) {
    curl_success = true;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERNAME, username);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, password);

    res = curl_easy_perform(curl); /* Perform the request, res will get the return code */

    if (res != CURLE_OK) { /* Check for errors */
      fprintf(se, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }

    curl_easy_cleanup(curl); /* always cleanup */
  } else {
    curl_success = false;
  }

  if (curl_success) {
    return 0;
  } else {
    return -1;
  }
}

// Outputs filedir/verb.json to stdout, nicely formatted
int pretty_json_output(char *filedir, char *verb) {
  yajl_handle handle;

  /* 65kb buffer */
  static unsigned char fileData[65536];

  /* generator config */
  yajl_gen gen;
  yajl_status stat;
  size_t read_size;
  int return_val = 0;
  FILE *fp;
  char *filename;

  Trace();

  asprintf(&filename, "%s/%s.json", filedir, verb);

  fp = fopen(filename, "r");
  // TODO: feels like stuff like this should use Stopif?
  if (!fp) {
    return -1;
  }

  // Can specify our own memory allocator, NULL means use default
  gen = yajl_gen_alloc(NULL);

  // Nice-looking output
  yajl_gen_config(gen, yajl_gen_beautify, 1);

  // Check input is UTF-8
  yajl_gen_config(gen, yajl_gen_validate_utf8, 1);

  /* ok.  open file.  let's read and parse */
  handle = yajl_alloc(&callbacks, NULL, (void *)gen);

  /* and let's allow comments by default */
  yajl_config(handle, yajl_allow_comments, 1);

  for (;;) {
    read_size = fread((void *)fileData, 1, sizeof(fileData) - 1, fp);

    if (read_size == 0) {
      // if not EOF something funny going on, set return_val and break
      if (!feof(fp)) {
        fprintf(se, "error on file read.\n");
        return_val = 1;
      }
      break;
    }

    fileData[read_size] = 0;

    stat = yajl_parse(handle, fileData, read_size);

    if (stat != yajl_status_ok) {
      break;
    }

    {
      const unsigned char *buf;
      size_t len;

      yajl_gen_get_buf(gen, &buf, &len);
      fwrite(buf, 1, len, stdout);
      yajl_gen_clear(gen);
    }
  }

  stat = yajl_complete_parse(handle);

  if (stat != yajl_status_ok) {
    unsigned char *str = yajl_get_error(handle, 1, fileData, read_size);
    fprintf(se, "%s", (const char *)str);
    yajl_free_error(handle, str);
    return_val = 1;
  }

  yajl_gen_free(gen);
  yajl_free(handle);
  fclose(fp);

  free(filename);
  return return_val;
}

// API bits

int pinboard_api_del(char *username, char *password, char *webaddr) {
  char *url;
  char *escaped_url;
  int retval = -1;

  Trace();

  if (!strval(username) || !strval(password) || !strval(webaddr)) {
    return -1;
  }

  CURL *curl;
  curl = curl_easy_init();
  escaped_url = curl_easy_escape(curl, webaddr, 0);

  asprintf(&url, "https://api.pinboard.in/v1/posts/delete?url=%s&format=json", escaped_url);

  V("Using:\n%s as URL\n", url);

  retval = pinboard_api(url, username, password);

  free(url);
  curl_free(escaped_url);
  curl_easy_cleanup(curl); /* always cleanup */
  return retval;
}

int pinboard_api_add(char *username, char *password, char *url, char *title) {
  char *api_url;
  char *escaped_url;
  char *escaped_title;
  int retval = -1;
  Trace();

  if (!strval(username) || !strval(password) || !strval(url) || !strval(title)) {
    return -1;
  }

  CURL *curl;
  curl = curl_easy_init();
  escaped_url = curl_easy_escape(curl, url, 0);
  escaped_title = curl_easy_escape(curl, title, 0);

  asprintf(&api_url, "https://api.pinboard.in/v1/posts/add?format=json&url=%s&description=%s", escaped_url, escaped_title);

  V("Using:\n%s as URL\n", api_url);

  retval = pinboard_api(api_url, username, password);

  free(api_url);
  curl_free(escaped_url);
  curl_free(escaped_title);
  curl_easy_cleanup(curl); /* always cleanup */
  return retval;
}

// Wrapper for downloading with the API
int pinboard_api_download_wrapper(char *verb, char *username, char *password, char *directory) {
  char *url;
  char *filepath;

  Trace();

  // API endpoint
  asprintf(&url, "https://api.pinboard.in/v1/posts/%s&format=json", verb);
  // where we save the response to
  asprintf(&filepath, "%s/%s.json", directory, verb);

  /* TODO: check update time */

  if (!remove(filepath)) {
    V("Deleted %s", filepath);
  }

  make_file(filepath);

  V("Using: %s as filepath, %s as URL\n", filepath, url);

  pinboard_api_download(url, filepath, username, password);

  free(url);
  free(filepath);
  return 0;
}

void del_prompt(char *url) {
  char b1[512];
  puts(url);
  ask_string("Do you want to delete this URL (y/n/q?) ", b1);
  puts("");
  if (strcasestr(b1, "q")) {
    exit(0);
  }
  if (strcasestr(b1, "y")) {
    pinboard_api_del(options.username, options.password, url);
  }
}

int json_parse_and_print(unsigned char *buf, size_t len) {
  /*
   * buf of size len is provided from YAJL and then passed in via
   * argument
   * Note: relies on opt_tags
   */

  unsigned char *loc = NULL;
  bool between_quotes = 0;
  int between_quotes_n = 0;
  char out_buffer[len + 1];
  char *out = out_buffer;
  char *spare = out_buffer;
  char sep = 94; // ^ character
  unsigned char *loc2;
  unsigned char *loc3;

  Trace();

  Dbg("%zu bytes (%zu kbs) of JSON data", len, len / 1000);

  /* Zero our output buffer */
  for (int j = 0; j < len; j++) {
    *spare = '\0';
    spare++;
  }

  spare = out;
  loc = loc2 = loc3 = buf;

  /* Replace separator signifier with something else to avoid confusing the
   * seperator logic later on */
  swapchar(sep, '-', (char *)loc2, len);

  /* Replace \" within any of the fields with ' as the former breaks the parser
   */
  swapchars("\\\"", "\'\'", (char *)loc3, len);

  /*
   * The below removes everyting except tag and value, adding sep
   * between the two by way of delimiter
   */

  for (int j = 0; j < len; j++) {
    if (*loc == '"') {
      /* When we hit the double quote char, toggle the bool to reflect the fact
       * that we are entering or leaving a quoted string */
      between_quotes = !between_quotes;
      /* If entering a quoted string, increment the count of quoted strings */
      if (between_quotes) {
        between_quotes_n++;
      }

      /* Out is a pointer to output buffer so we can do pointer arithmatic and
       * therefore move about */
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

  char *cp = NULL;
  char *dp = NULL;
  char tokens[] = {sep, '\0'};
  pair pairs[between_quotes_n];         // TODO: 2 x as big as needed ? Not worried as essentially a buffer value
  bookmark bookmarks[between_quotes_n]; // Same comment
  int count = 0;
  int bm_count = 0;
  int i = 0;
  bool tag = true;

  cp = strtok(spare, tokens); /* Breaks string into tokens -- first token */
  Null_bookmarks(bookmarks, between_quotes_n);

  /* TODO: remove strtok as depreciated */
  while (1) {
    if (cp == NULL) {
      break;
    }

    dp = strdup(cp); /* strdup returns pointed to malloc'd copy, thus dp is a new string */
    if (dp == NULL) {
      Ftl("Failed strdup, likely malloc issue");
    }

    /* If we are not in tag we are in value -- point the tag or value pointer to
     * the new string */
    if (tag) {
      pairs[count].tag = dp;
    } else {
      pairs[count].value = dp;
      count++;
    }

    tag = !tag;                /* Toggle tag and value */
    cp = strtok(NULL, tokens); /* Breaks string into tokens -- subsequent tokens */
  }

  Dbg("Tokenising complete on %i strings", count);

  // TODO: remove
  if (opt_super_debug)
    for (int j = 0; j < count; j++) {
      printf("%s %s\n", pairs[j].tag, pairs[j].value);
    }

  for (int j = 0; j < count; j++) {
    /* If any of the tag strings are the strings we are interested in */
    if (strstr(pairs[j].tag, "href") || strstr(pairs[j].tag, "description") || strstr(pairs[j].tag, "tags") ||
        strstr(pairs[j].tag, "hash")) {

      /* Put the values in
       * strstr() returns the pointer to first occurrence of substring in the string
       * Point the bm struct pointers to the values in the pair structs */

      if (strstr(pairs[j].tag, "hash")) {
        bookmarks[i].hash = pairs[j].value;
      }
      if (strstr(pairs[j].tag, "href")) {
        bookmarks[i].href = pairs[j].value;
      }
      if (strstr(pairs[j].tag, "description")) {
        bookmarks[i].desc = pairs[j].value;
      }
      if (strstr(pairs[j].tag, "tags")) {
        bookmarks[i].tags = pairs[j].value;
      }

      /* If now all non-null values, increment */
      /* bm_count used when we print to screen so know when done */
      if (strval(bookmarks[i].hash) && strval(bookmarks[i].href) && strval(bookmarks[i].desc) && strval(bookmarks[i].tags)) {
        i++;
        bm_count++;
      }
    }
  }

  if (!bm_count) {
    Dbg("bm_count is 0");
    return 0; /* Saves below being executed with 0 */
  }

  Dbg("In bookmark array with %i bookmarks", bm_count);

  if (options.print_max_items > 0) {
    Dbg("Only printing max %i items", options.print_max_items);
  } else {
    // Hacky garbage value so it doesn't interfere
    options.print_max_items = bm_count;
  }

  /*
   * OUTPUT LOOP
   */

  for (int j = 0; j < bm_count && printed_items < options.print_max_items; j++) {
    printed_items += 1;
    // If this is set we are searching, otherwise we are listing
    if (strval(options.search_str)) {
      if ((strcasestr(bookmarks[j].hash, options.search_str) != NULL) || (strcasestr(bookmarks[j].desc, options.search_str) != NULL) ||
          (strcasestr(bookmarks[j].href, options.search_str) != NULL)) {
        if (options.no_colours) {
          if (options.hashes) {
            printf("%s\n", bookmarks[j].hash);
          }
          printf("%s\n", bookmarks[j].desc);
          printf("%s\n", bookmarks[j].href);
        } else {
          if (options.hashes) {
            printf("%s%s%s\n", clr[1], bookmarks[j].hash, clr[0]);
          }
          printf("%s%s%s\n", clr[3], bookmarks[j].desc, clr[0]);
          printf("%s%s%s\n", clr[2], bookmarks[j].href, clr[0]);
        }
        if (options.del) {
          del_prompt(bookmarks[j].href);
        }
      }
    } else {
      /* No colours version */
      if (options.no_colours) {
        // printf("%s%s%s\n", clr[1], bm[j].hash, clr[0]);
        if (!options.tags_only) {
          if (options.hashes) {
            printf("%s\n", bookmarks[j].hash);
          }
          printf("%s\n", bookmarks[j].desc);
          printf("%s\n", bookmarks[j].href);
        }
        /* Note we exclude tag string if short enough to be empty */
        if (!options.no_tags && (strlen(bookmarks[j].tags) > 1)) {
          printf("%s\n", bookmarks[j].tags);
        }
      } else {
        // printf("%s%s%s\n", clr[1], bm[j].hash, clr[0]);
        if (!options.tags_only) {
          if (options.hashes) {
            printf("%s%s%s\n", clr[1], bookmarks[j].hash, clr[0]);
          }
          printf("%s%s%s\n", clr[3], bookmarks[j].desc, clr[0]);
          printf("%s%s%s\n", clr[2], bookmarks[j].href, clr[0]);
        }
        /* Note we exclude tag string if short enough to be empty */
        if (!options.no_tags && (strlen(bookmarks[j].tags) > 1)) {
          printf("%s%s%s\n", clr[4], bookmarks[j].tags, clr[0]);
        }
      }
    }
    // putchar('\n');
  }

  /* Free strings created with stdup */
  for (int j = 0; j < count; j++) {
    free(pairs[j].tag);
    free(pairs[j].value);
  }

  return 0;
}

// Sets up YAJL then sends to parse_handoff
int print_json_output(char *filedir, char *verb) {
  yajl_handle hand;
  /* TODO: find out how large the file is and make buffer the right size
   * accordingly */

  static unsigned char fileData[65536 * 5];
  int retval = 0;

  /* generator config */
  yajl_gen g;
  yajl_status stat;
  size_t rd;
  FILE *fp;
  char *filename;

  Trace();

  asprintf(&filename, "%s/%s.json", filedir, verb);

  fp = fopen(filename, "r");
  if (!fp) {
    return -1;
  } else {
    Dbg("JSON input found");
  }

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

    if (stat != yajl_status_ok) {
      break;
    }

    {
      const unsigned char *buf;
      size_t len;

      yajl_gen_get_buf(g, &buf, &len);
      Dbg("Parsing");
      json_parse_and_print((unsigned char *)buf, len);
      yajl_gen_clear(g);
    }
  }

  stat = yajl_complete_parse(hand);
  if (stat != yajl_status_ok) {
    unsigned char *str = yajl_get_error(hand, 1, fileData, rd);
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
char *file_to_mem(char *directory, char *verb, int *size) {
  char *filepath;
  char *data;
  int fildes;
  int bytes_read = 1;
  struct stat buffer;

  Trace();

  asprintf(&filepath, "%s/%s.json", directory, verb);
  V("Opening file %s", filepath);
  fildes = open(filepath, O_RDONLY);

  if (fildes < 0) {
    Ftl("Cannot open %s", filepath);
  }

  fstat(fildes, &buffer);
  close(fildes);

  *size = buffer.st_size;
  V("%lld bytes size, %d bytes", buffer.st_size, *size);
  if (buffer.st_size > 1000000) {
    // If more than a meg something is going wrong, bail
    Ftl("File is %lld big which doesn't make sense, bailing", buffer.st_size);
  } else {
    data = malloc(buffer.st_size + 1);
  }

  fildes = open(filepath, O_RDONLY);

  while (bytes_read > 0) {
    bytes_read = read(fildes, data, buffer.st_size);
  }

  close(fildes);
  free(filepath);

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

void simple_parse_field(int field, char delim, char *data_from, char *data_to, int max) {
  char *loc, *out;
  bool in_field = false;
  int in_field_count = 0;

  Trace();

  loc = data_from;
  out = data_to;

  for (int j = 0; j < max; j++) {
    if (*loc == delim) {
      in_field = !in_field;

      if (in_field) {
        in_field_count++;
      }
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

/**
 * @brief Converts an ISO 8601 formatted date string to a time_t value
 *
 * This function parses an ISO 8601 formatted date string (YYYY-MM-DDThh:mm:ssZ)
 * and converts it to a time_t value. The function assumes the input time is in GMT,
 * which is consistent with the Pinboard API's behavior.
 *
 * @param iso8601_str A string containing the ISO 8601 formatted date
 * @return time_t The converted time value
 *
 * @see http://stackoverflow.com/questions/26895428/how-do-i-parse-an-iso-8601-date-with-optional-milliseconds-to-a-struct-tm-in-c
 */
time_t convert_iso8601(char *iso8601_str) {
  int year, month, day, hour, minute;
  float second;
  struct tm time;

  Trace();

  // Extract relevent values from the ISO 8601 string
  sscanf(iso8601_str, "%d-%d-%dT%d:%d:%fZ", &year, &month, &day, &hour, &minute, &second);

  // Fill out the tm struct
  time.tm_year = year - 1900; // Year since 1900
  time.tm_mon = month - 1;    // 0-11
  time.tm_mday = day;         // 1-31
  time.tm_hour = hour;        // 0-23
  time.tm_min = minute;       // 0-59
  time.tm_sec = (int)second;  // 0-61 (0-60 in C++11)
  // time.tm_isdst = 0;

  // Return the time_t value
  return mktime(&time);
}

/**
 * @brief Get the last modified time of a file
 *
 * @param filepath The path to the file
 * @return time_t The last modified time of the file
 */
time_t last_modified_time(char *filepath) {
  int fd;
  struct stat stat_;

  Trace();
  V("Opening file %s", filepath);

  fd = open(filepath, O_RDONLY);
  if (fd < 0) {
    Ftl("Cannot open %s", filepath);
  }

  fstat(fd, &stat_);
  close(fd);

  return stat_.st_mtime;
}

/* Get age of file -- returns 'age' (i.e. time since last modified) of file in
 * seconds as a double */
// TODO: API uses GMT, localtime seems to be given in BST, so a delta will be
// shown even if there is none

double time_since_last_modified(char *file) {
  struct tm file_tm;
  struct tm current_tm;
  time_t file_last_mod;
  time_t current_time;

  char buffer[512];
  double seconds_age;

  Trace();

  file_last_mod = last_modified_time(file);

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

bool is_file_more_recent(char *file, struct tm dt_tm) {
  time_t file_last_mod;
  struct tm file_tm;
  char buffer[512];
  double delta;

  Trace();

  file_last_mod = last_modified_time(file);
  localtime_r(&file_last_mod, &file_tm);

  Zero_char(buffer, sizeof(buffer));
  strftime(buffer, sizeof(buffer), "%A %F %T %Z", &file_tm);
  V("Update time %s", buffer);

  // TODO: this seems not to work properly
  delta = difftime(mktime(&file_tm), mktime(&dt_tm));
  V("Delta in seconds %.0f, minutes %.2f, hours %.2f", delta, delta / 60.0, delta / 60.0 / 60.0);

  return (delta > 0);
}

// Auto update all.json if update.json (which is cached for 5 mins) suggests
// this is needed
int auto_update(char *filepath, char *username, char *password) {
  char *d;
  char b1[1024] = {0};
  char buffer[512];
  double time_since_update;
  int d_len;

  time_t api_update_time;
  struct tm api_update_time_tm;

  Trace();

  /* Check we have all.json */
  Zero_char(buffer, sizeof(buffer));
  snprintf(buffer, sizeof(buffer), "%s/%s", filepath, "all.json");

  if (!does_file_exist(buffer)) {
    V("%s doesn't seem to exist, pulling", buffer);
    pinboard_api_download_wrapper("all", username, password, filepath);
  }

  /* Check when update.json last updated */
  Zero_char(buffer, sizeof(buffer));
  snprintf(buffer, sizeof(buffer), "%s/%s", filepath, "update.json");

  /* If it doesn't exist, download it */
  if (!does_file_exist(buffer)) {
    V("Downloading")
    pinboard_api_download_wrapper("update", username, password, filepath);
  }

  time_since_update = time_since_last_modified(buffer);

  /* D/l if stale */
  if (time_since_update >= (5 * 60)) { /* If greater than 5 mins have elapsed */
    V("Update info is >5 mins old, updating");
    pinboard_api_download_wrapper("update", username, password, filepath);
  } else {
    V("Update info is <5 mins old, not updating");
  }

  /* See when update.json says we last updated */
  d = file_to_mem(filepath, "update", &d_len);
  simple_parse_field(2, '"', d, b1, strlen(d));
  free(d);

  api_update_time = convert_iso8601(b1);
  localtime_r(&api_update_time, &api_update_time_tm);

  Zero_char(buffer, sizeof(buffer));
  strftime(buffer, sizeof(buffer), "%A %F %T %Z", &api_update_time_tm);
  V("API tells us we last updated on %s (note this is cached so info could be "
    "up to 5 mins old)",
    buffer);

  /* Check when all.json last updated */
  Zero_char(buffer, sizeof(buffer));
  snprintf(buffer, sizeof(buffer), "%s/%s", filepath, "all.json");
  if (is_file_more_recent(buffer, api_update_time_tm)) {
    V("We are up to date according to API");
  } else {
    V("We are not up to date according to API, updating");
    pinboard_api_download_wrapper("all", username, password, filepath);
    /* We also delete update.json to force this to be updated next time run */
    Zero_char(buffer, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s/%s", filepath, "update.json");
    if (remove(buffer)) {
      Ftl("Failed to delete %s", buffer);
    } else {
      V("%s deleted", buffer);
    }
  }

  return 0;
}

void force_update(char *filepath, char *username, char *password) {
  char buffer[512];
  Trace();

  V("Deleting cached info and re-downloading");

  /* Delete update.json and re-download */
  Zero_char(buffer, sizeof(buffer));
  snprintf(buffer, sizeof(buffer), "%s/%s", filepath, "update.json");
  if (remove(buffer)) {
    // V("Failed to delete %s", buffer);
  } else {
    V("%s deleted", buffer);
    pinboard_api_download_wrapper("update", username, password, filepath);
  }

  /* Delete all.json and re-download */
  Zero_char(buffer, sizeof(buffer));
  snprintf(buffer, sizeof(buffer), "%s/%s", filepath, "all.json");
  if (remove(buffer)) {
    V("Failed to delete %s", buffer);
  } else {
    V("%s deleted", buffer);
    pinboard_api_download_wrapper("all", username, password, filepath);
  }

  return;
}

char *get_working_directory() {
  // See
  // https://pubs.opengroup.org/onlinepubs/007904975/functions/getcwd.html

  long size;
  char *buf;
  char *ptr = NULL;

  size = pathconf(".", _PC_PATH_MAX);

  if ((buf = (char *)malloc((size_t)size)) != NULL) {
    ptr = getcwd(buf, (size_t)size);
  }

  return ptr;
}

int main(int argc, char *argv[]) {
  /* Pointer to string of filepath where working files are located */
  char *working_filepath = NULL;
  // Used with getopt
  int c;
  // Toggle to show usage
  bool show_usage = false;

  /* default is we assume do not need to make request */
  options.remote = false;
  // stdout is used for output
  options.print_output = false;
  // auto update is disabled
  options.autoupdate = false;
  // username and password are not set
  options.username = NULL;
  options.password = NULL;
  // force update is disabled
  options.force_update = false;

  /*
   * Default options
   */

  options.del = false;
  options.add = false;
  options.hashes = false;
  options.no_colours = false;
  options.no_tags = false;
  options.tags_only = false;
  options.print_max_items = 0; // 0 means all

  /* Required for getopt */
  extern char *optarg;
  extern int optind, optopt;

  /* Send stderr to stdout */
  se = stdout;
  // se = stderr;

  /* Let's cheekily see what the current working directory is, and if it's a development directory, we will turn on verbose and debug */
  char *working_directory = get_working_directory();
  if (strstr(working_directory, "pinboard-shell")) {
    opt_verbose = true;
    opt_debug = true;
    opt_trace = true;
    options.print_max_items = 5;
  }
  Safe_free(working_directory);

  // TODO: add print_max arg

  /* getopt loop per example at
   * http://pubs.opengroup.org/onlinepubs/009696799/functions/getopt.html */
  while ((c = getopt(argc, argv, ":zafcovdhwzpu:t:s:r:")) != -1) {
    switch (c) {
    // (S)earch
    case 's':
      options.print_output = true;
      options.remote = false; // EDITED
      opt_verbose = false;    // Required to work properly
      asprintf(&options.search_str, "%s", optarg);
      Dbg("Searching");
      break;
    // (A)uto update
    case 'a':
      options.autoupdate = true;
      options.remote = true;
      Dbg("Autoupdate");
      break;
    // (F)orce update
    case 'f':
      options.force_update = true;
      options.autoupdate = false;
      options.remote = true;
      Dbg("Forced update");
      break;
    // (P)rint
    case 'p':
      options.no_colours = true;
      Dbg("Printing with no colours");
      break;
    // ?
    case 'c':
      options.tags_only = true;
      Dbg("Tags only");
      break;
    // (W)ithout tags
    case 'w':
      options.no_tags = true;
      Dbg("No tags");
      break;
    // (O)utput
    case 'o':
      options.print_output = true;
      options.autoupdate = false;
      Dbg("Output toggled");
      break;
    // ?
    case 'z':
      options.remote = true;
      // opt_super_debug = true;
      // Dbg("Superdebug toggled");
      break;
    // (V)erbose
    case 'v':
      opt_verbose = !opt_verbose;
      Dbg("Verbose toggled");
      break;
    // (D)ebug
    case 'd':
      opt_verbose = true;
      opt_trace = true;
      opt_debug = true;
      error_mode = 's'; /* Makes Stopif use * abort() */
      Dbg("Debug mode set");
      break;
    // (H)elp
    case 'h':
      show_usage = true; // i.e. show help
      break;
    // Delete
    case 'r':
      options.remote = true;
      options.del = true;
      options.print_output = true; // required to run through the loop
      asprintf(&options.search_str, "%s", optarg);
      Dbg("Deleting");
      break;
    // (T)itle for addition of new bookmark
    case 't':
      options.remote = true;
      options.add = true;
      asprintf(&options.add_title, "%s", optarg);
      break;
    // (U)rl for addition of new bookmark
    case 'u':
      options.remote = true;
      options.add = true;
      asprintf(&options.add_url, "%s", optarg);
      break;
    // (:) Option requires argument without argument
    case ':':
      fprintf(se, "Option -%c requires an operand\n", optopt);
      show_usage = true;
      break;
    // (?) Unrecognized option
    case '?':
      fprintf(se, "Unrecognized option: -%c\n", optopt);
      show_usage = true;
    }
  }

  /* Print what toggles have been toggled */
  if (opt_verbose) {
    V("Verbose mode on");
  }

  if (opt_trace) {
    V("Tracing mode on");
  }

  if (opt_debug) {
    V("Debugging mode on");
  }

  check_env_variables(); /* Checks env variables for user/pass combo */

  /* Some issue -- print usage message */
  if (show_usage) {
    return print_usage();
  }

  tzset(); /* Set timezone */
  Dbg("tzname: %s,%s timezone: %li daylight: %i", tzname[0], tzname[1], timezone, daylight);

  /* Username and password are set */
  if (options.username && options.password) {
    Dbg("Using: %s as username, %s as password", options.username, "****");
    /* Ask for them if not set and we are needing to connect */
  } else if (options.remote && (!options.username || !options.password)) {
    char b1[512];
    if (!options.username) {
      Zero_char(b1, sizeof(b1));
      ask_string("Username: ", b1);
      asprintf(&options.username, "%s", b1);
    }
    if (!options.password) {
      Zero_char(b1, sizeof(b1));
      /* NOTE: getpass is depreciated */
      options.password = getpass("Password: ");
      // asprintf(&password, "%s", b1);
    }
  }

  /* If these are both set we want to add and then quit */
  if (strval(options.add_url) && strval(options.add_title)) {
    pinboard_api_add(options.username, options.password, options.add_url, options.add_title);
  }

  /*
   * Steps > See if JSON file exists and is not old
   * > If not, download with libcurl
   * > Parse
   * > Output
   * > Other functions if required
   */

  /* Look in ~/.pinboard and check we can write to it */
  asprintf(&working_filepath, "%s/%s", getenv("HOME"), ".pinboard");

  // Dbg("Looking in %s", filepath);
  check_working_dir(working_filepath);

  if (options.force_update && options.remote) {
    force_update(working_filepath, options.username, options.password);
  }

  if (options.autoupdate && options.remote) {
    auto_update(working_filepath, options.username, options.password);
  }

  // Print any output
  // "all" is the appropriate verb for the API
  if (options.print_output) {
    print_json_output(working_filepath, "all");
  }

  // Print something to say we're done
  if (options.del || options.force_update || options.add || options.autoupdate) {
    V("Done");
  }

  /* Free allocated memory */
  Safe_free(working_filepath);
  Safe_free(options.username);
  Safe_free(options.password);

  return 0;
}
