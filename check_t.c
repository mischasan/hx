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

int
main(void)
{
    HXFILE *hp;
    char    buf[99];
    int     ret, i;
    HXROOT  root = (HXROOT) { 2, 4, 0, 0 };

    plan_tests(42);

    setvbuf(stdout, 0, _IOLBF, 0);
    setvbuf(stderr, 0, _IOLBF, 0);

#   define  PGSIZE  256
    ok(!hxcreate("clean_t.hx", 0644, PGSIZE, "ch", 2), "hxcreate clean_t.hx");

    hp = hxopen("clean_t.hx", HX_UPDATE);
    if (!ok(hp, "hxopen clean_t.hx for update")) {
        plan_skip_all("could not create/open clean_t.hx");
        exit(exit_status());
    }

    memset(buf, '-', sizeof buf);
    buf[sizeof buf - 1] = ']';
    for (buf[0] = 'A'; buf[0] < 'S'; ++buf[0])
        if (0 != (ret = hxput(hp, buf, sizeof buf)))
            break;
    if (!ok(ret == 0, "%d records inserted", buf[0] - 'A'))
        fprintf(stderr, "hxput failed at %d: %s\n",
                buf[0] - 'A', hxerror(ret));

    hxclose(hp);

    //COVERAGE:
    hp = hxopen("clean_t.hx", HX_UPDATE);

    ret = hxfix(hp, (FILE *) 1, 0, NULL, 1);
    if (!ok
        (ret == HXERR_BAD_REQUEST,
         "hxfix rejects (uleng > 0, udata == NULL)"))
        fprintf(stderr, "hxfix returned %s\n", hxmode(ret));

    hxnext(hp, buf, sizeof buf);

    ret = hxfix(hp, (FILE *) 1, 0, NULL, 0);
    if (!ok
        (ret == HXERR_BAD_REQUEST,
         "hxfix(repair) during next is not allowed"))
        fprintf(stderr, "hxfix returned %s\n", hxmode(ret));

    hxclose(hp);

    hp = hxopen("clean_t.hx", HX_MMAP);
    ret = hxfix(hp, (FILE *) 1, 0, NULL, 0);
    if (!ok(ret == HXERR_BAD_REQUEST, "hxfix in MMAP mode is not allowed"))
        fprintf(stderr, "hxfix returned %s\n", hxmode(ret));
    hxclose(hp);

    system("cp clean_t.hx repair_t.hx");

    int     fd = open("repair_t.hx", O_RDWR, 0);

    root.pgsize = 32;
    root.version = hxversion;
    root.uleng = 1;
    lseek(fd, 0, 0L);
    write(fd, (char *)&root, sizeof root);
    write(fd, "?", 1);

    hp = hxopen("repair_t.hx", HX_REPAIR);
    ret = hxfix(hp, NULL, 0, "ch", 2);
    if (!ok(ret == HX_REPAIR, "hxfix detects bad udata"))
        fprintf(stderr, "hxfix returned %s\n", hxmode(ret));

    FILE   *fp = tmpfile();

    ret = hxfix(hp, fp, 0, "ch", 2);
    if (!ok(ret == (HXRET) HX_UPDATE, "hxfix repairs bad udata"))
        fprintf(stderr, "hxfix returned %s\n", hxmode(ret));
    fclose(fp);
    hxclose(hp);
    close(fd);

    system("cp clean_t.hx repair_t.hx");

    hp = hxopen("repair_t.hx", HX_REPAIR);
    char   *path;

    ret = hxlib(hp, "ch", &path);
    ok(ret == 7, "loaded ch from file %s: %d", path ? path : "?", ret);
    free(path);

    off_t   filesize = lseek(hxfileno(hp), 0L, SEEK_END);
    char    zeroes[PGSIZE * 2] = { };
    for (i = 9 * PGSIZE; i < filesize; i += PGSIZE) {
        lseek(hxfileno(hp), i, 0);
        write(hxfileno(hp), zeroes, 6);
    }

    fp = tmpfile();
    ok(fp, "created tempfile for recoverable records");
    ret = hxfix(hp, fp, PGSIZE, NULL, 0);
    if (!ok(ret == (HXRET) HX_UPDATE, "hxfix repaired zeroed page links"))
        fprintf(stderr, "hxfix returned: %s\n", hxmode(ret));

    fclose(fp);
    hxclose(hp);

    int     pos, step = PGSIZE / 5;

    for (pos = 0; pos < PGSIZE * 6; pos += step) {

        system("cp clean_t.hx repair_t.hx");
        fd = open("repair_t.hx", O_RDWR);
        lseek(fd, pos, 0);
        write(fd, zeroes, step * 2);
        close(fd);

        hp = hxopen("repair_t.hx", HX_REPAIR);
        fp = tmpfile();
        ret = hxfix(hp, fp, PGSIZE, "ch", 2);
        ok(ret == (HXRET) HX_UPDATE,
           "hxfix returned %s repairing zeroes at %d", hxmode(ret), pos);
        hxclose(hp);
        fclose(fp);
    }

    return exit_status();
}
