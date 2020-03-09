#! /bin/sh

#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

#
# Copyright 2020 Joyent, Inc.
#

# The version of do_netsetup() that comes with the system, found in
# /usr/lib/live/boot/9990-networking.sh, does not respect the command
# line arguments that initramfs-tools uses.  Rather than trying to fix the
# broken version, we simply call configure_networking that is delivered by
# initramfs-tools in [/usr/share/initramfs-tools]/scripts/functions.

do_netsetup ()
{
	configure_networking
}
