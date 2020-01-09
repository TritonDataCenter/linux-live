/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Copyright 2019 Joyent, Inc.
 */

/*
 * This program sets the hostid based on a crc32 of the system's UUID string.
 * Normally, a Linux machine sets the hostid based on the system's IP address or
 * based on a random number if the IP addess is not known.
 *
 * This is useful on live media that would like to import a zfs pool so that
 * pool-in-use detection doesn't get tripped up across reboots.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#define	ETC_HOSTID "/etc/hostid"
#define	UUIDSTRLEN 36
#define	DMIDECODE "/usr/sbin/dmidecode -s system-uuid"

int
main(int argc, char **argv)
{
	const char *path;
	struct stat sb;
	FILE *dmidecode;
	unsigned char buf[UUIDSTRLEN + 2];
	int fd;
	size_t nbytes;
	uint32_t crc;

	if (argc == 2) {
		path = argv[1];
	} else if (argc != 1) {
		(void) fprintf(stderr, "Usage: sethostid [file]\n");
		return (1);
	} else {
		path = ETC_HOSTID;
	}

	if (lstat(path, &sb) == 0) {
		(void) fprintf(stderr, "sethostid: %s exists\n", path);
		return (1);
	}

	dmidecode = popen(DMIDECODE, "r");
	if (dmidecode == NULL) {
		(void) fprintf(stderr, "sethostid: failed to run '%s'\n",
		    DMIDECODE);
		return (1);
	}

	nbytes = fread(buf, 1, sizeof (buf), dmidecode);
	(void) fclose(dmidecode);
	if (nbytes != UUIDSTRLEN + 1) {
		(void) fprintf(stderr, "sethostid: got %ld bytes from "
		    "dmidecode, expected %d\n", nbytes, UUIDSTRLEN + 1);
		return (1);
	}

	crc = crc32(0, NULL, 0);
	crc = crc32(crc, buf, UUIDSTRLEN);

	fd = open(path, O_WRONLY |O_CREAT | O_EXCL);
	if (fd < 0) {
		(void) fprintf(stderr, "sethostid: %s: %s\n", path,
		    strerror(errno));
		return (1);
	}
	if (write(fd, &crc, sizeof (crc)) != sizeof (crc)) {
		(void) fprintf(stderr, "sethostid: write: %s\n",
		    strerror(errno));
		return (1);
	}
	(void) fchmod(fd, S_IRUSR | S_IRGRP | S_IROTH);
	(void) close(fd);

	return (0);
}
