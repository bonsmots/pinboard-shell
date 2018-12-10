#include <stdlib.h>
#include <stdio.h>

int main() {
  char * s_user = "PB_USER";
  char * s_pass = "PB_PASS";
  printf("%s: %s\n", s_user, getenv(s_user));
  printf("%s: %s\n", s_pass, getenv(s_pass));
  return 0;
}
