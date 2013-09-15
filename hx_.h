// Copyright(c)2001 Mischa Sandberg, Vancouver, Canada.
//  (604) 298-6710. mischa.sandberg@telus.net
// hx_.h: interface definitions for record-type library implementers.

#ifndef HX__H
#define HX__H
#include "hx.h"

int     diff(char const*, char const*, char const*, int);
HXHASH  hash(char const*, char const*, int);
int     load(char *, int recsize, char const *, char const*, int);
int     save(char const*, int reclen, char *, int bufsize, char const*, int);
int     test(char const*, int reclen, char const*, int);

#ifndef __unused
#   define __unused __attribute__((unused))
#endif
#endif//HX__H
