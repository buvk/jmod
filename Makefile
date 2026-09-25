# Use the 32-bit MinGW toolchain from MSYS2 or another MinGW-w64 install.
# Example: make CC="C:/msys64/mingw32/bin/i686-w64-mingw32-gcc.exe"
CC ?= i686-w64-mingw32-gcc
CPPFLAGS ?= -Isrc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra
LIBS = -luser32
SRC = src/jmod.c src/config.c src/item_labels.c src/quick_cast.c src/auto_gold.c src/rune_color.c src/loot_filter.c
HDR = $(wildcard src/*.h)
OUTDIR = build
PYTHON ?= python

.PHONY: all clean validate
all: $(OUTDIR)/D2Win.dll $(OUTDIR)/jmod.dll

$(OUTDIR):
	mkdir -p $@

$(OUTDIR)/D2Win.dll: $(SRC) $(HDR) D2Win.def | $(OUTDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -shared -o $@ $(SRC) D2Win.def $(LIBS)

$(OUTDIR)/jmod.dll: $(SRC) $(HDR) | $(OUTDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -shared -o $@ $(SRC) $(LIBS)

validate:
	$(PYTHON) tools/validate_items.py

clean:
	rm -rf $(OUTDIR)
