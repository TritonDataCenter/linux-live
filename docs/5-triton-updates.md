# Updating an existing triton headnode to use Linux CNs

This document describes how to update an existing triton setup (most likely COAL, given the highly experimental stage of all the components involved) in order to be able to boot Linux CNs and provision VMs on those using LXD.

**Please, note that this is a development document. All these features are still under active development
and are ages away from being completed.**

## 1. Update sdcadm

```
sdcadm self-update --latest -C dev
```

## 2. Update booter

```
sdcadm up dhcpd -y -C dev
sdcadm experimental update-other
```

## 3. Update headnode components

```
sdcadm up -C experimental imgapi@~linuxcn -y
sdcadm up -C experimental vmapi@~linuxcn -y
sdcadm up -C experimental cnapi@~linuxcn -y
```

## 4. Install linux (Debian) USB image

Please, refer to [Platform Image](./2-platform-image.md) for build instructions
for LinuxCN Platform images.

```
sdcadm platform install /var/tmp/linux-platform-20201211T092901Z.tgz --os=linux

sdcadm platform list
VERSION           CURRENT_PLATFORM  BOOT_PLATFORM  LATEST  DEFAULT  OS
20201211T092901Z  0                 0              true    false    linux
20201124T154309Z  2                 2              true    false    smartos
```

Two approaches - you can set the default for all boots:

```
sdcadm platform set-default 20201211T092901Z
```

Or boot the linux CN initially (which will grab the SmartOS image), then
set that CN to use linux and reboot the CN (will now grab linux image):

```
sdcadm platform assign 20201211T092901Z $LINUXCN
```

## 5. Update joysetup scripts for linux support

```
sdc-login -l assets
cp -r /assets/extra/joysetup /var/tmp/joysetup
mount -f lofs /var/tmp/joysetup /assets/extra/joysetup
exit
```

From the global zone run:

```
curl -k https://raw.githubusercontent.com/joyent/sdc-headnode/linuxcn/scripts/joysetup.sh > /zones/$(vmadm lookup alias=assets0)/root/var/tmp/joysetup/joysetup.sh
curl -k https://raw.githubusercontent.com/joyent/sdc-headnode/linuxcn/scripts/agentsetup.sh > /zones/$(vmadm lookup alias=assets0)/root/var/tmp/joysetup/agentsetup.sh
```

(Optional) These can also be copied to the usbkey (so after a headnode restart, it will use the same scripts):

```
sdc-usbkey mount
cp /zones/$(vmadm lookup alias=assets0)/root/var/tmp/joysetup/joysetup.sh /mnt/usbkey/scripts/
cp /zones/$(vmadm lookup alias=assets0)/root/var/tmp/joysetup/agentsetup.sh /mnt/usbkey/scripts/
```

## 6. Boot linux CN (iPXE boot)

I use VMWare (network boot on Admin network, added multiple disks, second nic for external...)

Once booted - you can login to the Linux CN (root password will be the same as the headnode root password) - SSH will also be available.

# (Optional) Configure external network

```
ip link set ens33 down
ip link set ens33 name external
ip link set ens33 up
```

### 7. Run sdc-server list|setup on the Linux server

```
sdc-server list
HOSTNAME             UUID                                 VERSION    SETUP    STATUS      RAM  ADMIN_IP
debian-xxx           04664d56-7b07-631e-6cb2-ca0ce9979b4d     7.0     false  running     1993  172.16.2.201
headnode             20f5a12a-dad7-dd11-a1a5-1c872c47f978     7.0     true   running    16255  172.16.2.1

sdc-server setup -s 04664d56-7b07-631e-6cb2-ca0ce9979b4d hostname=linux-1
```

## 8. Verify with sdc-server and sdc-vmapi tools

At this point the linux CN should be running and you can use `sdc-oneachnode` and tools like `sdc-server` and `sdc-vmapi` against this server.

## 9. Import an lxd image into imgapi

```
# Note that the format is the same as lxd, so prefixed with "images:" or "ubuntu:".
sdc-imgapi /images'?action=import-lxd-image&alias=images:alpine/3.12' -X POST
```

## 10. Provision an lxd instance

```
cat lxd.json
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

sdc-vmapi /vms -X POST -d@lxd.json
{
  "vm_uuid": "05370106-2c50-4f18-8d7e-3e1e80465344",
  "job_uuid": "7444e332-fce2-4085-8bda-65047d97f171"
}

sdc-waitforjob "7444e332-fce2-4085-8bda-65047d97f171"

sdc-vmapi /jobs/7444e332-fce2-4085-8bda-65047d97f171
```


## 11. Provision through Triton CLI

To be able to provision through the Triton CLI (or CloudAPI) you'll need to
update CNAPI and make the lxd images public.

Update CNAPI:

```
sdcadm up -C experimental cnapi@~linuxcn -y
```

Make lxd images public (so Triton CLI can find them):
```
sdc-imgapi /images?type=lxd | json -Hga uuid | xargs -n1 -I '{}' sdc-imgapi '/images/{}?action=update' -X POST -d '{"public": true}'
```

Find image and provision a lx container on Linux:
```
triton images | grep lxd
71b94f10  alpine_3.12_amd64_default       20201204_13_00  P      linux    lxd           2020-12-04
```

Explicitly find a network to use (the default network is usually a fabric
network, which will not be available on Linux):
```
triton networks
SHORTID   NAME                          SUBNET            GATEWAY        FABRIC  VLAN  PUBLIC
0abea36b  My-Fabric-Network             192.168.128.0/22  192.168.128.1  true    2     false
b2336046  sdc_nat                       -                 -              -       -     true
```

Provision a container using the lxd image and the chosen network:

```
triton create -w 71b94f10 sample-256M -N sdc_nat
Creating instance 95157ccb (95157ccb-9f04-4ede-894c-f9e995e3fae3, alpine_3.12_amd64_default@20201204_13_00)
Created instance 95157ccb (95157ccb-9f04-4ede-894c-f9e995e3fae3) in 5s
```
