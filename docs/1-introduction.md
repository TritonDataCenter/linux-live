<!--
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
-->

<!--
    Copyright 2021 Joyent, Inc
-->

# Introduction

This repository contains the tools needed for construction of a Linux platform
image (PI) for Triton Compute.  Triton has historically supported SmartOS for
compute nodes (CNs) and head nodes (HNs).  The construction of SmartOS-based
platform images is handled by [smartos-live][smartos-live].

[smartos-live]: https://github.com/joyent/smartos-live

## Stateless platform images with local storage

Platform images are intended to be stateless: rebooting a physical machine loads
a fresh image via PXE boot or a live image boot from a DVD or USB drive.

Of course, there is some state that is needed on each compute node.  For
instance, installed container instances should survive the CN reboot.  There is
also a small amount of configuration that is needed across a variety of
different directories.  All of this state is maintained in a ZFS pool.

## Key technology mapping

Over the years, Linux has grown many native features that resemble those that
are used in SmartOS.  A quick mapping is as follows:

| SmartOS                  | Linux                 | Notes                                                                |
|--------------------------|-----------------------|----------------------------------------------------------------------|
| SMF                      | systemd               | systemd is the de-facto standard system service runtime on Linux     |
| ZFS                      | ZFS                   | We will follow Ubuntu's lead in shipping ZFS                         |
| `vmadm action`           | `vmadm action`        | Some actions on Linux will be unsupported                            |
| `imgadm action`          | `imgadm action`       | Some actions on Linux will be unsupported                            |
| zoneadmd                 | lxc/lxc               | Linux container service                                              |
| `zoneadm -z $i boot`     | `lxc start triton-$i` | Instance actions are performed through lxc/lxd                       |
| `zoneadm -z $i shutdown` | `lxc stop triton-$i`  | Same                                                                 |
| `zlogin $i`              | `lxc console triton-$i` or `lxc exec triton-$i` | |

Of course, these will all be wrapped in Triton APIs and familiar CLIs that call
those APIs so it will all be the same from the Triton perspective.

Notable in all of this is that as systemd has become capable of taking over more
and more parts of the OS, it becomes a useful abstraction layer across
distributions.  Joyent's reference implementation may be based on a freely
redistributable distribution while allowing Triton CN software to be easily
supported on a wide variety of distributions that use systemd of the same or
newer version.

## Triton components

In the initial phase, the Linux Platform Image will support Compute Nodes
running Linux containers only.  Specifically excluded are:

* Head Nodes
* HVM instances, including transparent container wrapping ala Kata Containers or
  Firecracker.
* Docker or OCI images. Eventually we'd like sdc-docker to deploy OCI images to
  Linux CNs.

As much as possible, Linux compute nodes will run the same agents that are run
on SmartOS.  The agents are delivered from the same repositories with the same
versioning scheme.  Tools like `vmadm` and `imgadm` will be available and work
in the same manner as SmartOS.

## Installation

Follow the [Linux CN Installation](5-triton-updates.md) steps to create a Linux
compute node.

## Design docs

* [Platform Images](2-platform-image.md)
* [Systemd Integration](3-systemd-integration.md)
* [ZFS](4-zfs.md)

## Bugs

Linux CN is currently in "technology preview" state. There *will* be things
that don't work right.

Before filing bugs, make sure to update all components to the latest dev
channel, replicate the issue on the latest Linux PI, and check the
[github issues][gh-issues] to see if it has already been filed.

During the development cycle in these early phases it's common for multiple
platform images to be released per day.

If you run into an issue that is unreported, please do file it! You can also
drop by IRC in `#triton` on `irc.libera.chat` to discuss it with us.

[gh-issues]: https://github.com/joyent/linux-live/issues
