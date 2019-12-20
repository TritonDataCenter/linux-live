/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Copyright 2019 Joyent, Inc.
 */

/*
 * If an lxc instance does not use a UID namespace, the container is consider a
 * privileged container.  Vulnerabilities that lead to escape from a privileged
 * container are all too common.  When a UID namespace is used, processes in
 * that namespace will have UIDs mapped such that UID 0 in the container is a
 * non-privileged process outside of the container.
 *
 * These mapped UIDs cause problems for accessing files.  For instance, the root
 * directory is typically writable by root.  However, if UID 0 in the container
 * is mapped to UID 1000 outside of the container, the UID/EUID 0 process in the
 * container will be accessing the file system as UID 1000.  This causes
 * permission problems.  Similar issues exist for GIDs.
 *
 * In a Triton cloud, each lxc instance is a clone of an lx/lxc image.  Files in
 * images have the user and group ownership one would expect.  This program
 * takes a range of IDs and does a recursive chown to change ownership from one
 * UID:GID space to another.
 */

#include <assert.h>
#include <errno.h>
#include <ftw.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define	ARRAY_SIZE(a) (sizeof (a) / sizeof (a[0]))

id_t from, to, count, delta;
bool verbose, check, dryrun, preflight;
const char *cmd;

int exitval = 0;

// XXX fix
static int
fix_acl(const char *path)
{
	return (0);
}

// XXX fix
static int
fix_xattr(const char *path)
{
	return (0);
}

typedef enum {
	IDT_UID = 1,
	IDT_GID
} id_type_t;

/*
 * If the global variable preflight is set, the id is expected to be in the
 * range [from, from + count).  When not in preflight, the id may be also be in
 * the rnage [to, to + count), but only if there file has multiple links.  In
 * order to avoid tracking the (dev,ino) tuple of every inode visited, we assume
 * that in the second pass any file already in the "to" range is there because
 * it is not the first time this file has bene visited (previous time by a
 * different name).
 */
static bool
id_ok(id_type_t idtype, const char *path, const struct stat *sb,
    bool *in_to_id_space)
{
	id_t id;
	const char *idstr;

	if (idtype == IDT_UID) {
		id = sb->st_uid;
		idstr = "uid";
	} else {
		id = sb->st_gid;
		idstr = "gid";
	}

	*in_to_id_space = (id >= to && id < (to + count));

	/* Normal case: in "from" range. */
	if (id >= from && id < (from + count)) {
		return (true);
	}

	/*
	 * 1. The "to range ok" exception only allowed after preflight.
	 * 2. Directories are expected to have 2 links but nftw() only visits
	 *    one of them (not "..").
	 * 3. Other objects with only one link couldn't have already been
	 *    changed by a chown() on another link.
	 * 4. Not a directory, multiple links, out of "to" id space.
	 */
	if (preflight ||
	    S_ISDIR(sb->st_mode) ||
	    sb->st_nlink == 1 ||
	    id < to || id >= (to + count)) {
		(void) fprintf(stderr, "%s: %s %s %d out of range [%d, %d)\n",
		    cmd, path, idstr, id, from, from + count);
		return (false);
	}

	return (true);
}

