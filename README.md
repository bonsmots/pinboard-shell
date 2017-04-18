pinboard-shell
==============

Shell client for pinboard.in

Currently work in progress -- not ready yet

---

If you want to try anyway:

Note: requires cmake and glib2

With brew the following with suffice on OS X/macos (tested on fresh OS X 10.12.3)

	brew install glib
	brew cask install cmake

For yajl see <https://github.com/lloyd/yajl>

	git clone 'https://github.com/bonsmots/pinboard-shell'
	cd pinboard-shell
	git clone 'https://github.com/lloyd/yajl'
	cd yajl
	./configure && make
	cd ..
	make
