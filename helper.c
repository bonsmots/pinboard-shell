/*
 * Helper functions
 */

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

void swpch(char find[3], char replace[3], char *string, int string_length)
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

void chch(char from, char to, char *string, int len)
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
