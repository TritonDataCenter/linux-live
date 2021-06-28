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

    ```bash
    zpool create data /dev/sdb
    sudo touch /data/.system_pool
    ```

4. Install Git

    ```bash
    apt install -y git
    ```

5. Build the [Image](#image-creation) - clone this repo and run the debian-live
   image builder:

    ```bash
    git clone https://github.com/joyent/linux-live
    cd linux-live
    ./tools/debian-live
    ```

6. Copy the resulting image (ISO or USB) out of the Debian machine and use that
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
  * Generate the same hostid every time to keep ZFS happy.
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

Several conventions from SmartOS have been carried forward to Linux. This
includes, but is not limited to:

* noimport=true boot parameter will skip importing pools
* destroy_zpools=true will destroy the system pool at boot
* marking zones/var for factory reset will destroy the system pool at boot
* sdc-factoryreset will mark zones/var for reset
* `/opt/custom`. This includes `/opt/custom/systemd` for custom unit files

## Image Distribution

Initially images will be available via Manta. At a later time, these will be
distributed via updates.joyent.com.

### Platform Image Import

The layout of a Linux platform.tgz file will be:

```ls
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

### iPXE configuration

When a Linux CN is configured in booter, the following files will be configured
under `/tftboot`:

* `menu.lst.01<MAC>`: grub configuration
* `boot.ipxe.01<MAC>`: ipxe configuration
* `bootfs/<MAC>/networking.json`: same as SmartOS

`boot.ipxe01<MAC>` will resemble:

```ipxe
#!ipxe
kernel /os/20200401T0123456Z/platform/x86_64/vmlinuz console=ttyS0 boot=live fetch=http:///os/20200401T0123456Z/platform/x86_64/filesystem.squashfs
initrd /os/20200401T0123456Z/platform/x86_64/initrd
module http://<asset_server>/<path>/node.config /etc/node.config
module http://<booter>/bootfs/<MAC>/networking.json /etc/triton-networking.json
module http://<booter>/bootfs/<MAC>/networking.json.hash /etc/triton-networking.json.hash
boot
```

This configuration requires that booter is configured to serve files via HTTP,
not TFTP.

### ISC DHCP server configuration

As a stand-in, ISC DHCP server can be used on a network that doesn't already
have another DHCP server.  The following configuration will serve up dynamic
addresses on the 192.168.42.0/24 network, assuming that the server it is running
on has an interface with address 192.168.42.1 and is running a web server, such
as is described in the nginx configuration that follows this section.

```bash
apt install -y isc-dhcp-server
```

In `/etc/dhcp/dhcpd.conf`:

```conf
default-lease-time 600;
max-lease-time 7200;
ddns-update-style none;
authoritative;
# https://ipxe.org/howto/dhcpd
option space ipxe;
option ipxe-encap-opts code 175 = encapsulate ipxe;
option ipxe.priority code 1 = signed integer 8;
option ipxe.keep-san code 8 = unsigned integer 8;
option ipxe.skip-san-boot code 9 = unsigned integer 8;
option ipxe.syslogs code 85 = string;
option ipxe.cert code 91 = string;
option ipxe.privkey code 92 = string;
option ipxe.crosscert code 93 = string;
option ipxe.no-pxedhcp code 176 = unsigned integer 8;
option ipxe.bus-id code 177 = string;
option ipxe.san-filename code 188 = string;
option ipxe.bios-drive code 189 = unsigned integer 8;
option ipxe.username code 190 = string;
option ipxe.password code 191 = string;
option ipxe.reverse-username code 192 = string;
option ipxe.reverse-password code 193 = string;
option ipxe.version code 235 = string;
option iscsi-initiator-iqn code 203 = string;
# Feature indicators
option ipxe.pxeext code 16 = unsigned integer 8;
option ipxe.iscsi code 17 = unsigned integer 8;
option ipxe.aoe code 18 = unsigned integer 8;
option ipxe.http code 19 = unsigned integer 8;
option ipxe.https code 20 = unsigned integer 8;
option ipxe.tftp code 21 = unsigned integer 8;
option ipxe.ftp code 22 = unsigned integer 8;
option ipxe.dns code 23 = unsigned integer 8;
option ipxe.bzimage code 24 = unsigned integer 8;
option ipxe.multiboot code 25 = unsigned integer 8;
option ipxe.slam code 26 = unsigned integer 8;
option ipxe.srp code 27 = unsigned integer 8;
option ipxe.nbi code 32 = unsigned integer 8;
option ipxe.pxe code 33 = unsigned integer 8;
option ipxe.elf code 34 = unsigned integer 8;
option ipxe.comboot code 35 = unsigned integer 8;
option ipxe.efi code 36 = unsigned integer 8;
option ipxe.fcoe code 37 = unsigned integer 8;
option ipxe.vlan code 38 = unsigned integer 8;
option ipxe.menu code 39 = unsigned integer 8;
option ipxe.sdi code 40 = unsigned integer 8;
option ipxe.nfs code 41 = unsigned integer 8;

subnet 192.168.42.0 netmask 255.255.255.0 {
    option ipxe.no-pxedhcp 1;
    range 192.168.42.100 192.168.42.200;
    filename "http://192.168.42.1/boot.ipxe";
}
```

After the configuration is in place:

```bash
systemctl enable --now isc-dhcp-server
```

### nginx configuration

The following is only needed until booter is updated.  This configuration can be
used on a Debian 10 machine that is on the same network on some other machine
you wish to PXE boot.

Install nginx

```bash
apt install -y nginx
```

Remove `/etc/nginx/sites-enabled/default`.

Add `/etc/nginx/sites-enabled/booter` with the following content:

```bash
server {
    listen 80;
    location / {
        root /tftpboot;
    }
}
```

### Common Boot Procedure

The boot of a Linux CN via iPXE uses the following general procedure:

1. ipxe downloads `/os/<platform-timestamp>/platform/x86_64/vmlinuz` as the
   kernel.
2. ipxe downloads `/os/<platform-timestamp>/platform/x86_64/initrd.img` as the
   initial ramdisk.
3. ipxe downloads `/bootfs/<MAC>/network.json` as `/networking.json`
4. The kernel starts, loads `initrd.img` into a initramfs and mounts at `/`.
   `/networking.json` and `/packages.tar` are visible.
5. The live-boot scripts download `filesystem.squashfs` and creates the required
   overlay mounts to make it writable.
6. `/networking.json` is moved to a location that will be accessible in the new
   root.  This is at a path that a systemd generator will find it to generate
   the appropriate networking configuration under `/run/systemd`.
7. live-boot pivots root and transfers control to systemd.

### Boot of a new CN

A new CN will boot the default image, which may be a SmartOS or a Linux image.
The server may need to be explicitly assigned a linux platform image via
`sdcadm platform assign` and rebooted to the desired operating system before
proceeding with `sdc-server setup`.

<!--

---- Comment within a comment
Removing this paragraph for now. it's a great idea, and we'd like to in the
future, but currently triton doesn't even remotely do this. Hopefully someday,
dear comment snoopers!

This could conceivably be done by adding a PI parameter to server setup jobs and
a "wait for server to be the correct system_type" step in the workflow. If it's
already the right system_type it'll just continue. If not, it would assign the
platform, reboot and wait.
----

If the wrong OS is booted, booter will be updated to cause the next boot to boot
from the appropriate OS and trigger a reboot.  On this subsequent boot, the CN
will be set up.

-->

## Platform Image creation

As stated earlier in this document, in general the image creation process is
roughly:

```bash
git clone https://github.com/joyent/linux-live
cd linux-live
sudo tools/debian-live
```

If all goes well, the final few lines will tell you what the name of the
generated `.iso` and `.usb.gz` files are.

### Build machine installation

The debian-live image can be created on an existing debian-live system.  The
`tools/debian-live` script can install the necessary dependencies.

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

Over time, each problematic `Makefile` should be changed to include:

```bash
SHELL = /bin/bash
```

Note that `/bin/bash` works across various Linux distros and SmartOS.
`/usr/bin/bash` does not exist on at least some Linux distros.

##### Install ZFS

See <https://wiki.debian.org/ZFS#Installation> or some other "Getting Started"
link found at <https://zfsonlinux.org/>.  For licensing reasons, the
installation will need to build ZFS, which will take several minutes.

### First boot build machine setup

Once you have a Debian instance with ZFS, perform the following steps.

#### Create a zpool

For now, the image creation script assumes that there is a pool named `triton`
where it can create images.

```bash
sudo zpool create data /dev/vdb
```

If you will be developing and testing agents on this box, you probably want to
tell them that your pool is the system pool.

```bash
sudo touch /data/.system_pool
```

#### Install other dependencies

Note: This step assumes you you have a non-root user with a home directory on
a persistent file system (ZFS, ext4, etc.) so that it survives reboots.  If
running on debian-live, see [4-zfs.md](4-zfs.md) and look for `sysusers`.

Run the preflight check to see what other things you need.  If it tells you to
install other packages, do the needful.

```bash
$ git clone https://github.com/joyent/linux-live
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
  are delivered into the image, then the appropriate systemd symbolic links are
  created.
* `tools`: home to the `debian-live` script and perhaps other tools.

### Other components

Other pre-built binary components may be found in Manta at
`/Joyent_Dev/public/linuxcn`.  Currently, the only component pulled from that
directory is an [sdcnode](https://github.com/joyent/sdcnode) build.

### Iterative development

As stated above, the typical way that an image is built is with:

```bash
sudo tools/debian-live
```

That's great when things will be smooth sailing from start to finish.  However,
some of the early steps take several minutes to complete.  When tweaking files
in the `proto` directory, it is quite annoying to have to wait for a ZFS build
to fix a typo in a file.  For this reason, the build can be rolled back to the
beginning of any stage so that stage (and presumably others) can be executed.

To list the available stages, use `tools/debian-live -h`:

```bash
$ ./tools/debian-live -h
./tools/debian-live: illegal option -- h
Usage:
Create a new image from scratch
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

```bash
$ zfs list -Ho name | grep debian-live | tail -1
triton/debian-live-20200105T222739Z

$ sudo tools/debian-live -r 20200105T222739Z install_proto
```

To inspect the outcome of that, you can `chroot
/data/debian-live/<platform_stamp>/chroot` and poke around.  More commonly you
will want to run `install_proto` and all steps after it.  You can use `...` for
that.

```bash
sudo tools/debian-live -r 20200105T222739Z install_proto ...
```
