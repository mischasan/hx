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

// chx: command-line for calling hx procs from shell.

#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include "_hx.h"
#include "util.h"

static FILE*	do_fopen(char const*cmd, char const*fname,
        		    char const*mode);
static HXFILE*	do_hxopen(char const*cmd, char const*name,
        		    int mode);
static void	is_hxret(char const*, HXRET);

static void	del(HXFILE*, FILE*);
static void	do_load(HXFILE*, FILE*);
static void     do_lock(HXFILE*);
static void	do_save(HXFILE*, FILE*);
static int	dump(HXFILE*, FILE*);
static void	help(void);
static int	hdrs(HXFILE*);
static void	info(HXFILE*);
static HXRET	maps(HXFILE*);
static void	stats(HXFILE*);
static void     types(char const *dirs);

static int	mmode   = 0;	// set to HX_MMAP by "-m".
static int	verbose = 0;

//--------------|---------------------------------------------
int
main(int argc, char **argv)
{
    int		ret = 0;
    int		opt;
    int         timed = 0;
    char	cmd[10240];

    while (0 < (opt = getopt(argc, argv, "?c:dmp:s:tv-"))) {
        switch (opt) {

        case '?': help();		    break;
        case 'c': hxcrash = atoi(optarg);   break;
        case 'd': ++hxdebug;		    break;
        case 'm': mmode |= HX_MMAP;	    break;
        case 's': mmode |= HX_FSYNC;	    break;
        case 't': timed++;                  break;
        case 'v': ++verbose;		    break;
        }
    }

    argc -= optind;
    argv += optind;

    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);
    errno = 0;	//setvbuf sets errno !?

    int		size	= 0;
    HXMODE	mode;
    HXFILE	*hp	= NULL;
    FILE	*fp	= NULL;
    
    if (!*argv)
        die("See 'chx help' for usage");
    if (*argv[0] == '?' || !strcmp(argv[0], "help"))
        help();

    double      tstart = tick();
    if (hxdebug) hxtime = tstart;
    if (!strcmp(argv[0], "build")) {

        hp = do_hxopen("build", argv[1], HX_UPDATE);
        fp = do_fopen("build", argv[2], "r");
        int	memsize = argc > 3 ? atoi(argv[3]) : 1;
        int	inpsize = argc > 4 ? atoi(argv[4]) : 0;
        memsize <<= 20;
        is_hxret("build", hxbuild(hp, fp, memsize, inpsize));

    } else if (!strcmp(argv[0], "check")) {
        char	*udata	= argc > 3 ? argv[3] : NULL;

        if (argc < 2)
            die("%s: requires filename [pgsize [udata]]", argv[0]);
        if (argc > 2 && !sscanf(argv[2], "%d", &size))
            die("%s: invalid pgsize", argv[2]);

        hp = do_hxopen("check", argv[1], HX_CHECK);
        mode = hxfix(hp, NULL, size, udata, udata ? strlen(udata) : 0);
        if (verbose || mode != HX_UPDATE)
            printf("%s %s\n", hxmode(mode),
        	   errno ? strerror(errno) : "");
        ret = mode != HX_UPDATE;

    } else if (!strcmp(argv[0], "create")) {

        if (argc < 3 || !sscanf(argv[2], "%d", &size))
            die("create: requires filename, pgsize, type");

        char const *type = argv[3] ? argv[3] : "";

        is_hxret("create", hxcreate(argv[1], 0644, size,
        			    type, strlen(type)));

        hp = do_hxopen("create", argv[1], HX_RECOVER);
        
    } else if (!strcmp(argv[0], "del")) {

        hp = do_hxopen("del", argv[1], HX_UPDATE);
        fp = do_fopen("del", argv[2], "r");
        del(hp, fp);

    } else if (!strcmp(argv[0], "dump")) {

        hp = do_hxopen("dump", argv[1], HX_READ);
        dump(hp, stdout);

    } else if (!strcmp(argv[0], "fix") || !strcmp(argv[0], "repair")) {
        char	*udata	= argc > 3 ? argv[3] : NULL;

        if (argc < 2)
            die("%s: requires filename [pgsize [udata]]", argv[0]);

        if (argv[2] && !sscanf(argv[2], "%d", &size))
            die("%s: invalid pgsize", argv[2]);

        hp = do_hxopen("repair", argv[1], HX_UPDATE);
        fp = tmpfile();
        mode = hxfix(hp, fp, size, udata, udata ? strlen(udata) : 0);
        if (verbose || mode != HX_UPDATE)
            printf("%s %s\n", hxmode(mode),
        	       errno ? strerror(errno) : "");
        ret = mode != HX_UPDATE;

    } else if (!strcmp(argv[0], "hdrs")) {

        hp = do_hxopen("hdrs", argv[1], HX_READ);
        fp = do_fopen("hdrs", argv[2], "w");
        hdrs(hp);

    } else if (!strcmp(argv[0], "info")) {

        hp = do_hxopen("info", argv[1], HX_READ);
        info(hp);

    } else if (!strcmp(argv[0], "load")) {

        hp = do_hxopen("load", argv[1], HX_UPDATE);
        fp = do_fopen("hdrs", argv[2], "r");
        do_load(hp, fp);

    } else if (!strcmp(argv[0], "lock")) {

        hp = do_hxopen("lock", argv[1], HX_READ);
        do_lock(hp);

    } else if (!strcmp(argv[0], "maps")) {

        hp = do_hxopen("maps", argv[1], HX_READ);
        is_hxret("maps", maps(hp));

    } else if (!strcmp(argv[0], "pack")) {

        hp = do_hxopen("load", argv[1], HX_UPDATE);
        is_hxret("pack", hxpack(hp));

    } else if (!strcmp(argv[0], "save")) {

        hp = do_hxopen("save", argv[1], HX_READ);
        fp = do_fopen("save", argv[2], "w");
        do_save(hp, fp);

    } else if (!strcmp(argv[0], "shape")) {
        double	density;
        
        if (argc != 3 || !sscanf(argv[2], "%lf", &density))
            die("%s: requires density arg (0 to 1.0)", argv[2]);

        hp = do_hxopen("shape", argv[1], HX_UPDATE);
        is_hxret("shape", hxshape(hp, density));

    } else if (!strcmp(argv[0], "stat")) {

        hp = do_hxopen("stat", argv[1], HX_READ);
        stats(hp);

    } else if (!strcmp(argv[0], "types")) {
        // for each dir in LD_LIBRARY_PATH (and "" -> ".") then /lib then /usr/lib,
        //  find all files of the form "<dir>/hx_<rectype>.so"
        //      and build a file of unique rectypes.
        // Then call hxlib which returns an exact path and a bitmask of DIFF/LOAD/TEST
        //  "type diff-load-test path"
        // If the path matches, print the entry.
        

        types(getenv("LD_LIBRARY_PATH"));
        types("lib:/usr/lib");
    } else {

        die("%s: unknown command. See 'chx help'", cmd);
    }

    if (fp) fclose(fp);
    if (hp) hxclose(hp);
    if (timed) fprintf(stderr, "# chx %s: %.3f secs\n", *argv, tick() - tstart);

    return  ret;
}

