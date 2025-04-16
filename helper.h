#ifndef HELPER_H
#define HELPER_H

#include <stdio.h>

int print_usage();
void swapchars(char find[3], char replace[3], char *string, int string_length);
void swapchar(char from, char to, char *string, int len);
char *ask_string(char *message, char *input);
int strval(char *str);

#include "helper.c"

#endif