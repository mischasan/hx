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
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include "util.h"

char const *errname[] = {
    /*_00*/ "",              "EPERM",           "ENOENT",       "ESRCH",           "EINTR",
    /*_05*/ "EIO",           "ENXIO",           "E2BIG",        "ENOEXEC",         "EBADF",
    /*_10*/ "ECHILD",        "EAGAIN",          "ENOMEM",       "EACCES",          "EFAULT",
    /*_15*/ "ENOTBLK",       "EBUSY",           "EEXIST",       "EXDEV",           "ENODEV",
    /*_20*/ "ENOTDIR",       "EISDIR",          "EINVAL",       "ENFILE",          "EMFILE",
    /*_25*/ "ENOTTY",        "ETXTBSY",         "EFBIG",        "ENOSPC",          "ESPIPE",
#if   defined(__FreeBSD__)
    /*_30*/ "EROFS",         "EMLINK",          "EPIPE",        "EDOM",            "ERANGE",
    /*_35*/ "EDEADLK",       "EINPROGRESS",     "EALREADY",     "ENOTSOCK",        "EDESTADDRREQ",
    /*_40*/ "EMSGSIZE",      "EPROTOTYPE",      "ENOPROTOOPT",  "EPROTONOSUPPORT", "ESOCKTNOSUPPORT",
    /*_45*/ "EOPNOTSUPP",    "EPFNOSUPPORT",    "EAFNOSUPPORT", "EADDRINUSE",      "EADDRNOTAVAIL",
    /*_50*/ "ENETDOWN",      "ENETUNREACH",     "ENETRESET",    "ECONNABORTED",    "ECONNRESET",
    /*_55*/ "ENOBUFS",       "EISCONN",         "ENOTCONN",     "ESHUTDOWN",       "ETOOMANYREFS",
    /*_60*/ "ETIMEDOUT",     "ECONNREFUSED",    "ELOOP",        "ENAMETOOLONG",    "EHOSTDOWN",
    /*_65*/ "EHOSTUNREACH",  "ENOTEMPTY",       "EPROCLIM",     "EUSERS",          "EDQUOT",
    /*_70*/ "ESTALE",        "EREMOTE",         "EBADRPC",      "ERPCMISMATCH",    "EPROGUNAVAIL",
    /*_75*/ "EPROGMISMATCH", "EPROCUNAVAIL",    "ENOLCK",       "ENOSYS",          "EFTYPE",
    /*_80*/ "EAUTH",         "ENEEDAUTH",       "EIDRM",        "ENOMSG",          "EOVERFLOW",
    /*_85*/ "ECANCELED",     "EILSEQ",          "ENOATTR",      "EDOOFUS",         "EBADMSG",
    /*_90*/ "EMULTIHOP",     "ENOLINK",         "EPROTO"                           
#elif defined(__linux__)
    /*_30*/ "EROFS",         "EMLINK",          "EPIPE",        "EDOM",            "ERANGE",
    /*_35*/ "EDEADLK",       "ENAMETOOLONG",    "ENOLCK",       "ENOSYS",          "ENOTEMPTY",
    /*_40*/ "ELOOP",         "E041",            "ENOMSG",       "EIDRM",           "ECHRNG",
    /*_45*/ "EL2NSYNC",      "EL3HLT",          "EL3RST",       "ELNRNG",          "EUNATCH",
    /*_50*/ "ENOCSI",        "EL2HLT",          "EBADE",        "EBADR",           "EXFULL",
    /*_55*/ "ENOANO",        "EBADRQC",         "EBADSLT",      "E058",            "EBFONT",
    /*_60*/ "ENOSTR",        "ENODATA",         "ETIME",        "ENOSR",           "ENONET",
    /*_65*/ "ENOPKG",        "EREMOTE",         "ENOLINK",      "EADV",            "ESRMNT",
    /*_70*/ "ECOMM",         "EPROTO",          "EMULTIHOP",    "EDOTDOT",         "EBADMSG",
    /*_75*/ "EOVERFLOW",     "ENOTUNIQ",        "EBADFD",       "EREMCHG",         "ELIBACC",
    /*_80*/ "ELIBBAD",       "ELIBSCN",         "ELIBMAX",      "ELIBEXEC",        "EILSEQ",
    /*_85*/ "ERESTART",      "ESTRPIPE",        "EUSERS",       "ENOTSOCK",        "EDESTADDRREQ",
    /*_90*/ "EMSGSIZE",      "EPROTOTYPE",      "ENOPROTOOPT",  "EPROTONOSUPPORT", "ESOCKTNOSUPPORT",
    /*_95*/ "EOPNOTSUPP",    "EPFNOSUPPORT",    "EAFNOSUPPORT", "EADDRINUSE",      "EADDRNOTAVAIL",
    /*100*/ "ENETDOWN",      "ENETUNREACH",     "ENETRESET",    "ECONNABORTED",    "ECONNRESET",
    /*105*/ "ENOBUFS",       "EISCONN",         "ENOTCONN",     "ESHUTDOWN",       "ETOOMANYREFS",
    /*110*/ "ETIMEDOUT",     "ECONNREFUSED",    "EHOSTDOWN",    "EHOSTUNREACH",    "EALREADY",
    /*115*/ "EINPROGRESS",   "ESTALE",          "EUCLEAN",      "ENOTNAM",         "ENAVAIL",
    /*120*/ "EISNAM",        "EREMOTEIO",       "EDQUOT",       "ENOMEDIUM",       "EMEDIUMTYPE",
    /*125*/ "ECANCELED",     "ENOKEY",          "EKEYEXPIRED",  "EKEYREVOKED",     "EKEYREJECTED",
    /*130*/ "EOWNERDEAD",    "ENOTRECOVERABLE", "ERFKILL"                          
#endif
};

