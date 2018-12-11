/*
 * Helper functions
 */

void swpch(char find[3], char replace[3], char *string, int string_length)
{
	char *p = string;
	for (int i = 0; i < string_length; i++)
	{
		if (*p == find[0] && *(p+1) == find[1])
		{
		   	*p = replace[0];
		   	*(p+1) = replace[1];
		}
		p++;
	}
}

// TODO: obviously not secure
char* ask_string(char *message, char *input)
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
