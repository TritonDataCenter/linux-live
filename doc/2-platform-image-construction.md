<!--
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
-->

<!--
    Copyright 2020 Joyent, Inc
-->

# Platform Image Construction

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
3. Install Git
```
apt install -y git
```
4. Build the [Image](#image-creation) - clone this repo and run the debian live
   image builder:
```
    git clone -b linuxcn https://github.com/joyent/linux-live
    cd linux-live
    ./tools/debian-live
```
5. Copy the resulting image (iso or usb) out of the debian machine and use that
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

## Image creation

In general, the image creation process is:

```
$ git clone -b linuxcn https://github.com/joyent/linux-live
$ cd linux-live
$ sudo tools/debian-live
```

If all goes well, the final few lines will tell you what the name of the
generated `.iso` and `.usb.gz` files are.

Before you can do that, you probably need to configure your build machine, as
described below.


## Build machine setup

The build machine should be running Debian 10 with zfs.

The build process is self-hosting.  The first image needs to be created
elsewhere - a Debian 10 (64 bit) box with the right packages well do.  Once your
organization has the first image, it is probably easiest to go with the self
hosting route.

### `/bin/sh`

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

### Install zfs

See https://wiki.debian.org/ZFS#Installation or some other "Getting Started"
link found at https://zfsonlinux.org/.  For licensing reasons, the installation
will need to build zfs, which will take several minutes.

### Create a zpool

For now, the image creation script assumes that there is a pool named `data`
where it can create images.

```
sudo zpool create data /dev/vdb
```

If you will be developing and testing agents on this box, you probably want to
tell them that your pool is the system pool.

```
sudo touch /data/.system_pool
```

### Install other dependencies

Run the preflight check to see what other things you need.  If it tells you to
install other packages, do the needful.

```
$ git clone -b linuxcn https://github.com/joyent/linux-live
$ cd linux-live
$ ./tools/debian-live preflight_check
$ echo $?
0
```

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
data/debian-live-20200105T222739Z

$ sudo tools/debian-live -r 20200105T222739Z install_proto
```

To inspect the outcome of that, you can `chroot
/data/debian-live/20200105T222739Z/chroot` and poke around.  More commonly you
will want to run `install_proto` and all steps after it.  You can use `...` for
that.

```
$ sudo tools/debian-live -r 20200105T222739Z install_proto ...
```
