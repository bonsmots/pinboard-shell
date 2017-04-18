pinboard-shell
==============

Shell client for pinboard.in

Currently work in progress -- not ready yet

---

If you want to try anyway:

Note: requires cmake and glib2. It also used the yajl library. For yajl see <https://github.com/lloyd/yajl>

With brew the following with suffice on OS X/macos (tested on fresh OS X 10.12.3)

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