static void
del(HXFILE *hp, FILE *fp)
{
    int		keysize = hxmaxrec(hp) + 1;
    char	buf[keysize * 2], key[keysize];

    while (fgets(buf, keysize*2, fp)) {
        int	len = strlen(buf);

        if (buf[len - 1] == '\n')
            buf[--len] = 0;

        len = hx_load(hp, key, keysize, buf);
        if (len <= 0)
            continue;
        is_hxret("del", hxdel(hp, key));
    }
    if (fp) fclose(fp);
}

static FILE*
do_fopen(char const*cmd, char const*fname, char const*mode)
{
    if (!fname || !*fname)
        fname = "-";

    FILE    *fp = strcmp(fname,"-") ? fopen(fname, mode)
        	    : *mode == 'w' ? stdout : stdin;
    if (!fp)
        fprintf(stderr, "%s: %s: cannot open for %s\n",
        	cmd, fname, mode);
    return  fp;
}

static HXFILE *
do_hxopen(char const*cmd, char const*hxname, int mode)
{
    if (!hxname)
        die("%s: requires hxfile name", cmd);

    HXFILE  *hp = hxopen(hxname, mode | mmode);
    if (!hp) {
        fprintf(stderr, "%s: cannot open %s: %s\n",
                cmd, hxname, strerror(errno));
        exit(1);
    }

    return  hp;
}

