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

#include "tap.h"
#include "_hx.h"

static const int pgsize = 32;
static char errs[999];
static void get(int fd, int pos, char *buf, int len);
static void put(int fd, int pos, char const *buf, int len);
static void strerrs(void);

#if 0
Cases not yet covered:bad_map_head map
pg(other than root)
having(next || used)
#endif
    // Making "TEST" a function makes line numbers meaningless in "ok" output.
#define TEST(hp,mode,exp) \
    ok((rc = hxfix(hp,0,0,0,0))== mode, "hxcheck says file ready for %s", hxmode(rc));\
    strerrs();		\
    ok(!strcmp(errs, exp), "hxcheck finds:%s", errs);	\
    rc = hxfix(hp, fp = tmpfile(), pgsize, "ch", 2);	\
    fclose(fp);		\
    ok(rc == (HXRET)HX_UPDATE, "after hxfix, file ready for %s", hxmode(rc));
#define TODO
     int
             main(int argc, char **argv)
{
    (void)argc, (void)argv;
#ifdef TODO
    plan_tests(44);
#else
    plan_tests(47);
#endif
    // "ch" rectype uses the first byte as the hash and the first 2 bytes as key.
    hxcreate("corrupt_t.hx", 0777, pgsize, "ch", 2);
    HXFILE *hp = hxopen("corrupt_t.hx", HX_UPDATE | HX_REPAIR);
    int     rc, fd = hxfileno(hp);
    char    buf[pgsize];
    FILE   *fp;

    diag("=== set bit in map beyond last ovfl page");
    buf[0] = 1;
    put(fd, 31, buf, 1);
    TEST(hp, HX_READ, " bad_overmap:1");

    rc = hxput(hp, "A123456789", 10);
    ok(rc == 0, "added a record");
    rc = hxget(hp, strcpy(buf, "A1"), 0);
    ok(rc == 10, "hxget finds record of %d bytes", rc);
    ok(!strcmp(buf, "A1"), "buffer unchanged: (%s)", buf);

    short   used;

    get(fd, 1 * pgsize + sizeof(PAGENO), (char *)&used, sizeof used);
    ok(used == 16, "pg.used is %d", used);

    diag("=== corrupt pg.used: 16 -> 15");
    used = 15;
    put(fd, 1 * pgsize + sizeof(PAGENO), (char *)&used, sizeof used);

    TEST(hp, HX_REPAIR, " bad_recs:1 bad_index:1 bad_rec_size:1");
    rc = hxget(hp, buf, 0);
    ok(rc == 10, "record recovered: hxget returns %d", rc);

    diag("=== corrupt rec.leng: 10 -> 0");
    short   leng = 0;

    put(fd, 1 * pgsize + sizeof(HXPAGE) + sizeof(HXHASH), (char *)&leng,
        sizeof leng);

    TEST(hp, HX_REPAIR, " bad_recs:1 bad_index:1 bad_rec_size:1");
    rc = hxget(hp, buf, 0);
    ok(rc == 0, "no record recovery possible: hxget returned %d", rc);
    hxput(hp, "A123456789", 10);

    diag("=== corrupt rec.leng: 10 -> 24");
    leng = 24;
    put(fd, 1 * pgsize + sizeof(HXPAGE) + sizeof(HXHASH), (char *)&leng,
        sizeof leng);

    TEST(hp, HX_REPAIR, " bad_recs:1 bad_index:1 bad_rec_size:1");
    rc = hxget(hp, buf, 0);
    ok(rc == 0, "no record recovery possible: hxget returned %d", rc);
    hxput(hp, "A123456789", 10);

    diag("=== corrupt rec.leng: 10 -> 27");
    leng = 27;
    put(fd, 1 * pgsize + sizeof(HXPAGE) + sizeof(HXHASH), (char *)&leng,
        sizeof leng);
    TEST(hp, HX_REPAIR, " bad_recs:1 bad_index:1 bad_rec_size:1");
    rc = hxget(hp, buf, 0);
    ok(rc == 0, "no record recovery possible: hxget returned %d", rc);
    hxput(hp, "A123456789", 10);

    diag("=== corrupt rec.hash: 'A' -> 'B'");
    put(fd, 1 * pgsize + sizeof(HXPAGE), "B", 1);
    TEST(hp, HX_REPAIR, " bad_recs:1 bad_index:1 bad_rec_hash:1");
    rc = hxdel(hp, "A1");
    ok(rc == 10, "record recovered: hxdel(sic) returned %d", rc);

    todo_start("dup recs in page");
    diag("=== dup records in one page");
    hxput(hp, "A11", 3);
    hxput(hp, "A22", 3);
    // Change key of "A11" to "A21":
    put(fd, 1 * pgsize + sizeof(HXPAGE) + sizeof(HXREC), "A2", 2);
    TEST(hp, HX_READ, " crap");
    rc = hxget(hp, buf, sizeof buf);
    ok(rc == 3, "one record kept: hxget returned %d (%.*s)", rc, rc, buf);
    todo_end();

    rc = hxput(hp, "A3..:....|....:....|", hxmaxrec(hp));
    ok(rc == 0, "added maxrec (%d) record: %s", hxmaxrec(hp), hxerror(rc));

    diag("=== one-page loop");
    PAGENO  next = 4;

    put(fd, next * pgsize, (char *)&next, sizeof next);
    TEST(hp, HX_REPAIR, " bad_loop:1");

    diag("=== two-page loop");
    hxput(hp, "A4..:....|....:....|", hxmaxrec(hp));
    next = 8;
    put(fd, 4 * pgsize, (char *)&next, sizeof next);
    TEST(hp, HX_REPAIR, " bad_loop:1");

    diag("=== two-page dup recs");
    get(fd, 4 * pgsize + sizeof(HXPAGE), buf, pgsize - sizeof(HXPAGE));
    put(fd, 8 * pgsize + sizeof(HXPAGE), buf, pgsize - sizeof(HXPAGE));
    TEST(hp, HX_READ, " bad_dup_recs:1 bad_orphan:1");

    diag("=== bad head rec. This forces overused. If head has an overflow,\n"
         "#\tthat becomes an orphan, which forces bad_refs.");
    put(fd, 1 * pgsize + sizeof(HXPAGE), "B", 1);
    put(fd, 1 * pgsize + sizeof(HXPAGE) + sizeof(HXREC), "B", 1);
    TEST(hp, HX_REPAIR, " bad_index:1");
#ifndef TODO
    diag("=== bad free next:");
    used = 0;
    put(fd, 2 * pgsize + 4, (char *)&used, sizeof used);
    TEST(hp, HX_READ, " bad_dup_recs:1 bad_orphan:1");
#endif
    system("od -tx1 corrupt_t.hx | sed 's/^/# /'");

    hxclose(hp);
    return exit_status();
}

static void
get(int fd, int pos, char *buf, int len)
{
    lseek(fd, pos, 0);
    read(fd, buf, len);
}

static void
put(int fd, int pos, char const *buf, int len)
{
    lseek(fd, pos, 0);
    write(fd, buf, len);
}

static void
strerrs(void)
{
    char   *cp = errs;
    int     i;

    for (i = 0; i < NERRORS; ++i)
        if (hxcheck_errv[i]) {
            cp += sprintf(cp, " %s:%d", hxcheck_namev[i], hxcheck_errv[i]);
            hxcheck_errv[i] = 0;
        }
    *cp = 0;
}
