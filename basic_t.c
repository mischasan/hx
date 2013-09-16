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
#include <sys/stat.h>
#include "tap.h"

#include "_hx.h"
#include "hx_.h"
#include "util.h"

static int mydiff(char const*r1 __unused, char const*r2 __unused, char const*udata __unused, int uleng __unused) { return 0; }
static HXHASH myhash(char const*rp __unused, char const*udata __unused, int uleng __unused) { return 0; }
static int myload(char *recp __unused, int recsize __unused, char const *str __unused, char const*udata __unused, int uleng __unused) { return 2; }
static int mysave(char const*recp __unused, int reclen __unused, char const*str __unused, int bufsize __unused, char const*udata __unused, int uleng __unused)
    { return 3; }
static int mytest(char const*recp __unused, int reclen __unused, char const*udata __unused, int uleng __unused) { return 0; }

int
main(void)
{
    HXFILE	*hp;
    HXRET	rc;
    char	buf[99];
    int		len, mode;
    char const	*err;

    setvbuf(stdout, 0, _IOLBF, 0);
    setvbuf(stderr, 0, _IOLBF, 0);

    plan_tests(186);

    // COVERAGE
    int		fd = creat("basic_t.hx", 0755);
    HXROOT	root = (HXROOT){16,4,0,0};

    write(fd, &root, sizeof(root)-1);
    ok(!hxopen("basic_t.hx", HX_READ), "file too small: %s", strerror(errno));
    hxclose(hxopen("basic_t.hx", HX_REPAIR));

    root = (HXROOT){16,4,0,0};
    lseek(fd, 0, 0);
    write(fd, &root, sizeof(root));
    ok(!hxopen("basic_t.hx", HX_READ), "PGSIZE too small: %s", strerror(errno));
    hxclose(hxopen("basic_t.hx", HX_REPAIR));

    root = (HXROOT){123,4,0,0};
    lseek(fd, 0, 0);
    write(fd, &root, sizeof(root));
    ok(!hxopen("basic_t.hx", HX_READ), "PGSIZE not a power of 2: %s", strerror(errno));
    hxclose(hxopen("basic_t.hx", HX_REPAIR));

    root = (HXROOT){128,1,0,0};
    lseek(fd, 0, 0);
    write(fd, &root, sizeof(root));
    ok(!hxopen("basic_t.hx", HX_READ), "PGRATE too low: %s", strerror(errno));

    ok(hp = hxopen("basic_t.hx", HX_REPAIR), "REPAIR accepts PGRATE too low: %s", strerror(errno));
    hxclose(hp);

    root = (HXROOT){128,32,0,0};
    lseek(fd, 0, 0);
    write(fd, &root, sizeof(root));
    ok(!hxopen("basic_t.hx", HX_READ), "PGRATE too high: %s", strerror(errno));
    ok(hp = hxopen("basic_t.hx", HX_REPAIR), "REPAIR accepts PGRATE too high");
    hxclose(hp);

    root = (HXROOT){128,4,1,0};		// filesize < uleng
    lseek(fd, 0, 0);
    write(fd, &root, sizeof(root));
    ok(!hxopen("basic_t.hx", HX_READ), "filesize < uleng: %s", strerror(errno));
    ok(hp = hxopen("basic_t.hx", HX_REPAIR), "REPAIR accepts filesize < uleng");
    hxclose(hp);

    root = (HXROOT){32,4,27,0};		// uleng >= pgsize-6
    lseek(fd, 0, 0);
    write(fd, &root, 32);
    ok(!hxopen("basic_t.hx", HX_READ), "uleng >= pgsize-6");
    ok(hp = hxopen("basic_t.hx", HX_REPAIR), "REPAIR accepts uleng >= pgsize-6");
    hxclose(hp);

    close(fd);
    ok(!hxopen("basic_t.hx", -1), "hxopen rejects bad MODE arg: %s", strerror(errno));
    hxclose(NULL);

    rc = hxcreate("basic_t.hx", 0666, 4096, "ch", 2);
    ok(rc == HXOKAY, "create hxfile with version 0x100: %s", hxerror(rc));
    ok(hp = hxopen("basic_t.hx", 0), "open hxfile with version matching hxversion: %s", strerror(errno));
    hxclose(hp);

    hxversion += 0x0001;
    ok(hp = hxopen("basic_t.hx", 0), "open hxfile with version same major version and lower minor version: %s", strerror(errno));
    hxclose(hp);

    hxversion += 0x0100;
    hp = hxopen("basic_t.hx", 0);
    if (!ok(!hp, "hxopen rejects hxfile not matching library major version: %s", strerror(errno)))
        hxclose(hp);

    hxversion -= 0x0101;

    rc = hxcreate("basic_t.hx", 0666, 32768, "ch", 2);
    ok(rc == HXOKAY, "create hxfile with 32K pagesize: %s", hxerror(rc));
    ok(hp = hxopen("basic_t.hx", 0), "open hxfile with 32K pagesize: %s", strerror(errno));
    hxclose(hp);

    // (COVERAGE)
    // Create a hxfile with a single map byte in the root page.
    // The next map will be page 32 (8bits/byte * HXPGRATE).

    memcpy(buf, "ch\0defghijklmnopqrstuvwxy", 23);
    err = hxerror(rc = hxcreate("basic_t.hx", 0755, 32, buf, 23));
    ok(rc == HXOKAY, "create hxfile with only 1 map byte in root page: %s", hxerror(rc));
    hp = hxopen("basic_t.hx", HX_UPDATE);
    ok(hp, "found hx_ch.so");

    char    udata[99];
    memset(udata, '?', sizeof(udata));
    ok(32-9 == hxinfo(hp, udata, 5), "hxinfo returns correct length");
    ok(!memcmp(udata, "ch\0de?????", 10), "hxinfo fills small buffer as specified");
    ok(32-9 == hxinfo(hp, udata, 99), "hxinfo returns correct length");
    if (!ok(!memcmp(udata, "ch\0defghijklmnopqrstuvw?????????", 32),
	    "hxinfo fills large buffer as specified"))
	fprintf(stderr,"# buffer is (%.32s)\n", udata);

    struct stat	    sb;
    ok(!fstat(hxfileno(hp), &sb), "hxfileno returns a valid handle");
    ok(sb.st_size == 64, "hxfileno returns the correct handle");

    memset(buf, '-', 18);
    strcpy(buf+18, "]");
    
    ok((HXRET)HX_UPDATE == (rc = hxfix(hp,0,0,0,0)), "before puts hxcheck says: %s", hxmode(rc));
    for (buf[0] = 'A'; buf[0] < 'A'+HX_MAX_CHAIN-2; ++buf[0]) {
	rc = hxput(hp, buf, len = 10 + buf[0]%3);
	ok(rc == 0, "hxput('%c...',%d) returned %s (%d)", buf[0], len, hxerror(rc), rc);
        ok((HXRET)HX_UPDATE == (rc = hxfix(hp,0,0,0,0)), "... and hxcheck says: %s", hxmode(rc));
    }

    for (buf[0] = 'A'; buf[0] < 'A'+HX_MAX_CHAIN-2; ++buf[0]) {
	rc = hxget(hp, buf, 14);
	ok(rc == 10 + buf[0]%3, "hxget('%c...') returned %s (%d)", buf[0], hxerror(rc), rc);
    }

    ok(HXERR_BAD_REQUEST == hxput(hp, NULL, 0), "hxput rejects NULL record");
    ok(HXERR_BAD_REQUEST == hxput(hp, buf, -1), "hxput rejects record length < 0");
    buf[0] = 1;
    buf[1] = 0;
    ok(HXERR_BAD_RECORD == hxput(hp, buf, 2), "hxput rejects invalid record");
    hxclose(hp);

    ok(hp = hxopen("basic_t.hx", 0), "hxopen read-only");
    ok(HXERR_BAD_REQUEST == hxput(hp, buf, 1), "hxput rejects readonly HXFILE");
    ok(HXERR_BAD_REQUEST == hxpack(hp), "hxpack rejects readonly HXFILE");

    system("chx info basic_t.hx >&2");

    hp = hxopen("basic_t.hx", HX_UPDATE|HX_RECOVER);
    rc = hxfix(hp,0,0,0,0);
    ok(rc == (HXRET)HX_UPDATE, "check works on hxfile with an overflow map page: %s", hxmode(rc));
    buf[1] = '-'; buf[2] = 0;
    for (buf[0] = 'A'; buf[0] < 'A'+HX_MAX_CHAIN+1; buf[0] += 3) {
	err = hxerror(rc = hxdel(hp, buf));
	if (rc <= 0)
	    fprintf(stderr, "# hxdel(%s) returned: %s (%d)\n",
		    buf, err, rc);
    }

    system("chx info basic_t.hx >&2");

    err = hxerror(rc = hxpack(hp));
    if (!ok(rc == HXOKAY,
	    "hxpack works on hxfile with an overflow map page"))
	printf("# hxpack returned: %s (%d)\n", err, rc);

    hxclose(hp);

    system("chx info basic_t.hx >&2");

    // Create a hxfile that exercises recursive GrowFile:
    // - all records hash to either 'H' or 'X'.
    // - all records with hash 'H' have length maxrec/2-1
    // - all records with hash 'X' have length maxrec/2+1
    // This creates the worst-case scenario chain to split.
    // The only WORSE cases are:
    //	- recursion wraps to the start of the file and
    //	    resplits the chain originally being split!
    //  - multiple chains that require recursion.
    //	    With a single obscene chain, recursion only
    //	    reaches depth=2.

    rc = hxcreate("basic_t.hx", 0755, 32, "ch", 2);
    ok(rc == HXOKAY, "create hxfile for recursive GrowFile test: %s", hxerror(rc));
    hp = hxopen("basic_t.hx", HX_UPDATE|HX_RECOVER);
    ok(hp, "opened file for recursive GrowFile test: %s", strerror(errno));
    rc = 0;
    for (buf[0]='X', buf[1]='A', buf[2] = '[';
	    !rc && buf[1] < 'Z'; ++buf[1], buf[0] ^= 16) {

	int	buflen =  buf[0] == 'X' ? 8 : 6;

	hxput(hp, buf, buflen);
        rc = hxfix(hp,0,0,0,0);
	ok(rc == (HXRET)HX_UPDATE, "file okay after inserting %.2s: %s", buf, hxmode(rc));
    }
    ok(rc == (HXRET)HX_UPDATE, "recursive GrowFile succeeded");
    hxclose(hp);
    hxdebug = 0;

    // More error-case coverage:
    rc = hxstat(NULL, NULL);
    ok(rc == HXERR_BAD_REQUEST, "hxstat returns %s for NULL HXSTAT* arg", hxerror(rc));
    hp = hxopen("nemo.hx", HX_UPDATE);
    ok(!hp, "hxopen returns NULL for non-existent file: %s", strerror(errno));

    hp = hxopen("/dev/null", HX_UPDATE);
    ok(!hp, "hxopen returns NULL for /dev/null: %s", strerror(errno));

    rc = hxget(NULL, buf, 1);
    ok(rc == HXERR_BAD_REQUEST, "hxget rejects NULL HXFILE* arg: %s", hxerror(rc));

    rc = hxnext(NULL, buf, 1);
    ok(rc == HXERR_BAD_REQUEST, "hxnext rejects NULL HXFILE* arg: %s", hxerror(rc));

    rc = hxcreate("basic_t.hx", 0755, 2049, "test", 5);
    ok(rc == HXERR_BAD_REQUEST, "hxcreate rejects bad pgsize: %s", hxerror(rc));

    rc = hxcreate("basic_t.hx", 0755, 256, "ch", 251);
    ok(rc == HXERR_BAD_REQUEST, "hxcreate rejects bad uleng: %s", hxerror(rc));

    system("PS4='# '; set -x; touch bad_t.hx; chmod 000 bad_t.hx");
    rc = hxcreate("bad_t.hx", 0755, 256, "ch", 2);
    ok(rc == HXERR_CREATE, "hxcreate reports failure for unwritable target file: %s", hxerror(rc));

    rc = hxcreate("basic_t.hx", 0755, 0, "ch", 2);
    ok(rc == HXOKAY, "created basic_t.hx with default pgsize: %s", hxerror(rc));

    ok(!stat("basic_t.hx", &sb), "file created: %s", strerror(errno));
    // blksize: FreeBSD => uint32_t; Linux => long int.
    ok(sb.st_size == 2*sb.st_blksize, "default blocksize (%d) used", (int)sb.st_blksize);

    rc = hxcreate("basic_t.hx", 0755, 256, "ch", 2);
    ok(rc == HXOKAY, "created basic_t.hx: %s", hxerror(rc));

    hp = hxopen("basic_t.hx", HX_READ);
    ok(hp, "opened basic_t.hx: %s", strerror(errno));

    memset(buf, ' ', sizeof(buf));
    memcpy(buf, "a:12", 4);

    rc = hxget(hp, NULL, 0);
    ok(rc == HXERR_BAD_REQUEST, "hxget(NULL) returns %s", hxerror(rc));

    hxdebug = 2;    // for coverage
    len = hxget(hp, buf, 1);
    ok(len == 0, "hxget on empty file returns no record: %s (%d)", hxerror(rc), rc);
    rc = hxput(hp, buf, 4);
    ok(rc == HXERR_BAD_REQUEST, "hxput rejected update of read-only HXFILE: %s", hxerror(rc));
    hxdebug = 0;

    hxclose(hp);

    hxcreate("basic_t.hx", 0755, 64, "ch", 2);
    hp = hxopen("basic_t.hx", HX_UPDATE);

    // Repeat basic test of put+del with modes:
    //	(normal), MMAP, MPROTECT(!?), MPROTECT+MMAP, FSYNC
    for (mode = HX_UPDATE; mode <= HX_UPDATE+HX_FSYNC; mode += HX_MMAP) {

	fprintf(stderr, "# Testing with mode %s (%o)\n", hxmode(mode), mode);

	hxdebug = mode == HX_MMAP+HX_UPDATE;	// for coverage only
	hp = hxopen("basic_t.hx", mode);
	hxdebug = 0;
	ok(hp, "opened basic_t.hx for update: %s", strerror(errno));
        buf[1] = ':';
	rc = hxput(hp, buf, 4);
	ok(rc == 0, "hxput added record: %s", hxerror(rc));

	buf[2] = '!';
	rc = hxput(hp, buf, 4);
	ok(rc > 0, "hxput updated record: %s", hxerror(rc));
        buf[2] = '?';
	rc = hxget(hp, buf, 30);
	ok(rc == 4, "hxget finds key: %s", hxerror(rc));
        ok(buf[2] == '!', "hxget fetches record: %s", hxerror(rc));

        buf[1] = 'B';
	rc = hxput(hp, buf, 34);
	ok(rc == 0, "hxput adds record that fills page: %s", hxerror(rc));

        buf[2] = '#';
	rc = hxput(hp, buf, hxmaxrec(hp));
	ok(rc == 34, "hxput updates record that forces file to grow: %s", hxerror(rc));

        buf[2] = '?';
	rc = hxget(hp, buf, 64);
	ok(rc == hxmaxrec(hp), "hxget finds key: %s", hxerror(rc));
        ok(buf[2] == '#', "hxget fetches updated record");

	rc = hxput(hp, buf, hxmaxrec(hp) + 1);
	ok(rc == HXERR_BAD_REQUEST, "hxput rejected record too large for file: %s", hxerror(rc));

	rc = hxdel(hp, buf);
	ok(rc == hxmaxrec(hp), "first hxdel reported record deleted: %s (%d)", hxerror(rc), rc);

        buf[1] = ':';
	rc = hxdel(hp, buf);
	ok(rc == 4, "second hxdel reported record deleted: %s (%d)", hxerror(rc), rc);

        diag("set hxdebug=3 strictly for coverage");
        hxdebug = 3;
	rc = hxdel(hp, buf);
	ok(rc == 0, "redundant hxdel reported no record to delete: %s (%d)", hxerror(rc), rc);

	hxclose(hp);

	hp = hxopen("basic_t.hx", 0);
        rc = hxfix(hp,0,0,0,0);
        hxclose(hp);
        ok(rc == (HXRET)HX_UPDATE, "file checks okay");
	hxdebug = 0;
    }

    hxdebug = 1;
    fprintf(stderr, "# Next 2 tests must show Service unavailable (hxdebug)\n");
    char const *hxdir = getenv("hx");
    if (!hxdir) hxdir = ".";
    systemf("chmod -x %s/hx_badh.so", hxdir);
    hp = hxopen("bad_t.hx", HX_READ);
    ok(!hp, "cannot open file with unexecutable rectype: %s", strerror(errno));
    systemf("chmod +x %s/hx_badh.so", hxdir);
    systemf("mv %s/hx_badh.so %s/hx_badh.so.bak", hxdir, hxdir); 
    ok(!hp, "cannot open file with unexecutable rectype: %s", strerror(errno));
    systemf("mv -f %s/hx_badh.so.bak %s/hx_badh.so", hxdir, hxdir); 
    unsetenv("LD_LIBRARY_PATH");

    rc = hxcreate("basic_t.hx", 0666, 64, "nonesuch", 8);
    ok(rc == HXOKAY, "created file with unimplemented rectype: %s", hxerror(rc));

    hp = hxopen("basic_t.hx", HX_UPDATE);
    hxbind(hp, (HX_DIFF_FN)mydiff, (HX_HASH_FN)myhash, NULL, NULL, NULL);

    rc = hx_load(hp, buf, sizeof(buf), "hello\tworld");
    ok(rc == HXERR_BAD_REQUEST, "hx_load unimplemented returns: %s", hxerror(rc));

    hxbind(hp, (HX_DIFF_FN)mydiff, (HX_HASH_FN)myhash, (HX_LOAD_FN)myload, NULL, NULL);

    // "myload" is hardwired to return 2
    len = hx_load(hp, buf, sizeof(buf), "happy\tdays");
    ok(len == 2, "hx_load implemented returns: %s", hxerror(len));

    char    buf2[99];
    rc = hx_save(hp, buf, rc, buf2, sizeof(buf2));
    ok(rc == HXERR_BAD_REQUEST, "hx_save unimplemented returns: %s", hxerror(rc));
    
    hxbind(hp, (HX_DIFF_FN)mydiff, (HX_HASH_FN)myhash, (HX_LOAD_FN)myload, (HX_SAVE_FN)mysave, mytest);

    // "mysave" is hardwired to return 3
    rc = hx_save(hp, buf, len, buf2, sizeof(buf2));
    ok(rc == 3, "hx_save implemented returns: %s", hxerror(rc));

    hxclose(hp);

    return exit_status();
}
