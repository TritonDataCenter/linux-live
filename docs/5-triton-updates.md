# Updating an existing triton headnode to use Linux CNs

This document describes how to update an existing triton setup in
order to be able to boot Linux CNs and provision VMs on those using LXD.

As of June, 2021, Linux CN support has been merged into the main development
branch and all images are available in the `dev` channel. To install support
for Linux CNs, update all Triton components. This includes all `sdcadm update`
and `sdcadm experimental update-*` commands.

**Please, note that this is a development document. All these features are still
under active development and are ages away from being completed.**

## Update sdcadm

```bash
sdcadm self-update --latest -C dev
```

## Update headnode components

```bash
sdcadm up -C dev --all --latest -y
```

## Update additional components

```bash
sdcadm experimental update-agents --latest --all -y
sdcadm experimental update-gz-tools --latest
sdcadm experimental update-other
```

## Install linux (Debian) USB image

Currently, platform images are available from manta.

```bash
curl -o /var/tmp/platform-20201211T092901Z.tgz https://us-east.manta.joyent.com/Joyent_Dev/public/xxxxx/platform-20201211T092901Z.tgz
sdcadm platform install /var/tmp/platform-20201211T092901Z.tgz
```

```shell
# sdcadm platform list
VERSION           CURRENT_PLATFORM  BOOT_PLATFORM  LATEST  DEFAULT  OS
20201211T092901Z  0                 0              true    false    linux
20201124T154309Z  2                 2              true    false    smartos
```

Two approaches - you can set the default for all boots (not yet recomended):

```bash
sdcadm platform set-default 20201211T092901Z
```

Or boot the CN initially to SmartOS, then set that CN to use linux and reboot
it. (will now grab linux image):

```bash
sdcadm platform assign 20201211T092901Z $LINUXCN
```

## Boot linux CN (iPXE boot)

Once booted - you can login to the Linux CN. Unlike SmartOS, the root password
will be the same as the headnode root password. After the CN is set up you can
ssh using the SDC key.

## Run sdc-server list|setup on the Linux server

```bash
sdc-server list
HOSTNAME             UUID                                 VERSION    SETUP    STATUS      RAM  ADMIN_IP
debian-xxx           04664d56-7b07-631e-6cb2-ca0ce9979b4d     7.0     false  running     1993  172.16.2.201
headnode             20f5a12a-dad7-dd11-a1a5-1c872c47f978     7.0     true   running    16255  172.16.2.1

sdc-server setup -s 04664d56-7b07-631e-6cb2-ca0ce9979b4d hostname=linux-1
```

## Configure nic_tags on the Linux CN

Currently neither SDN (underlay/overlay) networking or LACP are not supported.
Other nic_tags such as internal, external, etc. need to be configured on the
appropriate individual interfaces. This is easiest done via AdminUI.

## Verify with sdc-server and sdc-vmapi tools

At this point the linux CN should be running and you can use `sdc-oneachnode` and tools like `sdc-server` and `sdc-vmapi` against this server.

## Import an lxd image into imgapi

Note that the format is the same as lxd, so prefixed with `images:` or
`ubuntu:`. You can run `lxc image list images:` or `lxc image list ubuntu:` to
see images available. In the future, sdc-imgadm will be updated to list public
LXD images.

```bash
sdc-imgapi /images?action=import-lxd-image\&alias=images:alpine/3.12 -X POST
```

## Provision an lxd instance

This is an example. Use appropriate values for your Triton installation.

```json
{
  "server_uuid": "UPDATE-ME",
  "alias": "lxd-1",
  "image_uuid": "UPDATE-ME",
  "billing_id": "c53363bb-7b40-60d0-b3e9-fbcc246eb406", "billing_by_name_IGNORED": "sample-1G",
  "brand": "lx",
  "cpu_cap": 100,
  "owner_uuid": "UPDATE-ME",
  "resolvers": [
    "8.8.8.8",
    "8.8.4.4"
  ],
  "ram": "1024",
  "networks": [ "UPDATE-ME-EXTERNAL-NETWORK" ]
}
```

```shell
# sdc-vmapi /vms -X POST -d@lxd.json
{
  "vm_uuid": "05370106-2c50-4f18-8d7e-3e1e80465344",
  "job_uuid": "7444e332-fce2-4085-8bda-65047d97f171"
}
```

```shell
# sdc-waitforjob "7444e332-fce2-4085-8bda-65047d97f171"
# sdc-vmapi /jobs/7444e332-fce2-4085-8bda-65047d97f171
```

## Provision through Triton CLI

Make lxd images public (so Triton CLI can find them):

```bash
for img in $(sdc-imgapi /images?type=lxd | json -Ha uuid); do
    sdc-imgapi "/images/$img?action=update" -X POST -d '{"public":true}'
done
```

Find image and provision a lx container on Linux:

```bash
$ triton images | grep lxd
71b94f10  alpine_3.12_amd64_default       20201204_13_00  P      linux    lxd           2020-12-04
```

Explicitly find a network to use (the default network is usually a fabric
network, which will not be available on Linux):

```bash
$ triton networks
SHORTID   NAME                          SUBNET            GATEWAY        FABRIC  VLAN  PUBLIC
0abea36b  My-Fabric-Network             192.168.128.0/22  192.168.128.1  true    2     false
b2336046  sdc_nat                       -                 -              -       -     true
```

Provision a container using the lxd image and the chosen network:

```bash
triton create -w 71b94f10 sample-256M -N sdc_nat
Creating instance 95157ccb (95157ccb-9f04-4ede-894c-f9e995e3fae3, alpine_3.12_amd64_default@20201204_13_00)
Created instance 95157ccb (95157ccb-9f04-4ede-894c-f9e995e3fae3) in 5s
```

## Using AdminUI

AdminUI hasn't yet had any updates explicitly dealing with Linux CNs or LXD
images. Some things work (e.g., server platform assignment), while others
(e.g., importing lxd images) are known to not work. Other things may or may
not work and much of it hasn't been tested yet.

Because of this, AdminUI isn't currently a supported method of managing Linux
CNs. This is expected to change, but we really need to get the lower level
interfaces formalized first, and so there's no estimated time frame for when
this might happen yet.
