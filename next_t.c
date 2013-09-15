// Copyright (C) 2001-2013 Mischa Sandberg <mischasan@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License Version 2 as
// published by the Free Software Foundation.  You may not use, modify or
// distribute this program under any other version of the GNU General
// Public License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//  IF YOU HAVE NO WAY OF WORKING WITH GPL, CONTACT ME.
//-------------------------------------------------------------------------------
#include <errno.h>	//strerror
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include <tap.h>

#include "hx.h"

static int test_locked(HXFILE*);
static const char *strerr(void)
{ return errno ? strerror(errno) : "0"; }
//--------------|---------------------------------------------
char const *	recv[] = {
    "eta",   "theta",  "iota",   "kappa",
    "lambda",  "mu",    "nu",     "xi",     "omicron",
    "pi",      "rho",   "sigma",  "tau",    "upsilon",
    "phi",     "chi",   "psi",    "OMEGA",
    "yahoo",   "yeast", "yodel", "yummy"
};
const int	nrecs = sizeof(recv)/sizeof(*recv);

int main(void)
{
    HXRET	rc, len;
    HXFILE	*hp;

    plan_tests(44);
    setvbuf(stdout, 0, _IOLBF, 0);

    HXMODE  mode;   // test once without and once with mmap
    for (mode = 0; mode <= HX_MMAP; mode += HX_MMAP) {	//mode <= HX_MMAP
	rc = hxcreate("next_t.hx", 0755, 32, "ch", 5);
	ok(HXOKAY == rc, "created next_t.hx: %s", hxerror(rc));

	hp = hxopen("next_t.hx", HX_UPDATE + mode);
	ok(hp, "opened next_t.hx with %s", hxmode(HX_UPDATE + mode));

	char const **recp = recv + nrecs;
	while (--recp >= recv &&
		0 <= (rc = hxput(hp, *recp,
					strlen(*recp)))) {
	    if (hxdebug) fprintf(stderr, "# >%s\n", *recp);
	}
	ok(rc == 0, "inserted test records");

	int	bufsize = hxmaxrec(hp);
	char	buf[bufsize]; // hxmaxrec(hp) for pgsize 64
	int		actual = 0, deletable = 0, deleted = 0;

	while ((len = hxnext(hp, buf, bufsize)) > 0) {
	    if (hxdebug) fprintf(stderr, "# <%.*s\n", len, buf);
	    ++actual;
	    if (buf[0] < 'm' || !memcmp(buf, "yo",2)) {
		++deletable;
	    }
	}

	ok(rc == 0, "first scan completed: %s", hxerror(rc));
	ok(actual == nrecs, "first scan returned %d/%d records", actual, nrecs);

	hxclose(hp);

	hp = hxopen("next_t.hx", HX_UPDATE + mode);
	len = hxnext(hp, buf, bufsize);
	char ch = buf[len-1] ^= 1;
	rc = hxput(hp, buf, len);
	ok(rc == len, "update of current record allowed");

	buf[len-1] = 0;
	rc = hxget(hp, buf, len);
	ok(rc == len, "updated record fetched");
	ok(buf[len-1] == ch, "update succeeded");

	rc = hxput(hp, "crap", 4);
	ok(rc == HXERR_BAD_REQUEST, "update of different key while scanning is rejected: %s", hxerror(rc));

	rc = hxput(hp, buf, len+1);
	ok(rc == len, "small increase in record size allowed: %s", hxerror(rc));

	rc = hxput(hp, buf, len-1);
	ok(rc > len-1, "update that shortens record accepted: %s", hxerror(rc));

	rc = hxget(hp, buf, len);
	ok(rc == len-1, "record shortened: %s", hxerror(rc));

	rc = hxdel(hp, buf);
	ok(rc == len-1, "delete of %.*s accepted: %s", len, buf, hxerror(rc));

	rc = hxget(hp, buf, len);
	ok(rc == 0, "delete done: %s", hxerror(rc));

	hxclose(hp);

	hp = hxopen("next_t.hx", HX_UPDATE + mode);

	// restore record deleted in previous test!
	buf[len - 1] = ch ^ 1;
	rc = hxput(hp, buf, len);
	ok(rc == 0, "record restored: %s", hxerror(rc));

	int	scanned = 0;
	while ((len = hxnext(hp, buf, sizeof(buf))) > 0) {
	    if (!scanned++) {
		errno = 0;
		ok(test_locked(hp) == 1, "first hxnext leaves file locked: %s", strerr());
	    }

	    if (hxdebug) fprintf(stderr, "# hxnext: %.*s\n", len, buf);
	    if (buf[0] < 'm' || !memcmp(buf, "yo",2)) {
		rc = hxdel(hp, buf);
		if (rc > 0)
		    ++deleted;
		if (hxdebug) fprintf(stderr, "# hxdel(%.*s): %s\n", len, buf, hxerror(rc));
	    }
	}

	errno = 0;
	ok(test_locked(hp) == 0, "final hxnext leaves file unlocked: %s", strerr());
	ok(len == 0, "scan with deletes completed: %s", hxerror(len));
	ok(scanned == nrecs, "%d/%d records scanned for deletions", scanned, nrecs);
	ok(deleted == deletable, "%d/%d deletable records deleted", deleted, deletable);

	hxrel(hp);
	ok(hxnext(hp, buf, sizeof(buf)), "hxnext restarts after hxrel");

	hxclose(hp);

	hp = hxopen("next_t.hx", HX_READ);
	int save = hxdebug;
	hxdebug = 1;
        hxtime = 1 - hxtime;    // strictly for coverage
	rc = hxfix(hp,0,0,0,0);
	hxdebug = save;
	if (!ok(rc == (HXRET) HX_UPDATE, "file not corrupted"))
	    system("cp next_t.hx foo.hx; chmod -w foo.hx");

	hxclose(hp);
    }

    return exit_status();
}

static int
test_locked(HXFILE *hp)
{
    int	    ret;

    if (fork()) 
	return wait(&ret) < 0 ? -1 : WEXITSTATUS(ret);

    // Attempt an exclusive lock of entire file
    struct flock    what;
    what.l_type = F_WRLCK;
    what.l_start = 0;
    what.l_len = 0;
    what.l_whence = SEEK_SET;
    what.l_pid = 0;
    ret = fcntl(hxfileno(hp), F_SETLK, &what);

    if (!ret) { // lock succeeded; undo it
	what.l_type = F_UNLCK;
	fcntl(hxfileno(hp), F_SETLK, &what);
    }

    _exit(ret && errno == EAGAIN);
}
