#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

#
# Copyright 2020 Joyent, Inc.
#

REPO_ROOT	= $(shell git rev-parse --show-toplevel)
PREFIX		= $(REPO_ROOT)/proto/usr/triton

all: debug release

release: target/release/sethostid
debug: target/debug/sethostid

target/release/sethostid: src/main.rs
	cargo build --release

target/debug/sethostid: src/main.rs
	cargo build

install: target/release/sethostid
	install -m 0755 $< $(PREFIX)/bin/sethostid

clean:
	-rm -r -f target

.PHONY: install all clean release debug
