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

### Consistent hostid

By default, each Linux system will get a unique hostid by generating a random
one at first boot.  Depending on timing, the hostid may be generated based on
the system's IP address.  In a DHCP world, the timing and consistent IP address
are not guaranteed.  With live media, every boot is a first boot, so a unique
hostid may be generated on every boot.  That's fine, except when we are relying
on the hostid for something important.

Normally when we want to keep state, we store the data in the zpool.  That
doesn't work in this case, because the hostid is part of the mechanism used by
ZFS to ensure that the pool is only imported by a single host.

To generate a consistent hostid,
[triton-hostid.service](../proto/usr/lib/systemd/system/triton-hostid.service)
calls [set-hostid](../proto/usr/triton/bin/set-hostid), which generates the
hostid using a crc32 hash of the system's UUID.  Since Triton relies on
uniquness and permanence of the system UUID, there's a very good chance that the
crc32 hash of the UUID will be sufficiently for ZFS' needs.

One notable feature part of this service is the use of `DefaultDependencies=no`.
This was required to avoid a dependency cycle caused by the default
dependencies.


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

To cause `systemd.networkd` to start zfs directoriese containing configurate are
mounted, we start networking after `zfs.target` is reached.  This is
accomplished by using a drop-in file to add `After=zfs.target`.
`/usr/lib/systemd/system/systemd-networkd.service.d/triton.conf` contains:

```
[Unit]
After=zfs.target
```

The following are not part of the platform image, but are included here to give
a complete picture of why the above items were configured the way there were.

We need to be sure that `systemd.networkd.service` is able to use
configuration that is stored in the system zpool.  As described in XXX, a file
system is mounted at `/etc/systemd/network` with:

```
# zfs create -o canmount=off -o mountpoint=/ triton/system
# zfs create -o canmount=off triton/system/etc
# zfs create -o canmount=off triton/system/etc/systemd
# zfs create triton/system/etc/systemd/network
```

To prevent `99-dhcp-all-nics.network` from having effect, we can now mask it
with a symbolic link in `/etc/systemd/network`:

```
# ln -s /dev/null /etc/systemd/network/99-dhcp-all-nics.network
```

Finally, to add static configuration of a single nic, we add
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

In theory, renaming a network link should be as simple as dropping a `.link`
file in `/etc/systemd/network` and nudging systemd in some way.  It's a little
more complicated.

Per `systemd.link(5)`:

> Network link configuration is performed by the net_setup_link udev builtin.

As it turns out, `systemd-udevd` is one of those services that really does need
to start early.  I think (but do not know) that it is not possible to reach
`zfs.target` prior to starting `systemd-udevd`, so we need to nudge
`systemd-udevd` after the `/etc/systemd/network/*.link` files become available.

First, an example `.link` file to rename a link to a name that makes it suitable
for including in other unit files and drop-in files.  As a reminder, this
directory is mounted from the system zpool.

```
# cat /etc/systemd/network/00-admin0.link
[Match]
MACAddress=52:54:00:ab:bf:60

[Link]
Name=admin0
```

To get this to be recognized, we have
[triton-post-import.service](../proto/usr/lib/systemd/system/triton-post-import.service)
which runs the [post-zpool-import](../proto/usr/triton/bin/post-zpool-import)
script.

In order to make that link useful, we need to force the link up.  I don't yet
see a way to do that in systemd without assigning an address.  Temporarily, I
have assigned a secondary loopback address.

```
# cat /etc/systemd/network/admin0.network
[Match]
Name=admin0

[Network]
Address=127.0.0.2
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
MACVLAN=admin0
EOF
# systemctl start triton-instance@deb9
```

More details will come in a future document.
