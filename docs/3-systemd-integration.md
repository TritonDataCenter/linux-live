<!--
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
-->

<!--
    Copyright 2020 Joyent, Inc.
    Copyright 2022 MNX Cloud, Inc.
-->

# Systemd Integration

At first blush, it seems that systemd provides a common set of abstractions
across many Linux distributions.  More importantly, the various features of
systemd are developed with a more complete knowledge of how they can
interoperate with other features.  This seems to mean that if there is a native
systemd way to perform a task, it is quite likely that it is going to be the
most pain-free way to accomplish it in the long run.  Please try to take this
leap of faith with me.

## systemd documentation

The documentation set for systemd is surprisingly good.  Online versions of the
man pages are at [freedesktop.org][fd-sysd-doc]).

Notice that the commands (sections 1 and 8) tend to be `systemd-something` (a
**dash**) and files (section 5) tend to be described in `systemd.something`
(a **dot**).

[fd-sysd-doc]: https://www.freedesktop.org/software/systemd/man/index.html

* [systemd(1)](https://www.freedesktop.org/software/systemd/man/systemd.html)
* [systemctl(1)](https://www.freedesktop.org/software/systemd/man/systemctl.html#)
* [systemd-nspawn(1)](https://www.freedesktop.org/software/systemd/man/systemd-nspawn.html)
* [systemd.service(5)](https://www.freedesktop.org/software/systemd/man/systemd.service.html)
* [systemd.unit(5)](https://www.freedesktop.org/software/systemd/man/systemd.unit.html)
* [systemd.nspawn(5)](https://www.freedesktop.org/software/systemd/man/systemd.nspawn.html)
* [systemd.syntax(5)](https://www.freedesktop.org/software/systemd/man/systemd.syntax.html)
* [systemd.target(5)](https://www.freedesktop.org/software/systemd/man/systemd.target.html)
* [systemd.slice(5)](https://www.freedesktop.org/software/systemd/man/systemd.slice.html)

The above documentation and the various links you will be inspired to follow
within that documentation should be matched with at least half a pot of coffee.
There's a lot there, and it is only a fraction of the available documentation.
systemd is easier to love (use, tolerate?) with the proper investment in
learning about how it works.

So far, there has been no need to tweak any of systemd's configuration.  That
being said, it is comforting to know that it is possible.  See
[systemd-system.conf](https://www.freedesktop.org/software/systemd/man/systemd-system.conf.html)
and [systemd](https://www.freedesktop.org/software/systemd/man/systemd.html).

## How linux-live will configure systemd

As described in
[systemd.unit](https://www.freedesktop.org/software/systemd/man/systemd.unit.html),
a service's configuration is synthesized from:

* Template service files: `<service>@.service` files in the service directories
* Service files: `<service>[@<instance>].service` files in the service directories
* Drop in files: `<service>[@[<instance>]].service.d/*.conf` files in the
  service directories.

Where the service directories are:

* `/lib/systemd/system`: Part of the platform image. Typically, these files are
  delivered by the distribution's packages.
* `/run/systemd/system`: Dynamically generated at run-time and not persisted
  across reboots.
* `/etc/systemd/system`: Under the administrator's control.  This will be
  mounted from the system ZFS pool very early in boot, allowing persistence.

### Best practices

1. Do not directly replace any file delivered by the distribution.  That is, do
   not add a file under [`proto`](../proto) that will replace
   `/usr/lib/systemd/system/*.service`.
2. If a service needs to be customized for the platform image, use a drop-in
   file per need. Include comments explaining why it is needed so that future
   developers may understand when it is no longer appropriate.  Consider filing
   a JIRA ticket or github issue calling for the removal of the customization if
   it is intended to work around a bug in the distribution's software.
3. If a service is dependent on runtime information that is obtained from some
   Triton service, consider using a generator that defines the service under
   `/run/systemd/system`.
4. Service definitions under `/etc/systemd/system` should be rare.  Services
   configured in `/etc/systemd/system` are almost certainly configured by a
   Triton agent.  This implies that as services and platform images evolve, the
   future versions of Triton agents need to know how to migrate any
   configuration between arbitrary configuration versions.
5. End users can add systemd unit files to `/opt/custom/systmed` which will be
   automatically enabled and started by systemd.

### Example: Fixing a broken service

As described in [linux-live#10](https://github.com/TritonDataCenter/linux-live/issues/10),
the `systemd-nspawn@.service` template service requires a workaround to allow
container reboots to work.

Rather than replacing `/lib/systemd/systemd-nspawn@.service` as described in
some workarounds, a drop-in configuration is added as
`/usr/lib/systemd/system/systemd-nspawn@.service.d/10-reboot-workaround.conf`.
It contains:

```systemd
#
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

#
# Copyright 2020 Joyent, Inc.
#

#
# Remove --keep-unit from the command line so that the container can restart.
# https://github.com/systemd/systemd/issues/2809
#

[Service]
# First clear the existing ExecStart, else we end up with multiple commands.
ExecStart=
# Now define the new one
ExecStart=/usr/bin/systemd-nspawn --quiet --boot --link-journal=try-guest \
    --network-veth -U --settings=override --machine=%i
```

## Platform image services

The following subsections describe the services that are part of the platform
that have been customized.

All systemd unit and drop-in files that are part of the platform image are
delivered under `/usr/lib/systemd`.  This ensures that most directories under
`/etc/systemd` are empty, allowing ZFS file systems to be mounted on them.

### Networking

The examples below describe static configuration in `/etc/systemd/network`.
This is a place-holder until dynamic configuration from `networking.json` is
enabled.  As described in
[linux-live#12](https://github.com/TritonDataCenter/linux-live/issues/12), the
networking configuration provided by booter in `networking.json` should be
transformed into configuration in `/run/systemd` using a
[generator](https://www.freedesktop.org/software/systemd/man/systemd.generator.html).

#### Enable DHCP on all NICs

During development, it is handy to have NICs configured by DHCP.  Except when
it's not.  `/usr/lib/systemd/network/99-dhcp-all-nics.network` contains:

```systemd
[Match]
KernelCommandLine=triton.dhcp-all-nics

[Network]
DHCP=ipv4
```

The `Match` section means that all NICs are matched, but only when the kernel is
booted with the `triton.dhcp-all-nics` command line argument.  This is set in
the default grub entry by [debian-live](../tools/debian-live).  An alternate
grub entry exists to disable that feature, leaving it to the admin to configure
NICs however they want.

It is anticipated that enabling DHCP on all NICs is a poor choice in many cases
and we need to have a way to override it without selecting a non-default boot
entry each time.

As described in [*Dataset hierarchy*](4-zfs.md#datasets-hierarchy),
`/etc/systemd/network` is persistent across boots.  This can be leveraged to
create non-default persistent network configuration.  Details follow.

The following are not part of the platform image, but are included here to give
a complete picture of why the above items were configured the way there were.

To prevent `99-dhcp-all-nics.network` from having effect, we can now mask it
with a symbolic link in `/etc/systemd/network`:

```bash
ln -s /dev/null /etc/systemd/network/99-dhcp-all-nics.network
```

To add static configuration of a single NIC, we add
`/etc/systemd/network/10-external0.network`:

```systemd
[Match]
MACAddress=52:54:00:d6:56:a1

[Network]
Address=192.168.1.183/24
Gateway=192.168.1.1
DNS=192.168.1.1
```

#### Rename network links

Renaming a network link should be as simple as dropping a `.link` file in
`/etc/systemd/network`.  On the next reboot the link should be renamed
automatically.  If adding a link on the fly, it is necessary to nudge systemd.
As a reminder `/etc/systemd/network` is mounted from the system ZFS pool.

A `.link` file like the following can be used to rename a link to a name that
makes it suitable for including in other unit files and drop-in files.

```bash
# cat /etc/systemd/network/10-external0.link
[Match]
MACAddress=52:54:00:ab:bf:60

[Link]
Name=external0
```

XXX: At one point, I could not get these drop-in files to be recognized unless I
added `net.ifnames=0` to the kernel command line.  As I started to document that
requirement here, I found I no longer needed to do that.  I don't know if I had
a subtle bug in a `.link` file or if there is some race condition that I was
consistently hitting and am now consistently not hitting.

If a reboot is not acceptable, the link rename can be forced without a reboot
with:

```bash
udevadm trigger -s net -c add
udevadm settle
```

While it is tempting to add the `-w` option to wait for it to complete, there's
bug in systemd (XXX reproduce then file it) in that the wait never completes
because it seems to need to wait on the renamed path, not the original path.
To ensure that the rename has completed, we run `udevadm settle`.

With the link renamed, it can be configured.  If the link doesn't need an
address in the host, the link will remain down.  This causes problems for
MACVLAN instances in containers, as they cannot use their MACVLAN instances
while the lower link is down.

```bash
# cat /etc/systemd/network/external0.network
[Match]
Name=external0

[Network]
# XXX-linuxcn: hack to bring lower-link up.
Address=127.0.0.2
```

If we want to attach metadata to a link (or any other unit file), that can be
done with `X-` prefixes.

```systemd
[X-Triton]
X-NICTag=external
X-Customer=00000000-0000-0000-0000-000000000000
```
