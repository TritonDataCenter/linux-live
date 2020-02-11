<!--
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
-->

<!--
    Copyright 2020 Joyent, Inc
-->

# Platform Image

**NOTICE:** This is a work in progress.  This document is terse to convey intent
that exists at the time it was written.  Things may change.

The platform image is based on Debian utilizing ZFS on Linux.

## Build Machine Requirements

### TLDR

1. Install [Debian 10 64-bit](https://www.debian.org/CD/http-ftp/) on a machine
   (e.g. in VMware)
2. Install [ZFS](#install-zfs)
3. Create a zpool (warning - this will destroy the disk partition), e.g.
```
zpool create data /dev/sdb
sudo touch /data/.system_pool
```
4. Install Git
```
apt install -y git
```
5. Build the [Image](#image-creation) - clone this repo and run the debian live
   image builder:
```
    git clone -b linuxcn https://github.com/joyent/linux-live
    cd linux-live
    ./tools/debian-live
```
6. Copy the resulting image (iso or usb) out of the debian machine and use that
   for the compute node boot (e.g. in a different VMware virtual machine, or on
   real hardware).

## Image Contents

The root file system of the image will be a squashfs image that is mounted with
a tmpfs overlay, allowing the root file system to be read-write.  Since the
overlay is backed by tmpfs, it is not persisted across reboots.

There are a small number of Triton-specific files that need to be delivered into
the image.  In general they fall into the following categories:

* Things needed to bootstrap Triton.  For instance, `ur-agent` and its
  dependencies (e.g. `/usr/node`) need to be present.
* Things needed for sane operation.  Currently this includes:
  * Setting the locale to quiet various warnings.
  * Fixing a very poor default setting for `mouse` in `vim` so that copy and
    paste will work.
  * Adding Triton paths to `$PATH`
  * Use of DHCP when booted as not part of Triton
  * SSH host key generation and preservation
  * Altering service dependencies so that service configuration stored in ZFS is
    respected.
  * Generate the same hostid every time to keep zfs happy.
  * Joyent utilities that SmartOS admins expect to have.  This includes manta
    client utilities (`mget` and friends) and `json`.

As much as possible, Triton components that are part of the image are installed
under `/usr/triton`.  When needed, symbolic links should be installed
for compatibility.  For instance, much of Joyent's software may assume that
`/usr/node/bin/node` is the platform's node installation, so `/usr/node` is a
symbolic link to the appropriate directory under `/usr/triton`.

Files used for configuring systemd (service files, drop-ins, etc.) are installed
under `/usr/lib/systemd`.

Special effort is made to keep some directories empty so that ZFS file systems
can be mounted over them.  For instance, persistent network configuration is
stored as drop-in files in `/etc/systemd/network`, which is mounted from
`<system pool>/platform/etc/systemd/network`.

## Image Distribution

XXX This is aspirational as of 2020-02-10.

Linux ZFS binaries are not redistributable due to incompatibility between the
GPL and CDDL.  Distribution of ZFS will be only via source code.  Each
organization that uses ZFS will need to create their own ZFS binaries for each
image.  This process will be automated.

Each distributed platform image will have four key files:

1. The kernel, `vmlinuz`.
2. The initial ramdisk, `initrd.img`
3. The platform root file system, `filesystem.squashfs`
4. The platform build file system, `build.tgz`

All of these files will be included in `platform.tgz`, with a hierarchy and
auxiliary files that match those found in a SmartOS platform archive.

### zfsbuilder service.

The zfsbuilder service will run in an HVM instance.  An HVM instance is needed
so that nested containers may be used in the build process.  In the future, we
may allow builds to occur on lxc instances on Linux CNs, as nested containers
are possible.

A new trition service, zfsbuilder, will become aware of a new platform image
import (XXX mechanism TBD) and will use the content of `build.tgz` to build
ZFS binaries.  `build.tgz` is a container root file system that has
all of the source code and dependencies required to build ZFS binaries that are
appropriate for the associated platform image.

The build will produce a file, `zfs-$platform.tar` containing the ZFS
packages that need to be installed in the platform image.  This tar file should
not include test packages or others that are not needed for typical operation.
The test package(s) should be available to aid in development.

The files will be made available via http from the zfsbuilder VM.

## Booter Integration

XXX This is aspirational as of 2020-02-10.

### Platform Image Import

The layout of a Linux platform.tgz file will be:

```
platform/
platform/etc/
platform/etc/version/
platform/etc/version/platform
platform/etc/version/os-release
platform/etc/version/gitstatus
platform/x86_64/vmlinuz
platform/x86_64/initrd
platform/x86_64/initrd.hash
platform/x86_64/filesystem.squashfs
platform/x86_64/filesystem.squashfs.hash
platform/x86_64/filesystem.packages
platform/x86_64/filesystem.manifest
platform/x86_64/build.tgz
platform/x86_64/build.tgz.hash
platform/x86_64/build.tgz.packages
platform/x86_64/build.tgz.manifest
```

The existing `sdcadm platform` command will be enhanced to support Linux
platform images.

When a Linux PI is imported, (`sdcadm platform`? `booter`?)  will notify the
zfsbuilder service of its existence.  The zfsbuilder service will build the ZFS
bits, as described above.

### iPXE configuration

When a Linux CN is configured in booter, the following files will be configured
under `/tftboot`:

* `menu.lst.01<MAC>`: XXX grub configuration, for a reason I don't understand.
* `boot.ipxe.01<MAC>`: ipxe configuration
* `bootfs/<MAC>/networking.json`: same as SmartOS

`boot.ipxe01<MAC>` will resemble:

```
#!ipxe
kernel /os/20200401T0123456Z/platform/x86_64/vmlinuz console=ttyS0 boot=live \
  fetch=http:///os/20200401T0123456Z/platform/x86_64/filesystem.squashfs
initrd /os/20200401T0123456Z/platform/x86_64/initrd
module --name /packages.tar /zfs/20200401T0123456Z/packages.tar
module --name /networking.json /bootfs/<MAC>/networking.json
boot
```

In this example, it is anticipated that http://<booter-ip>/zfs is proxied
to the appropriate URL on the zfsbuild instance.  This configuration requires
that booter is configured to serve files via http, not tftp.

**XXX for initial prototyping, zfs bits will be in `filesystem.squashfs` and
perhaps `initrd`.  `packages.tar` will not be available and if `networking.json`
is present it can be ignored.**

### Common Boot Procedure

The boot of a Linux CN via iPXE uses the following general procedure:

1. ipxe downloads `/os/<platform-timestamp>/platform/x86_64/vmlinuz` as the
   kernel.
2. ipxe downloads `/os/<platform-timestamp>/platform/x86_64/initrd.img` as the
   initial ramdisk.
2. ipxe downloads `/zfs/<platform-timestamp>/packages` as `/packages.tar`.
3. ipxe downloads `/bootfs/<MAC>/network.json` as `/networking.json`
4. The kernel starts, loads `initrd.img` into a initramfs and mounts at `/`.
   `/networking.json` and `/packages.tar` are visible.
5. The live-boot scripts download `filesystem.squashfs` and creates the required
   overlay mounts to make it writable.
6. The zfs packages are installed in the soon-to-be root file system, the zfs
   kernel modules are loaded, and any existing pool is imported and zfs file
   systems critical to early boot are mounted.
7. `/networking.json` is moved to a location that will be accessible in the new
   root.  This is at a path that a systemd generator will find it to generate
   the appropriate networking configuration under `/run/systemd`.
8. live-boot pivots root and transfers control to systemd.

### Boot of a new CN

A new CN will boot the default image, which may be a SmartOS or a Linux image.
`sdc-server setup` may specify which image the server is to run.  If the wrong
OS is booted, booter will be updated to cause the next boot to boot from the
appropriate OS and trigger a reboot.  On this subsequent boot, the CN will be
set up.


## Platform Image creation

In general, the image creation process is:

```
$ git clone -b linuxcn https://github.com/joyent/linux-live
$ cd linux-live
$ sudo tools/debian-live
```

If all goes well, the final few lines will tell you what the name of the
generated `.iso` and `.usb.gz` files are.

### Build machine installation

The debian-live image can be created on an existing debian-live system.  This
image is not redistributable due to the inclusion of zfs.  For this reason, the
steps to creating your a PI build environment varies.


#### Build machine installation in Joyent

The build process is self-hosting.  The first image needs to be created
elsewhere - a Debian 10 (64 bit) box with the right packages well do.  Once your
organization has the first image, it is probably easiest to go with the self
hosting route.

#### Build machine installation outside Joyent

If you are not part of Joyent and your organization has not yet built its own
image, you will need to use a generic Debian 10 system to build the the first
image.  The procedure is as follows.

##### `/bin/sh`

Debian uses `dash` as the Bourne shell.  It is less featureful than `bash`,
which causes problems for some Joyent Makefiles.  To work around this:

XXX-linuxcn: remove this advice once more cleanup happens

```
# dpkg-reconfigure dash
```

Follow the prompts to tell it that `/bin/sh` should be `bash`, not `dash`.

Over time, each problematic `Makefile` should be changed to include:

```
SHELL = /bin/bash
```

Note that `/bin/bash` works across various Linux distros and SmartOS.
`/usr/bin/bash` does not exist on at least some Linux distros.

##### Install zfs

See https://wiki.debian.org/ZFS#Installation or some other "Getting Started"
link found at https://zfsonlinux.org/.  For licensing reasons, the installation
will need to build zfs, which will take several minutes.

### First boot build machine setup

Once you have a Debian instance with zfs, perform the following steps.

#### Create a zpool

For now, the image creation script assumes that there is a pool named `triton`
where it can create images.

```
sudo zpool create triton /dev/vdb
```

If you will be developing and testing agents on this box, you probably want to
tell them that your pool is the system pool.

```
sudo touch /triton/.system_pool
```

#### Install other dependencies

Note: This step assumes you you have a non-root user with a home directory on
a persistent file system (zfs, ext4, etc.) so that it survives reboots.  If
running on debian-live, see [4-zfs.md](4-zfs.md) and look for `sysusers`.

Run the preflight check to see what other things you need.  If it tells you to
install other packages, do the needful.

```
$ git clone -b linuxcn https://github.com/joyent/linux-live
$ cd linux-live
$ ./tools/debian-live preflight_check
$ echo $?
0
```

### Special steps for development on debian-live

If you are developing on debian-live, the additional software that you install
to build the image or other software will not survive reboot.  You will need to
follow the advice given by `debian-live preflight_check` after each reboot.

## Advanced `debian-live` use and development

This section talks a bit more about the structure of this repository and the use
of the `debian-live` script.

### Repository structure

The repository has the following directories that largely conform to what you
would expect from a Joyent repository.

* `doc`: the directory containing the file you are reading now.
* `proto`: extra files that are copied directly into the repository.  In
  addition, each systemd service that is included in
  `proto/usr/lib/systemd/system` is enabled in the image.  That is, the files
  are delivered into the iamge, then the appropriate systemd symbolic links are
  created.
* `tools`: home to the `debian-live` script and perhaps other tools.

### Other components

Other pre-built binary components may be found in Manta at
`/Joyent_Dev/public/linuxcn`.  Currently, the only component pulled from that
directory is an [sdcnode](https://github.com/joyent/sdcnode) build.

### Iterative development

As stated above, the typical way that an image is built is with:

```
$ sudo tools/debian-live
```

That's great when things will be smooth sailing from start to finish.  However,
some of the early steps take several minutes to complete.  When tweaking files
in the `proto` directory, it is quite annoying to have to wait for a zfs build
to fix a typo in a file.  For this reason, the build can be rolled back to the
beginning of any stage so that stage (and presumably others) can be executed.

To list the available stages, use `tools/debian-live -h`:

```
$ ./tools/debian-live -h
./tools/debian-live: illegal option -- h
Usage:
Create a new iamge from scratch
    debian-live

Run just the specified steps on an existing image build
    debian-live -r release step [...]

Valid steps are:
    preflight_check
    create_bootstrap
    install_live
    build_zfs
    reduce_zfs
    install_usr_triton
    install_proto
    postinstall
    prepare_archive_bits
    create_iso
    create_usb
    ...

Literal ... may be used to specify all remaining steps.
```

If I only want to roll back to before `install_proto` and re-run that step:

```
$ zfs list -Ho name | grep debian-live | tail -1
triton/debian-live-20200105T222739Z

$ sudo tools/debian-live -r 20200105T222739Z install_proto
```

To inspect the outcome of that, you can `chroot
/triton/debian-live/20200105T222739Z/chroot` and poke around.  More commonly you
will want to run `install_proto` and all steps after it.  You can use `...` for
that.

```
$ sudo tools/debian-live -r 20200105T222739Z install_proto ...
```