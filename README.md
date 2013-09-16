README
======

HXFILE: Dynamic Hash Files
Mischa Sandberg Oct 2009


What is HXFILE?
---------------

HXFILE is an implementation of linear hash-indexed files (Larson/Litwin's algorithm). HXFILE has the following features:

* Record types can be defined by the caller as virtual classes loaded from shared libs.
* There can be multiple concurrent updates (not just multiple updaters)
* The files are difficult to corrupt, and easy to repair.
* Files grow incrementally, as needed.
* A typical PC does about 1000 updates/sec; about 40,000 updates/sec for memory-mapped files; and bulk-load at about 10MB/sec.
* Relative to other hash file schemes, it is cache-friendly.
* The compiled library is under 60K.


Hash-indexed Files
------------------

If you know all about hash indexing already, skip this section.

Hash indexing is a way to find keyed items quickly in a collection. A hash function mangles the bytes
of a key to produce a pseudo-random integer. The integer is used as an array index, and poof, your key's
data is there in the array (or not). Hashing requires no extra space for complex look-up structures,
such as tree index schemes; and hashing ideally gets to your data in a single step.

Unfortunately, hash indexing is not really that simple, because multiple keys may hash to the same index.
There are many solutions to that problem, with varying trade-offs in time or space. The usual solution
involves storing a link (index) into an 'overflow' array, and following a chain of such links to test
each of the keys that hashed to the original index. The longer the chains, the slower the search.

Because of this, hash table implementations are notorious for their worst-case behaviours. To paraphrase
Longfellow, when hash tables are good, they are very, very good; and when they are bad, they are horrid.
Worse yet, for some implementations, the only way to add a record to a "full" hash table is to rebuild it
larger, from scratch.

Hash indexing records in a disk file has the same issues. For a huge file, the last thing you want is the
"insert one record" function to rebuild the entire file! Fortunately, there's a trick called "linear hashing"
that solves the general problem of incremental growth; the hash file can be extended one page at a time.

By itself, linear hashing doesn't solve the immediate problem of what to do when too many records hash to one
particular location in a file. It also doesn't help the general hash-file problem of cache unfriendliness.
HXFILE uses linear hashing, then solves both the remaining problems. It is a self-tuning balance between
hash-addressed pages and overflow pages, plus a scheme for sharing overflow pages. For files that fit mostly
into memory, or mostly on disk, HXFILE dominates non-hashed indexing for speed and space.


Record Types
------------
HXFILE treats records as opaque byte-blocks on which it can apply certain user-defined functions. The required
functions are hash and diff, which suffice for look-up by key. In addition, test, save and load functions,
if provided, are used to verify records and convert records to and from printable strings. The prototypes
for these functions are the typedefs HX_DIFF_FN, HX_HASH_FN etc. Whenever a HXFILE API routine calls such
a function, it also passes some const user data (see hxcreate) as a context argument.

There are two basic ways of associating the user-defined functions with a file handle:
specifying a record-type type, as a leading (null-terminated) string in the user data passed to hxcreate.
hxopen will try to dynamically load a shared library named "hx_type.so", searching $LD_LIBRARY_PATH, /usr/lib
and /lib for a library that exports functions diff, hash and optionally load, save and test. calling hxbind
to bind diff, hash, and optionally load, save and test function pointers to the HXFILE handle. This is the
normal case for a wrapper around hxopen for a static binding to a single record type. Such a scheme does
not support the generic chx utility program. 

If no such shared library is not found, and  hxbind is not called, any further hx API call returns HXERR_BAD_REQUEST. 

Requirements of the implementation functions
--------------------------------------------
* diff and hash must be able to process a record's key without knowing the record's total size.
* load must return 0 if the input string is invalid, the record length otherwise; but no more bytes than
  the given buffer size may be written to the record buffer. Load should reject a string containing
  a newline (\n).
* save must return the output string size (including trailing null) even if that exceeds the buffer size; but
  no more bytes than the buffer size may be written to the string buffer; and a trailing null is always
  written to the buffer. The string may not contain a newline.
* test  must return 0 if a record is invalid, a nonzero value otherwise. 

