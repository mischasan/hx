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
#include <errno.h>
#include <pthread.h>
#include <signal.h>	// kill
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "tap.h"

#include "hx.h"

static void dowait(void);
//static void pt_locker(void *v);
//static int locker;

int
main(void)
{
    HXFILE	    *hp;
    char	    buf[9];
    int		    ret, rc;
    pid_t	    child;
    int		    synch[2];
    char	    junk[1];

    plan_tests(17);

    setvbuf(stdout, 0, _IOLBF, 0);
    setvbuf(stderr, 0, _IOLBF, 0);

    rc = hxcreate("lock_t.hx", 0644, 1024, "ch", 2);
    ok(rc == 0, "hxcreate lock_t.hx: %s", hxerror(rc));

    hp = hxopen("lock_t.hx", HX_UPDATE);
    ok(hp != NULL, "hxopen lock_t.hx for update: %s", hp ? "ok" : strerror(errno));

    rc = hxput(hp, "a:1", 3);
    ok(rc == 0, "hxput(a:1): added record: (%d) %s", rc, rc < 0 ? hxerror(rc) : "");

    strcpy(buf, "a:?");
    rc = hxget(hp, buf, 0);
    ok(rc == 3, "hxget checks for key present: (%d): %s", rc, rc < 0 ? hxerror(rc) : "");
    ok(!strcmp(buf, "a:?"), "hxget(%s,0) leaves buffer value unchanged", buf);

    // Until hxhold(hp,*,0) is fixed, make it reject 0
    rc = hxhold(hp, buf, 0);
    ok(rc == HXERR_BAD_REQUEST, "PROTEM: reject hxhold(hp, *, 0): %s", hxerror(rc));
    rc = hxrel(hp);
    ok(rc == HXOKAY, "hxrel returns %d %s", rc, hxerror(rc));
    ok(!pipe(synch), "created synch pipe 0");
    if (!(child = fork())) {
	hxclose(hp);
        hxproc = "\tBG";
	hp = hxopen("lock_t.hx", HX_UPDATE);

	fprintf(stderr, "# BGwait for synch\n");
	read(synch[0], junk, 1);

	fprintf(stderr, "# BGcall hxhold...\n");
	ret = hxhold(hp, buf, 3);
	fprintf(stderr, "# BGhxhold returns %d, record=%s\n", ret, buf);

	buf[2] = '0' + (buf[2] - '0') * 3;
	fprintf(stderr, "# bg: val *= 3; call hxput(%s)...\n", buf);
	ret = hxput(hp, buf, 3);
	fprintf(stderr, "# bg: hxput(%s) returns %d\n", buf, ret);
	_exit(0);
    }

    fprintf(stderr, "forked bg pid=%d\n", child);
    ret = hxhold(hp, buf, 3);
    ok(ret == 3, "hxhold acquires record=%.3s: (%d): %s", buf, ret, ret < 0 ? hxerror(ret) : "");
    ok(!strcmp(buf, "a:1"), "hxhold returns correct record");

    fprintf(stderr, "# pass synch to bg\n");
    write(synch[1], junk, 1);
    sleep(2);	// give bg a chance to call hxhold ...

    buf[2] += 1;
    ret = hxput(hp, buf, 3);
    ok(ret > 0, "hxput updates val += 1; record=%.3s: (%d): %s", buf, ret, ret < 0 ? hxerror(ret) : "");

    // Ensure that child completes
    dowait();

    buf[2] = 0;
    ret = hxget(hp, buf, 3);
    ok(ret == 3, "retrieves record %.3s: %s", buf, hxerror(ret));
    ok(buf[2] == '6', "atomic updates do (1+1)*3 = %s", buf);

    // Now test same thing with hxrel,
    if (!(child = fork())) {
        setenv("HXPROC", "(BG2)", 1);
	hxclose(hp);
	hp = hxopen("lock_t.hx", HX_READ);

	fprintf(stderr, "# bg2: wait for synch\n");
	read(synch[0], junk, 1);

	fprintf(stderr, "# bg2: call hxget...\n");
	ret = hxget(hp, buf, 3);
	fprintf(stderr, "# bg2: hxget returns %d, record=%s\n", ret, buf);
        fprintf(stderr, " bg2 exit(0)\n");
	_exit(0);
    }
    fprintf(stderr, "forked bg2 pid=%d\n", child);

    ret = hxhold(hp, buf, 3);
    ok(ret == 3, "hold record again: %s", hxerror(ret));
    fprintf(stderr, "# pass synch to bg2\n");
    write(synch[1], junk, 1);
    sleep(2);

    ok(!kill(child,0), "blocking bg2");
    ret = hxrel(hp);
    ok(ret == HXOKAY, "released record: %s", hxerror(ret));
    dowait();
    ok(1, "hxget completed in bg2");
    return exit_status();
}

static void
dowait()
{
    int     ret;
    fprintf(stderr, "# wait for child to exit...\n");
    pid_t   pid = wait(&ret);

    if (WIFEXITED(ret))
        fprintf(stderr, "# child %d exited with %d\n", pid, WEXITSTATUS(ret));
    else if (WIFSIGNALED(ret))
        fprintf(stderr, "# child %d killed by %d\n", pid, WTERMSIG(ret));
    else
        fprintf(stderr, "# child %d returned %d?\n", pid, ret);
}
