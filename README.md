pinboard-shell
==============

pb is a shell client for the excellent [pinboard.in](https://pinboard.in/) bookmarking service

Features:

- Fast
- Uses HTTPS to make API calls to [pinboard.in](https://pinboard.in/) as you would expect

Caveats:

- A bit rough around the edges

Looks like:

**Add**
![Delete](https://raw.githubusercontent.com/bonsmots/pinboard-shell/master/screen-add.png)
**Delete**
![Delete](https://raw.githubusercontent.com/bonsmots/pinboard-shell/master/screen-remove.png)
**Search**
![Search](https://raw.githubusercontent.com/bonsmots/pinboard-shell/master/screen-search.png)
**List**
![Output](https://raw.githubusercontent.com/bonsmots/pinboard-shell/master/screen-list.png)

With `brew` the following will suffice on OS X/MacOS. It should also work on linux etc -- substitute `brew` for your system's package manager.

Requires:

- cmake (broadly used and easily installed; see below)
- glib2 (ditto)
- [yajl](https://github.com/lloyd/yajl) (more detail below)

The below steps take care of these on `OSX`/`MacOS`

Note: if you are inside an anaconda environment run `conda deactivate` first or it will try to use the wrong compiler

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

Note: it will look for `$PB_USER` and `$PB_PASS` environment variables and prompt for login details if not found. The secure approach here is to set `$PB_USER` with `export PB_USER=pinboardusername` or similar in your `.bashrc` or similar, and it will then prompt you to enter your password when authentication is required.

````  
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
-s "string"
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
````
