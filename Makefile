# cdok -- Calcudoku solver/generator
# Copyright (C) 2012 Daniel Beer <dlbeer@gmail.com>
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

GIT_COMMIT_NAME := $(shell git rev-parse --verify --short HEAD)
GIT_STATUS := $(shell git status --porcelain)
BUILD_DATE := $(shell date)

ifeq ($(GIT_STATUS),)
	GIT_DIRTY =
else
	GIT_DIRTY = +
endif

CC ?= gcc
PREFIX ?= /usr/local
CDOK_CFLAGS = -O2 -Wall -DGIT_VERSION='"$(GIT_COMMIT_NAME)$(GIT_DIRTY)"' \
	      -DBUILD_DATE='"$(BUILD_DATE)"'

all: cdok

cdok: main.o cdok.o parser.o printer.o solver.o generator.o
	$(CC) -o $@ $^

clean:
	rm -f cdok
	rm -f *.o

%.o: %.c
	$(CC) $(CFLAGS) $(CDOK_CFLAGS) -o $*.o -c $*.c

install: cdok
	install -m 0755 -o root -g root cdok $(DESTDIR)$(PREFIX)/bin

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/cdok
