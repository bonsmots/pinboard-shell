#include <stdio.h>
#include <stdlib.h> //abort, getenv
#include <curl/curl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h> // open
#include <unistd.h> // close

#define Dbg(...) \
{ \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr, "\n"); \
}

#define Ftl(...) \
{ \
	Dbg(__VA_ARGS__); \
	abort(); \
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

int
check_dir(char *dirpath)
{
	/*
	 * > Does the folder exist? > No: create > Yes with wrong
	 * permissions: fatal complain > Yes with right permissions: continue
	 */
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
		Stopif(status != 0, return -1, "Make folder %s failed.", dirpath);
	}
	fildes = open(dirpath, O_RDONLY | O_DIRECTORY);
	status = fstat(fildes, &buffer);

	/* Is it a directory? Does the user own it? Is it writeable? */
	if (S_ISDIR(buffer.st_mode) && ((S_IRUSR & buffer.st_mode) == S_IRUSR) && ((S_IWUSR & buffer.st_mode) == S_IWUSR)) {
		Dbg("We are in business");
	} else {
		Dbg("Not a directory or not read/writable");
	}

	close(fildes);

	return 0;
}

int 
colours()
{
	int		colour_count = 15;
	char           *colours[] =
	{
		"\e[0;30m",
		"\e[0;34m",
		"\e[0;32m",
		"\e[0;36m",
		"\e[0;31m",
		"\e[0;35m",
		"\e[0;33m",
		"\e[0;37m",
		"\e[1;30m",
		"\e[1;34m",
		"\e[1;32m",
		"\e[1;36m",
		"\e[1;31m",
		"\e[1;35m",
		"\e[1;33m",
		"\e[1;37m"
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

int
main(int argc, char *argv[])
{
	char           *filepath;

	puts("Bonza!");
	error_mode = 's';	/* Makes Stopif use abort() */

	colours();
	return 0;

	/*
	 * Steps > See if JSON file exists and is not old > If not, download
	 * with libcurl > Parse > Output > Other functions if required
	 */

	asprintf(&filepath, "%s/%s", getenv("HOME"), ".pinboard");

	puts("Looking in:");
	puts(filepath);
	check_dir(filepath);

	return 0;
}