static int
dump(HXFILE *hp, FILE *fp)
{
    HXLOCAL	loc;
#   define	locp (&loc)

    ENTER(locp, hp, NULL, 1);
    locp->mode = F_RDLCK;
    _hxlock(locp, 0, 0);
    _hxsize(locp);

    HXBUF	*bufp = &locp->buf[0];
    PAGENO	pg;

    int		uleng = hxinfo(hp, NULL, 0);
    char	udata[uleng];
    hxinfo(hp, udata, uleng);

    locp->mode	= F_RDLCK;
    for (pg = 0; pg < locp->npages; ++pg) {
        _hxload(locp, bufp, pg);
        _hxprbuf(locp, bufp, fp);
    }

    LEAVE(locp, 0);
#   undef	locp
}

static void
is_hxret(char const*cmd, HXRET ret)
{
    if (ret < 0) {
        fprintf(stderr, "%s failed: %s; %s\n", cmd,
        		hxerror(ret), strerror(errno));
        exit(2);
    }
}

static int
hdrs(HXFILE *hp)
{
    HXLOCAL	loc;
#   define	locp (&loc)
    PAGENO	pg;
    HXBUF	*bufp;

    ENTER(locp, hp, NULL, 1);
    locp->mode = F_RDLCK;
    _hxlock(locp, 0, 0);
    _hxsize(locp);

    bufp    = &locp->buf[0];

    printf("npages:%u dpages:%u mask:%u pgsize:%d"
            " udata[%d]:'%.*s'\n",
           locp->npages, locp->dpages, locp->mask,
           hp->pgsize, hp->uleng, hp->uleng, hp->udata);

    _hxload(&loc, bufp, 0);
    dx(stdout, bufp->data, ((loc.npages + 31) >> 5) + 15);

    locp->vprev = calloc(DATASIZE(locp->file) / MINRECSIZE,
        		 sizeof(PAGENO));
    for (pg = 1; pg < loc.npages; ++pg) {
        PAGENO	*pp = locp->vprev;

        _hxload(&loc, &loc.buf[0], pg);
        _hxfindHeads(locp, bufp);
        printf("%7u next=%u used=%d recs=%u heads:", (unsigned)pg,
        	(unsigned)bufp->next, bufp->used, bufp->recs);
        while (*pp)
            printf(" %u", *pp++);

        putchar('\n');
    }

    LEAVE(locp, 0);
#undef locp
}

static void
help()
{
    fputs(
    "Usage: chx [options] command arg...\n"
    "OPTIONS:\n"
    "\t-c N\tcore-dump after N write(2) calls\n"
    "\t-d\tenable hxdebug output (may be repeated)\n"
    "\t-m\tuse mmap file mode\n"
    "\t-s\tuse fsync file mode\n"
    "\t-t\treport elapsed time\n"
    "\t-v\tverbose diagnostics (may be repeated)\n"
    "COMMANDS:\n"
    "\tbuild  <hxfile> [text [memsize [inpsize]]] Populate hxfile from text\n"
    "\tcheck  <hxfile> [pgsize [type]]\n"
    "\tcreate <hxfile> pgsize [type]\n"
    "\tdel    <hxfile> [text]         Delete records for input keys\n"
    "\tdump   <hxfile>                Dump file dump (headers and data)\n"
    "\tfix    <hxfile> [pgsize [type]]\n"
    "\thdrs   <hxfile>                Dump internal page info\n"
    "\tinfo   <hxfile>                Show basic file info (size, recs, ...)\n"
    "\tload   <hxfile> [text]         Add/replace records from text file\n"
    "\tlock   <hxfile>                Retrieve info on file lock state\n"
    "\tmaps   <hxfile>                Dump freespace maps\n"
    "\tpack   <hxfile>                Pack file to minimal size\n"
    "\tsave   <hxfile> [text]         Print records as text (default to stdout)\n"
    "\tshape  <hxfile> density        Change file space/efficiency trade-off\n"
    "\tstat   <hxfile>                Show chain/share statisticss\n"
    "\ttypes                          List all accessible record type libraries\n"
"'density' is a float between 0 and 9E9. 0 is fast, 9E9 is miminum size.\n"
"\tTIt is the average number of overflow pages loaded\n"
"\ton an unsuccessful look-up.\n"
"'memsize' is in MB. 'pgsize' is a power of 2 between 32 and 32768.\n"
"'text' is a text file (or '-' for stdin) of save-format records.\n"
"'type' is a typename, implemented by hx_<type>.so\n"
    ,stdout);

    exit(0);
}

