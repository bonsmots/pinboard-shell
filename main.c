#include <stdio.h>
#include <stdlib.h> //abort, getenv
#include <curl/curl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h> // open
#include <unistd.h> // close
#include <string.h> // strstr

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
test_colours()
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

int curl_grab(char *url, char *buffer_file)
{

}

/* From example at http://curl.haxx.se/libcurl/c/simplessl.html */

int curl_grab_ssl(char *url, char *buffer_file)
{
  int i;
  CURL *curl;
  CURLcode res;
  FILE *headerfile;
  const char *pPassphrase = NULL;
 
  static const char *pCertFile = "testcert.pem";
  static const char *pCACertFile="cacert.pem";
 
  const char *pKeyName;
  const char *pKeyType;
 
  const char *pEngine;
 
#ifdef USE_ENGINE
  pKeyName  = "rsa_test";
  pKeyType  = "ENG";
  pEngine   = "chil";            /* for nChiper HSM... */ 
#else
  pKeyName  = "testkey.pem";
  pKeyType  = "PEM";
  pEngine   = NULL;
#endif
 
  headerfile = fopen("dumpit", "w");
 
  curl_global_init(CURL_GLOBAL_DEFAULT);
 
  curl = curl_easy_init();
  if(curl) {
    /* what call to write: */ 
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.pinboard.in/v1/posts/all");
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, headerfile);
 
    for(i = 0; i < 1; i++) /* single-iteration loop, just to break out from */ 
    {
      if (pEngine)             /* use crypto engine */ 
      {
        if (curl_easy_setopt(curl, CURLOPT_SSLENGINE,pEngine) != CURLE_OK)
        {                     /* load the crypto engine */ 
          fprintf(stderr,"can't set crypto engine\n");
          break;
        }
        if (curl_easy_setopt(curl, CURLOPT_SSLENGINE_DEFAULT,1L) != CURLE_OK)
        { /* set the crypto engine as default */ 
          /* only needed for the first time you load
 *              a engine in a curl object... */ 
          fprintf(stderr,"can't set crypto engine as default\n");
          break;
        }
      }
      /* cert is stored PEM coded in file... */ 
      /* since PEM is default, we needn't set it for PEM */ 
      curl_easy_setopt(curl,CURLOPT_SSLCERTTYPE,"PEM");
 
      /* set the cert for client authentication */ 
      curl_easy_setopt(curl,CURLOPT_SSLCERT,pCertFile);
 
      /* sorry, for engine we must set the passphrase
 *          (if the key has one...) */ 
      if (pPassphrase)
        curl_easy_setopt(curl,CURLOPT_KEYPASSWD,pPassphrase);
 
      /* if we use a key stored in a crypto engine,
 *          we must set the key type to "ENG" */ 
      curl_easy_setopt(curl,CURLOPT_SSLKEYTYPE,pKeyType);
 
      /* set the private key (file or ID in engine) */ 
      curl_easy_setopt(curl,CURLOPT_SSLKEY,pKeyName);
 
      /* set the file with the certs vaildating the server */ 
      curl_easy_setopt(curl,CURLOPT_CAINFO,pCACertFile);
 
      /* disconnect if we can't validate server's cert */ 
      curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
 
      /* Perform the request, res will get the return code */ 
      res = curl_easy_perform(curl);
      /* Check for errors */ 
      if(res != CURLE_OK)
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
main(int argc, char *argv[])
{
	char           *filepath;
	char colour_arg[] = "-c";

	puts("Bonza!");
	error_mode = 's';	/* Makes Stopif use abort() */

	/* TODO: replace with getopt */
	for (int j = 0; j < argc; j++)
		if (strstr(argv[j], colour_arg) != NULL)
		{
			test_colours();
			return 0;
		}

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
