# Makefile for lua-gtk2 (Linux)

# --- begin platform configuration ---
PLATFORM:=LINUX
EXTRALIB:=/usr/lib/libffi.a

# if you don't have cmph (http://sf.net/projects/cmph/), just comment this out.
CMPHDIR	:=/build/cmph-0.6/src

# Installation into these directories.
INDIR1	:=/usr/local/lib/lua/5.1
INDIR2	:=/usr/local/share/lua/5.1

# you probably don't want to change these...
LINK	:=1
MINGW	:=
ODIR	:=build/linux/
CFLAGS	:=
GTKLIB	:=-lgtk-x11-2.0
ODLL	:=gtk.so
# --- end platform configuration ---

all: dirs gtk2dll

include Makefile.common

# Build a tarball with the stripped library, the Lua library code and all the
# examples, plus an explanatory README file.

tar-bin: all
	(mkdir lua-gtk2-$(VERSION); \
	cp $(ODIR)$(ODLL) lua-gtk2-$(VERSION)/; \
	strip lua-gtk2-$(VERSION)/$(ODLL); \
	cp -a lib lua-gtk2-$(VERSION)/gtk; \
	cp examples/* lua-gtk2-$(VERSION); \
	cp doc/README.binary lua-gtk2-$(VERSION)/README; \
	tar czvf lua-gtk2-$(VERSION)-linux.tar.gz lua-gtk2-$(VERSION); \
	rm -r lua-gtk2-$(VERSION) \
	)