static void
info(HXFILE * hp)
{
    HXSTAT      st;

    hxstat(hp, &st);
    unsigned    dpages = _hxf2d(st.npages);
    unsigned    overs = st.npages - dpages - 1;
    if (!overs)
        overs = 1;

    printf( "recs=%.0f hash=%08X used=%.0f pages=%.0f ovfl_used=%.0f%%"
            " load=%.0f,%.0f%% pgsize=%d",
            st.nrecs, (unsigned)st.hash,
            st.head_bytes + st.ovfl_bytes,
            st.npages,
            st.ovfl_pages * 100 / overs,
            st.head_bytes * 100 / DATASIZE(hp) / dpages,
            st.ovfl_bytes * 100 / DATASIZE(hp) / st.ovfl_pages,
            hp->pgsize);

    if (hp->uleng)
        printf(" udata[%d]=%.*s", hp->uleng,
        	hp->uleng, hp->udata);
    putchar('\n');
}

static void
do_load(HXFILE *hp, FILE *inp)
{
    HXRET       hxret = 0;
    int         reclen, recsize = hxmaxrec(hp);
    int         bufsize = 2 * recsize;
    char        rec[recsize], buf[bufsize];
    int         lineno = 0, more, added = 0;

    if (verbose == 1) setvbuf(stderr, NULL, _IONBF, 0);
    while ((more = !!fgets(buf, bufsize, inp))) {
        ++lineno;
        char	*cp = buf + strlen(buf);
        if (cp > buf && cp[-1] == '\n')
            *--cp = 0;
        if (cp == buf)
            continue;

        reclen = hx_load(hp, rec, recsize, buf);
        if (reclen <= 0) {
            fprintf(stderr, "# load: invalid %s: %s\n",
        	    reclen == HXERR_BAD_REQUEST ? "request" : "input", buf);
            continue;
        }

        hxret = hxput(hp, rec, reclen);
        if (verbose > 1)
            fprintf(stderr, "# load(%s): %s\n",
        	    buf, hxerror(hxret));
        else if (verbose && lineno % 1000 == 0)
            putc('.', stderr);

        if (hxret < 0)
            break;
        if (hxret == HXNOTE)
            ++added;
    }
    if (verbose == 1 && lineno > 999) putc('\n', stderr);

    if (more)
        fprintf(stderr, "load: error at input line %d (%s)\n",
        	lineno, hxerror(hxret));

    if (verbose)
        fprintf(stderr, "load: in: %d added: %d\n",
        	    lineno, added);
}

static void
do_lock(HXFILE *hp)
{
    struct flock    what;
    what.l_type   = F_WRLCK;
    what.l_start  = 0;
    what.l_len    = 0;
    what.l_whence = SEEK_SET;
    what.l_pid    = 0;

    if (fcntl(hp->fileno, F_GETLK, &what)) {
        fprintf(stderr, "lock: GETLK failed: %s\n", strerror(errno));
    } else if (what.l_pid) {
        printf("%d blocked by pid=%d: %s page:%u count:%d\n",
                getpid(), what.l_pid,
                what.l_type == F_RDLCK ? "RDLCK" :
                what.l_type == F_WRLCK ? "WRLCK" : "??LCK",
                (PAGENO)(what.l_start / hp->pgsize),
                (COUNT)(what.l_len / hp->pgsize));
    }
}

