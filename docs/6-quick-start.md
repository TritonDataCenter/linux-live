<!--
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
-->

<!--
    Copyright 2021 Joyent, Inc.
    Copyright 2023 MNX Cloud, Inc.
-->

# Triton Datacenter Linux CN Quick Start Guide

This is written for users who are already familiar with Triton and are ready
to add Linux CNs to an existing datacenter.

**WARNING:** Linux CN is a *technology preview*. Some features are not yet
implemented and the behavior *will* change without explicit notification. A
general rule of thumb is that we want Linux CN to behave mostly like SmartOS.

Linux CNs are currently *not* recomended for multi-tenant hosting providers.
There will most likely be breaking changes with little to no warning.

## Update Triton

1. Update all components to the latest `release` image. See the
   [Triton Maintenance and Upgrades][triton-upgrade] documentation.
2. Update `imgapi` to the latest `dev` image.

        sdcadm update -C dev imgapi

## Obtaining Linux Platform Images

Linux Platform images are currently only available in Manta. At a later time,
they'll start being added to the `experimental` channel. You should keep as
up to date as possible with Linux CN images. Things can move pretty fast.

1. Check Manta for the latest platform image
   (<https://us-central.manta.mnx.io/Joyent_Dev/public/manta-browse/browse.html>,
   drill down to the TritonDCLinux directory).
2. Download the image

        mkdir /var/tmp/linuxcn
        cd /var/tmp/linuxcn
        curl -OC - https://us-central.manta.mnx.io/Joyent_Dev/public/TritonDCLinux/20210731T223008Z/platform-20210731T223008Z.tgz

3. Install the image

        sdcadm platform install ./platform-20210731T223008Z.tgz

## Compute Node Setup

You'll need to have an un-setup CN. If you don't have an available un-setup CN
you can wipe an existing CN. **THIS WILL DESTROY ALL DATA ON THE CN**.

### Factory reset a compute node

**WARNING:** These steps will ***irecoverably destroy all data*** on the CN.
Make sure you are sure!!

1. Migrate any zones that are still on the CN. See the `sdc-migrate` command.
2. SSH to the compute node
3. Issue the `sdc-factoryreset` command
4. After the server reboots, power it off. This can be done with IPMI if you
   have that available.
5. Delete the server from CNAPI. First make note of the CN UUID, then delete it

        sdc-server delete <CN UUID>

6. Delete all NICs from NAPI

        for m in $(sdc-napi /nics?belongs_to_uuid="<CN UUID>" | json -Ha mac); do
            sdc-napi /nics/"$m" -X DELETE
        done

7. Verify with `sdc-server list` that no background tasks have re-added the CN
   to CNAPI. If necessary, issue `sdc-server delete` again.

### New Compute Node Set up

1. Boot the CN. It will boot to the default platform image, which will most
   likely be SmartOS.
2. Assign a Linux platform, set kernel args, and reboot:

        sdcadm platform assign <platform> <CN UUID>

        sdc-cnapi /boot/<CN UUID> -X PUT -d \
            '{"kernel_args": {"systemd.unified_cgroup_hierarchy": "0"}}'

        sdc-oneachnode -n <CN UUID> 'exit 113'

3. The CN will now boot to the designated platform image.
   * To verify the OS run `sdc-oneachnode -n <CN UUID> 'uname -s'`
4. Run `sdc-server setup` as normal. Note that not all options are supported
   yet. Setting `hostname` *is* supported.
5. Assign nic tags via AdminUI or NAPI.
6. Reboot the CN after nic tags are assigned.

The CN is now ready for use. You should be able to ssh to the CN from the
headnode using the `sdc.id_rsa` key.

## Importing Linux CN (LXC) Images

Linux CN currently only supports LXC instances. To import images, first you need
to identify an image *alias*. You can use the following command on any Linux
system with LXC/LXD installed to list images.

    lxc image list images:
    lxc image list ubuntu:

Images in the `ubuntu:` repo are produced by Canonical. Any of these images
should work fine. Canonical image aliases are named after the Ubuntu code name.
E.g., `ubuntu:f` for Ubuntu 20.04 Focal Fossa.

Images in the `images:` repo are produced by <https://linuxcontainers.org>.
The `images:` repo provides images of type `CONTAINER` or `VIRTUAL-MACHINE`.
Virtual machine images **are not yet supported** (we will be adding support
at a later date). Most images tagged with `cloud` should work.
The requirement for an image to work properly is that it has both `cloud-init`
and `ssh-server` installed in the image. Unfortunately, not all images tagged
with `cloud` have `ssh` installed and there's no way to know without trying
it out.

Linuxcontainers.org image aliases are named in the format
`<distribution>/<version>/<variant>/<arch>`. Variant and arch are optional. If
the arch isn't listed in the tag, then it will be the same arch that matches
the host on which the command was run. In any case, imgapi will only import
`amd64/x86_64` images implicitly, so don't include the arch or the image won't
be found. Images that are not tagged with `cloud` will no thave `cloud-init`
installed, and will not set up correctly post-creation. They will be created,
but you won't be able to log into them.

A valid alias will look like `images:centos/7/cloud` or `ubuntu:f`. You must
include the image repo source to import the image to imgapi.

Once you have found an image you'll need to import it, and modify the access
control.

From the headnode, import the image:

    sdc-imgapi /images?action=import-lxd-image\&alias="${alias:?}" -X POST

After the image is installed, use `sdc-imgadm list type=lxd` to find the image
UUID.

Triton users will only be able to see the image via cloudapi if they are on the
ACL, or the image is public.

To add users to the ACL, use the following command:

    sdc-imgapi /images/${uuid:?}/acl?action=add -X POST -d '["<user_uuid>"]'

Note: this is a JSON array, so you can list multiple owners.

To make the image public, use the following command:

    sdc-imgapi "/images/${uuid:?}?action=update" -X POST -d '{"public":true}'

Making an image public does not remove the ACL, it is just ignored. To make an
image private pass `{"public":false}`. Access will revert to the existing ACL.

## Provisioning via CloudAPI

Following the steps above, you should now be able to proviison instances using
CloudAPI. Use `triton images` to list images and `triton packages` to list
packages. You'll need to select packages that use no brand requirement. Packages
that work for SmartOS (joyent) or LX brand will work fine.

Linux CNs do not support fabric networking, so if you have fabric networking
configured you'll need to explicitly pass hard VLAN networks at create time
or the instance will fail to be created.

Not all provision options are supported. E.g., delegated datasets. Provisions
are equally likely to fail or silently ignore unsupported options. If there's
a feature you're eagerly looking forward to, file an issue.

[triton-upgrade]: https://docs.tritondatacenter.com/private-cloud/maint-and-upgrades
