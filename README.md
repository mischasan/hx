hx
==

HXFILE hashed file system

Mischa Sandberg mischasan@gmail.com

HXFILE is an implementation of Litwin/Larson's linear-hash algorithm.

FEATURES
--------

- Highly concurrent. Simultaneous updates can happen through most of a file.
- Fast. On typical 2012 hardware, it takes less than 2 usec for a "get" or "set".
- Dynamic. Files grow incrementally; speed-vs-space file-shaping can be done concurrent with updates.
      Even when memory-mapped, files can grow or shrink.
- Extensible. Record-type methods are owned by the API user (a default type implementation is provided); 
     so generic API calls and commands can operate on different files at runtime.

DEFECTS
-------

The single serious issue is that, on Unix, it uses fcntl(2) advisory locking.
This means that one process can only have the file open once, so thread concurrency is not possible.

Originally, it was byte-order-independent. About ten years ago, Intel architecture 
became so dominant that I threw in the towel. HXFILE stores integers in LSB-first order.

DOCUMENTATION
-------------

The GoogleDocs description is at http://goo.gl/ItAcld
A presentation of how HXFILE implements linear-hash file growth is at http://goo.gl/OU9xDi
Sorry, HXFILE man-pages are in my TODO.

LICENSE
-------

Though I've had strong suggestions to go with BSD license, I'm going with GPL2 until I figure out
how to keep in touch with people who download and use the code. Hence the "CONTACT ME IF..." line in the license.

GETTING STARTED
---------------

Download the source, type "gmake".
"gmake install" exports bin/chx, include/hx.h, lib/libhx.a to $DESTDIR/.
(If you're interested in the GNUmakefile and rules.mk,
 check my blog posts on non-recursive make, at mischasan.wordpress.com.)

If you want to run test programs outside "make", be sure to set LD_LIBRARY_PATH
to include the directory containing hx_.so
 
See the file "hxample.c" for examples of record-API usage.

The standalone command "chx" can exercise the entire API, so "chx.c" is a useful reference
for the non-record-oriented API calls, like "hxshape" and "hxfix".

"hx_.c" is the default record-type implementation, which is used if you create a HXFILE without specifying a record type.

HISTORY
-------

I started HXFILE in 1991, for a sales-analysis OLAP DB running under MS-DOS.
It had to use no more than 32K for any operation; and the hard drives were slow (for 1991!) GriD Compass laptop drives.
That was used by several Fortune 500 companies. 

It has been ported from DOS to Windows 3.1, Solaris, AT&T Sys V.3, AIX, Windows NT, HP/UX, FreeBSD and Linux.

I removed the Win32 support five years ago, mostly because I was unimpressed with
Win32 memory-mapped files. Re-implementing some low-level routines would bring that back.

ACKNOWLEDGEMENTS
----------------

I'd like to thank Dr. Paul (Per-Ake) Larson, formerly at the University of Waterloo, for giving me the fundamentals 
for taking on this project.
