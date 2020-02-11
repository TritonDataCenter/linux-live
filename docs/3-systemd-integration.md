<!--
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
-->

<!--
    Copyright 2020 Joyent, Inc
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
man pages are at
[freedesktop.org](https://www.freedesktop.org/software/systemd/man/index.html).
Notice that the commands (sections 1 and 8) tend to be `systemd` **dash**
`something` and files (section 5) tend to be described in `systemd` **dot**
`something`.

* [systemd(1)](https://www.freedesktop.org/software/systemd/man/systemd.html)
* [systemctl(1)](https://www.freedesktop.org/software/systemd/man/systemctl.html#)
* [systemd-nspawn(1)](https://www.freedesktop.org/software/systemd/man/systemd-nspawn.html)
* [systmed.service(5)](https://www.freedesktop.org/software/systemd/man/systemd.service.html)
* [systemd.unit(5)](https://www.freedesktop.org/software/systemd/man/systemd.unit.html)
* [systemd.nspawn(5)](https://www.freedesktop.org/software/systemd/man/systemd.nspawn.html)
* [systemd.syntax(5)](https://www.freedesktop.org/software/systemd/man/systemd.syntax.html)
* [systemd.target(5)](https://www.freedesktop.org/software/systemd/man/systemd.target.html)
* [systemd.slice(5)](https://www.freedesktop.org/software/systemd/man/systemd.slice.html)

The above documentation and the various links you will be inspired to follow
within that documentation should be matched with at least half a pot of coffee.
There's a lot there, and it is only a fracton of the available documentation.
systemd is easier to love (use, tolerate?) with the proper investment in
learning about how it works.

So far, there has been no need to tweak any of systemd's configuration.  That
being said, it is comforting to know that it is possible.  See
[systemd-system.conf](https://www.freedesktop.org/software/systemd/man/systemd-system.conf.html)
and [systemd](https://www.freedesktop.org/software/systemd/man/systemd.html).

## Platform image services

The following subsections describe the services that are part of the platform
that have been customized.

All systemd unit and drop-in files that are part of the platform image are
delivered under `/usr/lib/systemd`.  This ensures that most directories under
`/etc/systemd` are empty, allowing zfs file systems to be mounted on them.


### Enable DHCP on all NICs

During development, it is handy to have NICs configured by dhcp.  Except when
it's not.  `/usr/lib/systemd/network/99-dhcp-all-nics.network` contains:

```
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

It is anticipated that enabling dhcp on all NICs is a poor choice in many cases
and we need to have a way to override it without selecting a non-default boot
entry each time.

As described in [*Dataset hierarchy*](4-zfs.md#datasets-hierarchy),
`/etc/systemd/network` is persistent across boots.  This can be leveraged to
create non-default persistent network configuration.  Details follow.

The following are not part of the platform image, but are included here to give
a complete picture of why the above items were configured the way there were.

To prevent `99-dhcp-all-nics.network` from having effect, we can now mask it
with a symbolic link in `/etc/systemd/network`:

```
# ln -s /dev/null /etc/systemd/network/99-dhcp-all-nics.network
```

To add static configuration of a single nic, we add
`/etc/systemd/network/10-admin.network`:

```
[Match]
MACAddress=52:54:00:d6:56:a1

[Network]
Address=192.168.1.183/24
Gateway=192.168.1.1
DNS=192.168.1.1
```

### Rename network links

Renaming a network link is as simple as dropping a `.link` file in
`/etc/systemd/network` and nudging systemd or rebooting.

Per `systemd.link(5)`:

> Network link configuration is performed by the net_setup_link udev builtin.

As it turns out, `systemd-udevd` is one of those services that really does need
to start early.

This example `.link` file can be used to rename a link to a name that makes it
suitable for including in other unit files and drop-in files.  As a reminder,
this directory is mounted from the system zpool.

```
# cat /etc/systemd/network/00-admin0.link
[Match]
MACAddress=52:54:00:ab:bf:60

[Link]
Name=admin0
```

To get this to be recognized, we can reboot or nudge systemd.  Nudging systemd
is a subset of:

- Run `systemctl daemon-reload` to let systemd that a new drop-in file is
  present.  (XXX not sure this is needed)
- Run `systemctl restart systemd-udevd` to let udevd know that it has some new
  configuration.
- Run `udevadm -s net -c add`.  While it is tempting to add the `-w` option to
  wait for it to complete, there's a bug in systemd (XXX-linuxcn file it) in
  that the wait never completes because it seems to need to wait on the renamed
  path, not the original path.  Instead, if you need to ensure that the async
  `udevd` work is done, run `udevadm settle`.

The the link renamed, it can be configured.  If the link doesn't need an address
in the host, the link will remain down.  This causes problems for macvlan
instances in containers, as they cannot use their macvlan instances while the
lower link is down.

```
# cat /etc/systemd/network/admin0.network
[Match]
Name=admin0

[Network]
# XXX-linuxcn: hack to bring lower-link up.
Address=127.0.0.2
```

If we want to attach metadata to a link (or any other unit file), that can be
done with `X-` prefixes.

```
[X-Triton]
X-NICTag=admin
X-Customer=00000000-0000-0000-0000-000000000000
```

### Machine template

The following can be used to create a template service that allows ephemeral
instances to be created.  This file should be stored as
`/usr/lib/systemd/system/triton-instance@.service`.

```
[Unit]
Description=nspawn-1

[Service]
ExecStart=/usr/bin/systemd-nspawn -b --machine=%i \
	--directory=/var/lib/machines/%i/root

[Install]
Also=dbus.service
```

Now you can:

```
# imgadm import 63d6e664-3f1f-11e8-aef6-a3120cf8dd9d
# zfs clone -o mountpoint=/var/lib/machines/deb9 \
      triton/63d6e664-3f1f-11e8-aef6-a3120cf8dd9d@final triton/deb9
# cat > /var/lib/machines/deb9.nspawn <<EOF
[Exec]
Boot=on
PrivateUsers=auto

[Network]
Private=yes
# A symbolic lower-link name makes this much clearer than enp7s0 would be
MACVLAN=admin0
EOF
# systemctl start triton-instance@deb9
```

More details will come in a future document.