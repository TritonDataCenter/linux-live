<!--
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
-->

<!--
    Copyright 2020 Joyent, Inc
-->

# Introduction

This repository contains the bits needed for construction of a Linux platform
image (PI) for Triton Compute.  Triton has historically supported SmartOS for
compute nodes (CNs) and head nodes (HNs).  The construction of SmartOS-based
platform images is handled by
[smartos-live](https://github.com/joyent/smartos-live).

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

| SmartOS                  | Linux                 | Notes                                    |
|--------------------------|-----------------------|------------------------------------------|
| SMF                      | systemd               | systemd has taken over more of the world than SMF has in SmartOS. |
| ZFS                      | ZFS                   | Due to licensing constraints, ZFS is distributed only as source code |
| `vmadm action`           | `vmadm action`        | Some actions on Linux will be unsupported |
| `imgadm action`          | `imgadm action`       | Some actions on Linux will be unsupported |
| zoneadmd                 | lxc                   | Linux container service |
| `zoneadm -z $i boot`     | `lxc start triton-$i` | Instance actions are performed through lxc/lxd |
| `zoneadm -z $i shutdown` | `lxc stop triton-$i`  | Same |
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

As much as possible, Linux compute nodes will run the same agents that are run
on SmartOS.  The agents are delivered from the same repositories with the same
versioning scheme.  Tools like `vmadm` and `imgadm` will be available and work
in the same manner as SmartOS.

## Installation

Follow the (Linux CN Installation)[5-triton-updates.md] steps to create a Linux
compute node.

## Design docs

* (Platform Images)[2-platform-image.md]
* (Systemd Integration)[3-systemd-integration.md]
* (ZFS)[4-zfs.md]
