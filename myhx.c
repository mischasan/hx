// myopen: skeleton for creating a statically-linked hxfile
//          record-type. Such files cannot be used with "chx".

#include <assert.h>
#include <string.h>
#include <hx.h>

HXFILE *myopen(char const *name, int mode);

static int
mydiff(char const *lp, char const *rp, char const *udata, int uleng)
{
    (void)lp, (void)rp, (void)udata, (void)uleng;
    int     ret = 0;            // keys match

    // DO SOMETHING
    return ret;
}

static  HXHASH
myhash(char const *recp, char const *udata, int uleng)
{
    (void)recp, (void)udata, (void)uleng;
    unsigned long ret = 0;      // 0 is invalid

    // DO SOMETHING
    return ret;
}

static int
myload(char *recp, int recsize, char const *buf, char const *udata, int uleng)
{
    (void)recp, (void)buf, (void)udata, (void)uleng;
    int     ret = 0;            // 0 => fail

    // DO SOMETHING
    assert(ret >= 0);
    assert(ret <= recsize);
    return ret;
}

static int
mysave(char const *recp, int reclen, char *buf, int bufsize,
       char const *udata, int uleng)
{
    (void)recp, (void)reclen, (void)udata, (void)uleng;
    assert(!buf || bufsize > 0);
    int     ret = 0;            // 0 => fail

    // DO SOMETHING
    assert(ret >= 0);
    assert(!buf || !(ret < bufsize ? buf[ret] : buf[bufsize - 1]));
    assert(!strchr(buf, '\n'));
    return ret;                 // 0 bytes written to buf
}

static int
mytest(char const *recp, int reclen, char const *udata, int uleng)
{
    (void)recp, (void)reclen, (void)udata, (void)uleng;
    assert(reclen > 0);
    int     ret = 0;            // => fail

    // DO SOMETHING
    return ! !ret;
}

HXFILE *
myopen(char const *name, int mode)
{
    HXFILE *hp = hxopen(name, mode, myhash, mydiff);

    hxbind(hp, mydiff, myhash, myload, mysave, mytest);
    return hp;
}