static HXRET
maps(HXFILE *hp)
{
    HXLOCAL     loc;
    PAGENO	pg, last;
    int		bitpos;
#   define	locp	(&loc)
#   define	bufp	(&loc.buf[0])

    ENTER(locp, hp, NULL, 1);
    locp->mode	= F_RDLCK;
    _hxlock(locp, 0, 0);
    _hxsize(locp);

    last = locp->npages - 1;
    last = _hxmap(hp, last - last % HXPGRATE, &bitpos);
    printf("last: mpg=%u bit=%d\n", (unsigned)last, bitpos);

    for (pg = 0; pg <= last; ) {
        int	i, n;

        _hxload(locp, bufp, pg);
        n = pg == last ? (bitpos + 7) >> 3 : (int)DATASIZE(hp);

        printf("---- pgno=%u next=%u used=%d recs=%u", pg, bufp->next, bufp->used, bufp->recs);
        for (i = 0; i < n; ++i) {
            unsigned	b = bufp->data[i] & 0xFF;

            if (!(i & 15))
                printf("\n%7u", (unsigned)pg + i*8*HXPGRATE);

            if (i < bufp->used) fputs(" >>", stdout);
            else                printf(" %02X", b);
        }
        putchar('\n');

        pg += (DATASIZE(hp) - bufp->used) * 8 * HXPGRATE;
    }

    LEAVE(locp, 0);
#   undef locp
}

static void
do_save(HXFILE *hp, FILE *fp)
{
    int         leng, maxrec = hxmaxrec(hp);
    int         strsize = 2 * maxrec;
    char        rec[maxrec], str[strsize];

    while (0 < (leng = hxnext(hp, rec, maxrec))) {
        hx_save(hp, rec, leng, str, strsize);
        fprintf(fp, "%s\n", str);
    }
}

static void
stats(HXFILE *hp)
{
    HXSTAT	st;
    int		i, n;

    hxstat(hp, &st);
    printf("shape: %.2f", st.avg_fail_pages - 1);
    printf("  Chains:");
    for (n = HX_MAX_CHAIN; !st.chain_hist[n]; --n);
    for (i = 0; i <= n; ++i)
        printf(" %d", st.chain_hist[i]);

    printf("  Shares:");
    for (n = HX_MAX_SHARE; !st.share_hist[n]; --n);
    for (i = 0; i <= n; ++i)
        printf(" %d", st.share_hist[i]);

    putchar('\n');
}

static void
types(char const *dirs)
{
    if (!dirs) return;

    HXFILE  dummy;
    dummy.dlfile = 0;

    char    path[strlen(dirs) + sizeof("./hx_*.so")];
    do {
        int i, len = strcspn(dirs, ":");
        // "::" "" 
        if (len)
            memcpy(path, dirs, len), dirs += len;
        else
            path[0] = '.', len = 1;
        // At this point, *dirs == 0 || *dirs == ':'

        strcpy(path + len, "/hx_*.so");
        glob_t  globv;
        if (!glob(path, /*FLAGS*/0, /*ERRFN*/NULL, &globv)) {
            for (i = 0; i < (int)globv.gl_pathc; ++i) {
                char *libp = NULL;
                char *type = strrchr(globv.gl_pathv[i], '/') + 4; //3=strlen("/hx_")
                char *tend = type + strlen(type) - 3;
                char save = *tend; *tend = 0;
                int ops = hxlib(&dummy, type, &libp);
                *tend = save;
                if (ops && libp && !strcmp(libp, globv.gl_pathv[i])) {
                    *tend = 0;
                    printf("%s%s%s%s\n", libp,
                                ops & 1 ? "" : " -diff/hash",
                                ops & 2 ? "" : " -load",
                                ops & 4 ? "" : " -test");
                }
                free(libp);
            }
            globfree(&globv);
        }

        dirs += *dirs == ':';
    } while (*dirs);

    if (dummy.dlfile) dlclose(dummy.dlfile);
}
//EOF
