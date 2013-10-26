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
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include "_hx.h"

static int _hxlib(HXFILE *, char const *path, char const *name, char **pathp);

//--------------|---------------------------------------------
HXFILE *
hxopen(char const *name, HXMODE mode)
{
    HXFILE *hp = 0;             // return value
    HXROOT  hd;                 // file header
    int     fd;
    COUNT   pgsize = 0, version = 0;
    off_t   uleng = 0, mlen = 0;
    char   *udata = NULL, *vp;
    int     fix = mode & HX_RECOVER;
    int     omode = (mode & HX_UPDATE) ? O_RDWR : O_RDONLY;

    if ((vp = getenv("HXDEBUG")))
        hxdebug = atoi(vp);
    if (!hxproc || !*hxproc)
        hxproc = getenv("HXPROC");
    if (!hxtime && (vp = getenv("HXTIME")) && !(hxtime = atoi(vp)))
        hxtime = time(0);
    errno = EINVAL;
    if (mode & ~(HX_REPAIR | HX_MMAP | HX_MPROTECT | HX_FSYNC))
        return NULL;
    errno = 0;

    // "fix" overrides hxopen returning NULL for conditions
    //  that hxfix() can repair:
    //  - invalid file size or uleng for a given pgsize.
    // Note hxopen must NOT call _hxload, which would always
    // lock the root page!
    fd = open(name, O_BINARY | omode);
    if (fd < 0)
        return NULL;

    if (sizeof hd == read(fd, (char *)&hd, sizeof hd)) {
        pgsize = LDUS(&hd.pgsize);
        //XXX:If version is a DIFFERENT valid version, do not attempt to fix.
        version = LDUS(&hd.version);
        uleng = LDUS(&hd.uleng);
    }

    if (TWIXT(pgsize, HX_MIN_PGSIZE, HX_MAX_PGSIZE)
        && !(pgsize & (pgsize - 1))
        && uleng < (int)(pgsize - sizeof(HXROOT))) {

        udata = malloc(uleng + 1);
        udata[uleng] = 0;
        mlen = uleng == read(fd, udata, uleng)
            ? lseek(fd, 0L, SEEK_END) : -1;
    }

    if (fix ||
        (OK_VERSION(version) && udata && mlen > 0 && mlen % pgsize == 0)) {

        hp = (HXFILE *) calloc(1, sizeof(HXFILE));
        hp->mode = mode;
        hp->fileno = fd;
        hp->pgsize = pgsize;
        hp->version = version;
        hp->uleng = uleng;
        hp->udata = udata;
        hp->tail.used = DATASIZE(hp);

        if (udata) {
            hp->udata[hp->uleng] = 0;
            if (!(hp->mode & HX_STATIC))
                hxlib(hp, hp->udata, 0);
        }

        hp->map1 = 8 * HXPGRATE * ROOT_SIZE(hp);
        errno = 0;
        return hp;
    }

    errno = EBADF;
    free(udata);
    return NULL;
}

void
hxclose(HXFILE * hp)
{
    if (hp) {
        if (hp->mmap)
            munmap(hp->mmap, hp->mlen);

        free(hp->udata);
        free(hp->buffer.page);
        close(hp->fileno);

        if (hp->dlfile)
            dlclose(hp->dlfile);

        free(hp);
    }
}

void
hxbind(HXFILE * hp, HX_DIFF_FN df, HX_HASH_FN hf,
       HX_LOAD_FN lf, HX_SAVE_FN sf, HX_TEST_FN tf)
{
    hp->diff = df;
    hp->hash = hf;
    hp->load = lf;
    hp->save = sf;
    hp->test = tf;
}

static const char stdldpath[] = "/lib:/usr/lib:/usr/local/lib";

int
hxlib(HXFILE * hp, char const *hxreclib, char **pathp)
{
    int     ret;
    char const *ep = getenv("LD_LIBRARY_PATH");

    if (!ep || !*ep || !(ret = _hxlib(hp, ep, hxreclib, pathp)))
        ret = _hxlib(hp, stdldpath, hxreclib, pathp);

    return ret;
}

static int
_hxlib(HXFILE * hp, char const *ep, char const *hxreclib, char **pathp)
{
    int     len;
    void   *dlp = 0;
    char    path[strlen(ep) + strlen(".") +
                 sizeof("/hx_.so") + strlen(hxreclib)];
    do {
        char const *cp = ep;

        ep = strchr(ep, ':');
        len = ep ? (ep - cp) : (int)strlen(cp);
        if (!len)
            len = 1, cp = ".";
        sprintf(path, "%.*s/hx_%s.so", len, cp, hxreclib);
        if (access(path, R_OK + X_OK))
            continue;

        dlp = dlopen(path, RTLD_LAZY);
        if (!dlp) {
            DEBUG("cannot open %s: %s", path, dlerror());
            errno = ENOEXEC;
            continue;
        }

        HX_DIFF_FN diffp = (HX_DIFF_FN) dlsym(dlp, "diff");
        HX_HASH_FN hashp = (HX_HASH_FN) dlsym(dlp, "hash");

        if (!diffp || !hashp) {
            dlclose(dlp);
            dlp = 0;
            DEBUG("cannot use %s:%s%s missing", path,
                  diffp ? "" : " diff", hashp ? "" : " hash");
            errno = ENOEXEC;
            continue;
        }

        hp->diff = diffp;
        hp->hash = hashp;

    } while (!dlp && ep && *ep++);

    if (dlp) {
        if (hp->dlfile)
            dlclose(hp->dlfile);
        hp->dlfile = dlp;
        hp->load = (HX_LOAD_FN) dlsym(dlp, "load");
        hp->save = (HX_SAVE_FN) dlsym(dlp, "save");
        hp->test = (HX_TEST_FN) dlsym(dlp, "test");
        errno = 0;
        if (pathp)
            *pathp = strdup(path);
    }

    return (hp->diff && hp->hash ? 1 : 0)
        + (hp->load && hp->save ? 2 : 0)
        + (hp->test ? 4 : 0);
}
