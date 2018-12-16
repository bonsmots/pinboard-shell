pinboard-shell
==============

If you want to try anyway:

Note: requires cmake and glib2. It also uses the yajl library. 

For yajl see <https://github.com/lloyd/yajl>

With `brew` the following with suffice on OS X/macos (tested on fresh OS X 10.12.3)

Pre-requisites:

	brew install glib
	brew cask install cmake

Build:

	git clone 'https://github.com/bonsmots/pinboard-shell'
	cd pinboard-shell
	git clone 'https://github.com/lloyd/yajl'
	cd yajl
	./configure && make
	cd ..
	make

Use:
  
  ./pb -h

  ______________
  pinboard-shell
  ______________

  Usage:
  ADD:
  -t Title -u "https://url.com/"
  DELETE:
  -r "string"
  SEARCH:
  -z "string"
  LIST:
  -o flag to list data
  -w do not output tags
  -c toggle tags only
  -p turn off formatting e.g. for redirecting stdout to a file
  UPDATE:
  -a auto update: updates if the API says it has updated since last downloaded
  -f force update: forces update
  OTHER:
  -v toggle verbose
  -d turn debug mode on
  -h this help

  Example usage:
  Download bookmarks file from pinboard.in
  ./pb -f
  OR
  Output only
  ./pb -o

  Further example usage:
  List most frequent tags:
  ./pb -ocp | sort | awk '{ print $NF }' | uniq -c | sort -nr | less
  List those tagged with $TAG for export to a file:
  ./pb -op | grep --color=never -B2 $TAG > $TAG.tagged  

Caveat:

Still being worked on.
