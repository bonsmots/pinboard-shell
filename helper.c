/*
 * Helper functions
 */

void swpch(char find[3], char replace[3], char *string, int string_length) {
  char *p = string;
  for (int i = 0; i < string_length; i++) {
    if (*p == find[0] && *(p + 1) == find[1]) {
      *p = replace[0];
      *(p + 1) = replace[1];
    }
    p++;
  }
}

void chch(char from, char to, char *string, int len) {
  char *p = string;
  for (int i = 0; i < len; i++) {
    if (*p == from) {
      *p = to;
    }
    p++;
  }
}

// ask_string gets a string as input
// TODO: obviously not secure -- needs redoing in a secure way i.e. with the buffer length set
// not tested
char *ask_string(char *message, char *input) {
  printf("%s", message);
  scanf("%s", input);
  return input;
}

// strval returns string length if not NULL
// tested in tests.c
int strval(char *str) {
  if (str)
    return strlen(str);
  else
    return 0;
}
