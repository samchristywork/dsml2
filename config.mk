VERSION = $(shell grep DSML_VERSION src/version.h | cut -d'"' -f2)

PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man
