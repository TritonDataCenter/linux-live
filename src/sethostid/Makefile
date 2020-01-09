#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

#
# Copyright 2020 Joyent, Inc.
#

LDFLAGS		= -lz
CFLAGS		= -Werror -Wall
REPO_ROOT	= $(shell git rev-parse --show-toplevel)
PREFIX		= $(REPO_ROOT)/proto/usr/triton

all: sethostid

install: sethostid
	install -m 0755 sethostid $(PREFIX)/bin/sethostid

clean:
	-rm -f sethostid *.o core

.PHONY: install all clean
