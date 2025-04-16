#include "macros.h"

#if !defined(YAJL_H)
#include "yajl/yajl_gen.h"
#include "yajl/yajl_parse.h"
#include "yajl/yajl_tree.h"
#define YAJL_H
#endif

/*
 * Helper functions
 */

/*
 * YAJL callback functions -- per the YAJL documentation
 */

static int reformat_null(void *ctx) {
  yajl_gen g = (yajl_gen)ctx;
  return yajl_gen_status_ok == yajl_gen_null(g);
}

static int reformat_boolean(void *ctx, int boolean) {
  yajl_gen g = (yajl_gen)ctx;
  return yajl_gen_status_ok == yajl_gen_bool(g, boolean);
}

static int reformat_number(void *ctx, const char *s, size_t l) {
  yajl_gen g = (yajl_gen)ctx;
  return yajl_gen_status_ok == yajl_gen_number(g, s, l);
}

static int reformat_string(void *ctx, const unsigned char *stringVal, size_t stringLen) {
  yajl_gen g = (yajl_gen)ctx;
  return yajl_gen_status_ok == yajl_gen_string(g, stringVal, stringLen);
}

static int reformat_map_key(void *ctx, const unsigned char *stringVal, size_t stringLen) {
  yajl_gen g = (yajl_gen)ctx;
  return yajl_gen_status_ok == yajl_gen_string(g, stringVal, stringLen);
}

static int reformat_start_map(void *ctx) {
  yajl_gen g = (yajl_gen)ctx;
  return yajl_gen_status_ok == yajl_gen_map_open(g);
}

static int reformat_end_map(void *ctx) {
  yajl_gen g = (yajl_gen)ctx;
  return yajl_gen_status_ok == yajl_gen_map_close(g);
}

static int reformat_start_array(void *ctx) {
  yajl_gen g = (yajl_gen)ctx;
  return yajl_gen_status_ok == yajl_gen_array_open(g);
}

static int reformat_end_array(void *ctx) {
  yajl_gen g = (yajl_gen)ctx;
  return yajl_gen_status_ok == yajl_gen_array_close(g);
}

// TODO: should explain
static yajl_callbacks callbacks = {reformat_null,
                                   reformat_boolean,
                                   NULL,
                                   NULL,
                                   reformat_number,
                                   reformat_string,
                                   reformat_start_map,
                                   reformat_map_key,
                                   reformat_end_map,
                                   reformat_start_array,
                                   reformat_end_array};


/**
 * @brief Print the usage message
 * 
 * @return int 
 */
int print_usage() {

  char msg_usage[] = "______________\n"
                     "pinboard-shell\n"
                     "______________\n"
                     "\n"
                     "Usage:\n"
                     "ADD:\n"
                     "-t Title -u \"https://url.com/\" \n"
                     "DELETE:\n"
                     "-r \"string\" \n"
                     "SEARCH:\n"
                     "-s \"string\"\n"
                     "LIST:\n"
                     "-o flag to list data\n"
                     "-w do not output tags\n"
                     "-c toggle tags only\n"
                     "-p turn off formatting e.g. for redirecting stdout to a file\n"
                     "UPDATE:\n"
                     "-a auto update: updates if the API says it has updated since last "
                     "downloaded\n"
                     "-f force update: forces update\n" /* TODO: check flow control */
                     "OTHER:\n"
                     "-v toggle verbose\n"
                     "-d turn debug mode on\n"
                     "-h this help\n"
                     "\n"
                     "Example usage:\n"
                     "Download bookmarks file from pinboard.in\n"
                     "./pb -f\n"
                     "OR\n"
                     "Output only\n"
                     "./pb -o\n"
                     "\n"
                     "Further example usage:\n"
                     "List most frequent tags:\n./pb -ocp | sort | awk '{ print $NF }' | uniq "
                     "-c | sort -nr | less\n"
                     "List those tagged with $TAG for export to a file:\n./pb -op | grep "
                     "--color=never -B2 $TAG > $TAG.tagged\n"
                     "\n";

    fprintf(se, "%s", msg_usage);

    return 2;
}

// TODO: should make this run-able via a switch
int test_colours() {
    int colour_count = 15;
    char *colours[] = {
        "\e[0;30m",
        "\e[0;34m", //+
        "\e[0;32m",
        "\e[0;36m", //+
        "\e[0;31m",
        "\e[0;35m", //+
        "\e[0;33m", "\e[0;37m", "\e[1;30m", "\e[1;34m", "\e[1;32m", "\e[1;36m", "\e[1;31m", "\e[1;35m", "\e[1;33m",
        "\e[1;37m" // +
    };

    char *colour_labels[] = {"Black            ", "Blue             ", "Green            ", "Cyan             ",
                             "Red              ", "Purple           ", "Brown            ", "Gray             ",
                             "Dark Gray        ", "Light Blue       ", "Light Green      ", "Light Cyan       ",
                             "Light Red        ", "Light Purple     ", "Yellow           ", "White            "};

    Trace();

    for (int i = 0; i < (colour_count - 1); i++) {
        printf("%s%s\n", colours[i], colour_labels[i]);
    }

    return 0;
}

// swapchars swaps one string of 1 or 2 characters with another
void swapchars(char find[3], char replace[3], char *string, int string_length)
{
  char *p = string;

  for (int i = 0; i < string_length; i++)
  {
    if (*p == find[0] && *(p + 1) == find[1])
    {
      *p = replace[0];
      *(p + 1) = replace[1];
    }
    p++;
  }
}

// swapchar swaps one char with another
void swapchar(char from, char to, char *string, int len)
{
  char *p = string;

  for (int i = 0; i < len; i++)
  {
    if (*p == from)
    {
      *p = to;
    }
    p++;
  }
}

// ask_string gets a string as input
// TODO: obviously not secure -- needs redoing in a secure way i.e. with the buffer length set
// not tested
char *ask_string(char *message, char *input)
{
  printf("%s", message);
  scanf("%s", input);
  return input;
}

// strval returns string length if not NULL
// tested in tests.c
int strval(char *str)
{
  if (str)
    return strlen(str);
  else
    return 0;
}
