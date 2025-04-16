#include <sys/stat.h>
#include <fcntl.h>   // open

// Crudely prints file to stdout
// UNUSED
int simple_output(char *filedir, char *verb) {
  char *filename;
  char buf;
  int fd;

  Trace();

  asprintf(&filename, "%s/%s.json", filedir, verb);

  fd = open(filename, O_RDONLY);
  if (!fd) {
    return -1;
  }

  // TODO: one char at a time maybe not that efficient, may check this
  while (read(fd, &buf, 1)) {
    putchar(buf);
  }

  close(fd);
  free(filename);

  return 0;
}