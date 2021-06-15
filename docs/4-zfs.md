<!--
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
-->

<!--
    Copyright 2020 Joyent, Inc
-->

# ZFS integration

To promote consistency and reuse across compute node types, ZFS is used on Linux
compute nodes.  The argument for consistency with the Triton ecosystem should
not be taken as an indication that there is a compromise: ZFS is a great file
system that is well-suited for the task at hand.

This document describes how ZFS is used and limitations it imposes.

## Dataset hierarchy

### system pool name

In SmartOS, it made sense to call the pool `zones`.  Here, not so much.  Better
names may be:

* system:  Triton expects to use a system pool.  Naming a pool that is somewhat
  attractive, but there's nothing saying that a pool with this name would be the
  system pool.  Maybe that would be confusing.  Also, if this pool is imported
  while running SmartOS, `/system` is already in use.
* sys: Similar to the previous option, but that conflicts with `/sys` on Linux.
* triton: Unlikely to be in use by anything else and is perfect until the next
  rebranding.

However, consistency with SmartOS can still be highly beneficial. E.g., being
able to run:

```bash
sdc-oneachnode -c 'zfs get compression zones'
```

allows operators to not have to distinguish which system type each CN is.
For this reason the default pool name shall remain `zones`.

### Mounts over root fs directories

There are some file systems that should be mounted over well-known directories
in the root file system so that systemd and perhaps other components can make
use of them.  For instance, persistent network configuration is likely to reside
in `/etc/systemd/network`.

The current plan is to create a hierarchy under the `zones` root, with file
systems under that mounting over directories in the root file system, using a
similar layout as SmartOS.

Some examples:

```bash
zfs create -o mountpoint=/zones zones
zfs create -o mountpoint=/opt zones/opt
zfs create -o mountpoint=/var/lib/lxd zones/lxd
```

## Boot integration

debian-live images boot only from PXE, ISO, or USB.

When the tools in this repository are used to build a debian-live image, the
non-dkms version of ZFS is installed in `live/filesystem.squashfs` root file
system.  This means that systemd services found in the squashfs archive are
responsible for importing the pool and mounting file systems.  Since there are
some systemd services that are defined in files defined or altered by files
that exist in the system zpool, there are *hook scripts* and *boot scripts*, as
described in `initramfs-tools(7)`, that ensure that the persistent data stored
in ZFS is mounted before systemd starts.  In particular:

* [sethostid](../src/sethostid.rs) is run to set the hostid to a hash of the
  system's UUID.  This is the same system UUID that is used by Triton.
* The [triton](../proto/usr/share/initramfs-tools/hooks/triton) *hook script*
  ensures that `sethostid`, `dmidecode`, and any libraries they require are in
  `initrd`.
* The [syspool](../proto/usr/share/initramfs-tools/scripts/live-bottom/syspool)
  *boot script* runs at boot to find the system pool, import it, and mount the
  ZFS file systems that are needed early in boot.  Services that can start later
  (e.g. Triton agents) should include `After=zfs.target` so they start after the
  systemd mounts the remaining datasets.

## Future directions

The following describe ideas, not decisions.

### Dataset delegation

It is expected that work will be required to make `delegated_dataset` work.  If
this is ever to host headnode services, build images, etc., that will likely be
important.

This is likely to be a significant amount of work, potentially adding a new
namespace for ZFS.

### ZFS device node security

We need to be sure that container users are not able to muck with ZFS or see
datasets not mounted in the container or delegated to it.  It may be that we
just need to be sure that `/dev/zfs` does not appear in containers and that
containers are not able to create new device nodes.