static int
fixer(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
	int ret = 0;
	int newuid;
	int newgid;
	bool u_done, g_done;

	switch (typeflag) {
	case FTW_F:
	case FTW_D:
	case FTW_DP:
	case FTW_SL:
		break;
	case FTW_DNR:
		(void) fprintf(stderr, "%s: %s: directory not readable\n", cmd,
		    path);
		ret = -1;
		break;
	case FTW_NS:
		(void) fprintf(stderr, "%s: %s: stat failed: %s\n", cmd, path,
		    strerror(errno));
		/*
		 * Anything further checks require a valid stat structure, which
		 * we don't have.
		 */
		return (-1);
	case FTW_SLN:
		(void) fprintf(stderr, "%s: %s: unexpected typeflag FTW_SLN\n",
		    cmd, path);
		abort();
	default:
		(void) fprintf(stderr, "%s: %s: unexpected typeflag %d\n",
		    cmd, path, typeflag);
		abort();
	}

	if (!id_ok(IDT_UID, path, sb, &u_done) ||
	    !id_ok(IDT_GID, path, sb, &g_done)) {
		ret = -1;
	}

	newuid = u_done ? sb->st_uid : sb->st_uid + delta;
	newgid = g_done ? sb->st_gid : sb->st_gid + delta;

	if (verbose) {
		(void) printf("%s%s\tuid %d -> %d\tgid %d -> %d\tflag %d\n",
		    preflight ? "preflight\t" : "", path,
		    sb->st_uid, newuid, sb->st_gid, newgid, typeflag);
	}


	if (fix_acl(path) != 0 || fix_xattr(path) != 0) {
		// XXX fix
		(void) fprintf(stderr, "%s: acls and xattrs not supported\n",
		    cmd);
		ret = -1;
	}

	/* Let dryrun report all errors by returning 0. */
	if (preflight || dryrun) {
		if (ret != 0 && exitval == 0) {
			exitval = 1;
		}
		return (0);
	}

	/* Stop on the first non-dryrun error */
	if (ret != 0) {
		return (ret);
	}

	if (u_done != g_done) {
		/*
		 * This indicates that someone was mucking with the file system
		 * after preflight succeeded.
		 */
		(void) fprintf(stderr, "%s: %s: uid %d in %s range but "
		    "gid %d is in %s range", cmd, path,
		    sb->st_uid, u_done ? "old" : "new",
		    sb->st_gid, g_done ? "old" : "new");
		return (-1);
	}
	if (u_done) {
		return (0);
	}

	if (lchown(path, newuid, newgid) != 0) {
		(void) fprintf(stderr, "%s: chown(%s, %d, %d): %s\n",
		    cmd, path, newuid, newgid, strerror(errno));
		return (-1);
	}

	if (check) {
		struct stat ck;

		if (lstat(path, &ck) != 0) {
			(void) fprintf(stderr,
			    "%s: check failed; stat(%s): %s\n", cmd, path,
			    strerror(errno));
			return (-1);
		}
		/*
		 * Just in case some security module determines that setid bits
		 * should be cleared during ownership change, we also check
		 * st_mode.
		 */
		if (ck.st_mode != sb->st_mode) {
			(void) fprintf(stderr, "%s Unexpected change of "
			    "permission bits on %s: was: %04o is %04o\n",
			    cmd, path, sb->st_mode, ck.st_mode);
			ret = -1;
		}
		if (ck.st_uid != newuid) {
			(void) fprintf(stderr, "%s: Unexpected UID on %s: "
			    "was %d expected %d is %d\n", cmd, path,
			    sb->st_uid, newuid, ck.st_uid);
			ret = -1;
		}
		if (ck.st_gid != newgid) {
			(void) fprintf(stderr, "%s: Unexpected GID on %s: "
			    "was %d expected %d is %d\n", cmd, path,
			    sb->st_gid, newgid, ck.st_gid);
			ret = -1;
		}
		// XXX Also check acl, xattr
	}

	return (ret);
}

static int
parse_idstr(const char *str, id_t *valp)
{
	char *end;
	id_t val;

	errno = 0;
	val = strtol(str, &end, 10);
	if (errno != 0) {
		return (errno);
	}
	if (str == end || *end != '\0') {
		return (EINVAL);
	}

	*valp = val;
	return (0);
}