static char esc[256] = {
       0 , 0 , 0 , 0 , 0 , 0 , 0 ,'a','b','t','n','v','f','r', 0 , 0 ,
       0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
       0 , 0 ,'"', 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
       0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
       0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
       0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,'\\'
};

char *
acstr(char const *buf, int len)
{
    int        i, wid = 3;
    //  Trailing NULs are ignored.
    while (len > 0 && !buf[len-1]) --len;

    for (i = 0; i < len; ++i)
        wid += esc[(unsigned char)buf[i]] ? 2 : isgraph(buf[i]) ? 1 : 4;
    char        *ret = malloc(wid + 1), *cp = ret, ec;
    *cp++ = '"';
    for (i = 0; i < len; i++) {
        if ((ec = esc[(unsigned char)buf[i]]))
            *cp++ = '\\', *cp++ = ec;
        else if (isgraph(buf[i]))
            *cp++ = buf[i];
        else
            cp += sprintf(cp, "\\x%2.2X", (unsigned char)buf[i]);
    }

    *cp++ = '"';
    *cp = 0;
    return      ret;
}

void
dx(FILE *fp, char const *buf, int len)
{
    int	i, j;

    fprintf(fp, "* %d#", len);
    for (i = 0; i < len; i += 16) {
        fputs("\n#\t", fp);

        for (j = i; j < i + 16 && j < len; j++)
            fprintf(fp, " %2.2X", buf[j] & 0xFF);
        fprintf(fp, "%*s| ", 3 * (16 - j + i) + 1, "");

        for (j = i; j < i + 16 && j < len; j++)
            fputc(isprint(buf[j]) ? buf[j] : '.', fp);
    }

    fputc('\n', fp);
}

void
die(char const *fmt, ...)
{
    va_list	vargs;
    va_start(vargs, fmt);
    if (*fmt == ':') fputs(getprogname(), stderr);
    vfprintf(stderr, fmt, vargs);
    va_end(vargs);
    if (fmt[strlen(fmt)-1] == ':')
        fprintf(stderr, " %s %s", errname[errno], strerror(errno));
    putc('\n', stderr);
    _exit(1);
}

#if defined(__linux__)
char const *
getprogname(void)
{
    static char *progname;

    if (!progname) {
        char    buf[999];
        int     len;
        sprintf(buf, "/proc/%d/exe", getpid());
        len = readlink(buf, buf, sizeof(buf));
        if (len < 0 || len == sizeof(buf))
            return NULL;
        buf[len] = 0;
        char    *cp = strrchr(buf, '/');
        progname = strdup(cp ? cp + 1 : buf);
    }

    return  progname;
}
#endif

int
systemf(char const *fmt, ...)
{
    char	*cmd;
    va_list	vargs;
    va_start(vargs, fmt);
    int ret = vasprintf(&cmd, fmt, vargs);
    va_end(vargs);
    ret = system(cmd);
    free(cmd);
    return  ret;
}

double
tick(void)
{
    struct timeval t;
    gettimeofday(&t, 0);
    return t.tv_sec + 1E-6 * t.tv_usec;
}

void
usage(char const *str)
{
    fprintf(stderr, "Usage: %s %s\n", getprogname(), str);
    _exit(2);
}