static void
usage(FILE *out)
{
	(void) fprintf(out, "Usage:\n"
	    "\n"
	    "\t%s [-chnv] directory from_id to_id count\n"
	    "\n"
	    "Options:\n"
	    "   -c   Check owner and group after modifying\n"
	    "   -n   Dry run - walk the directory but make no changes\n"
	    "   -v   Verbose\n"
	    "   -h   Show this message\n"
	    "\n"
	    "Operation:\n"
	    "\n"
	    "\tThe specified directory is walked.  Every object in the tree\n"
	    "\trooted <directory> must have a UID and GID that is between\n"
	    "\t<from> and <from> + <count>.  That is:\n"
	    "\n"
	    "\t\tfrom <= [UG]ID < from + count\n"
	    "\n"
	    "\tThe UID and GID of each object is updated to fall in the set\n"
	    "\tUIDs and GIDs starting at <to>.  That is:\n"
	    "\n"
	    "\t\tnewid = id - from + to\n"
	    "\n"
	    "\t<from> may be greater than or less than <to>.  The ranges\n"
	    "\tmust not overlap.\n", cmd);
}

int
main(int argc, char **argv)
{
	int opt;
	char *dir;
	int err;
	struct rlimit rlim;
	int fds;

	cmd = basename(argv[0]);

	while ((opt = getopt(argc, argv, ":chnv")) != -1)  {
		switch (opt) {
		case 'c':
			check = true;
			break;
		case 'h':
			usage(stdout);
			return (0);
		case 'n':
			dryrun = true;
			break;
		case 'v':
			verbose = true;
			break;
		case '?':
			(void) fprintf(stdout, "%s: Invalid option -%c\n",
			    cmd, optopt);
			usage(stderr);
			return (1);
		default:
			(void) fprintf(stdout, "%s: Unhandled option -%c\n",
			    cmd, opt);
			abort();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 4) {
		usage(stderr);
		return (1);
	}
	dir = argv[0];
	argc--;
	argv++;

	id_t *vals[] = {&from, &to, &count};
	for (int i = 0; i < ARRAY_SIZE(vals); i++) {
		if ((err = parse_idstr(argv[0], vals[i])) != 0) {
			(void) fprintf(stderr, "%s: \"%s\": %s\n", cmd, argv[0],
			    strerror(err));
			return (1);
		}
		argc--;
		argv++;
	}
	if (from > INT_MAX - count) {
		(void) fprintf(stderr,
		    "%s: integer overflow: %d + %d > INT_MAX\n", cmd,
		    from, count);
		return (1);
	}
	if (to > INT_MAX - count) {
		(void) fprintf(stderr,
		    "%s: integer overflow: %d + %d > INT_MAX\n", cmd,
		    to, count);
		return (1);
	}
	delta = to - from;

	if ((to > from && to < from + count) ||
	    (to < from && from < to + count)) {
		(void) fprintf(stderr, "%s: ranges overlap\n", cmd);
		return (1);
	}

	if (verbose) {
		(void) printf(
		    "Mapping UID and GID %d - %d to %d - %d under %s\n",
		    from, from + count - 1, to, to + count - 1, dir);
	}

	/*
	 * When getting the rlimit, remember that stdin, stdout, and stderr are
	 * all open.
	 */
	if (getrlimit(RLIMIT_NOFILE, &rlim) != 0) {
		(void) fprintf(stderr, "%s: getrlimit: %s\n", cmd,
		    strerror(errno));
		fds = 1;
	} else {
		fds = rlim.rlim_cur - 3;
	}

	/*
	 * TODO
	 * Create mount and uid namespaces and bind this process to them.  The
	 * mount namespace will ensure that we don't wander out due to a
	 * directory walk bug, perhaps perturbed by activity in a subdirectory
	 * as it is running.  The uid namespace will ensure that every chown()
	 * call only uses UIDs that are in the new namespace.
	 */

	/*
	 * Do a first pass to identify any problems before we make changes.
	 * This is also important to handle hard links.
	 */
	if (!dryrun) {
		preflight = true;
		err = nftw(dir, fixer, fds, FTW_PHYS);
		assert(err == 0);
		if (exitval != 0) {
			return (exitval);
		}
		preflight = false;
	}

	if (nftw(dir, fixer, fds, FTW_PHYS) != 0) {
		return (1);
	}
	return (exitval);
}
