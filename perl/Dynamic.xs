#define PERL_NO_GET_CONTEXT
#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#ifdef __cplusplus
}
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <hx.h>

#undef AF_HAS_NOLOCK

#define EXPORT_OK(CONST, AS) do { \
    AV *exp = get_av("Hash::Dynamic::EXPORT_OK", 1); \
    av_push(exp, newSVpvn(#AS, sizeof(#AS)-1)); \
    newCONSTSUB(gv_stashpv("Hash::Dynamic", 1), #AS, newSViv(CONST)); \
} while (0)

/* record format: "key\0value\0" */
static unsigned long
keyhash(const char *recp, const char *data __unused, int datalen __unused)
{
    long    hash = 2166136261U;

    while (*recp) { hash = hash * 16777619 ^ *recp++; }
    return  hash;
}

MODULE = Hash::Dynamic	PACKAGE = Hash::Dynamic

PROTOTYPES: DISABLE

void
_open(self, filename, mode, pagesize, repair)
	SV* self
	char *filename
	int mode
	int pagesize
	int repair
    PREINIT:
	HV* hv;
	struct stat sb;
	HXFILE *file;
    CODE:
	if (stat(filename, &sb) != 0) {
	    if (HXOKAY != hxcreate(filename, 0666, pagesize, "", 0)) {
		croak("Error creating %s: %s\n", filename, strerror(errno));
	    }
	}

	if (repair) {
	    mode = mode == HX_READ ? HX_CHECK : HX_REPAIR;
	}

	file = hxopen(filename, mode, (HX_HASH_FN)keyhash,
                                        (HX_DIFF_FN)strcmp);
	if (!file) {
	    croak("Error opening %s: %s\n", filename, strerror(errno));
	}

	hv = (HV*)SvRV(self);
	hv_store(hv, "_i_hxfile", 9, newSViv((IV)file), 0);

void
_close(self)
	SV* self
    PREINIT:
	HV* hv;
	SV** svp = NULL;
    CODE:
	hv = (HV*)SvRV(self);
	if (hv && SvTYPE(hv) == SVt_PVHV)
	    svp = hv_fetch(hv, "_i_hxfile", 9, 0);
	if (svp && SvOK(*svp) && SvIOK(*svp))
	    hxclose( (HXFILE*) SvIV(*svp) );

char*
_get(self, key, hold)
	SV* self
	char *key
	int hold
    PREINIT:
	HV* hv;
	int size, len;
	char *buf;
	HXFILE *hp;
	SV** svp;
    CODE:
	hv = (HV*)SvRV(self);
	svp = hv_fetch(hv, "_i_hxfile", 9, 0);
	hp = (HXFILE*) SvIV(*svp);

	size = hxmaxrec(hp);
	if (strlen(key) >= size) {
	    warn("key is too large");
	    RETVAL = NULL;
	}

	buf = alloca(size);
	strcpy(buf, key);
	len = hxget(hp, buf, hold ? -size : size);
	if (len < 0)
	    croak("Error on hxget(%s): %d\n", key, len);
	else if (len == 0)
	    RETVAL = NULL;
	else
	    RETVAL = buf + strlen(buf) + 1;
    OUTPUT: 
	RETVAL
	    
void
put(self, key, value)
	SV* self
	char *key
	char *value
    PREINIT:
	HV* hv;
	int len;
	char *buf;
	SV** svp;
    CODE:
	hv = (HV*)SvRV(self);
	svp = hv_fetch(hv, "_i_hxfile", 9, 0);

	len = strlen(key) + 1 + strlen(value) + 1;
	buf = alloca(len);
	strcpy(buf, key);
	strcpy(buf + strlen(key) + 1, value);

	len = hxput((HXFILE*)SvIV(*svp), buf, len);
	if (len < 0)
	    croak("Error on hxput(%s): %d\n",key, len);

void
delete(self, key)
	SV* self
	char *key
    PREINIT:
	HV* hv;
	int len;
	char *buf;
	SV** svp;
    CODE:
	hv = (HV*)SvRV(self);
	svp = hv_fetch(hv, "_i_hxfile", 9, 0);

	len = hxdel((HXFILE*)SvIV(*svp), key);
	if (len < 0)
	    croak("Error deleting %s: %d\n", key, len);


void
nextpair(self)
	SV* self
    PREINIT:
	AV* av;
	HV* hv;
	HXFILE *hp;
	int len;
	int size;
	char *buf;
	SV** svp;
    PPCODE:
	hv = (HV*)SvRV(self);
	svp = hv_fetch(hv, "_i_hxfile", 9, 0);
	hp = (HXFILE*) SvIV(*svp);

	size = hxmaxrec(hp);
	buf = alloca(size);

	len = hxnext(hp, buf, size);
	if (len < 0)
	    croak("Error calling next: %d\n", len);
	else if (len == 0)
	    XSRETURN(0);

	EXTEND(SP, 2);
	PUSHs(sv_2mortal(newSVpv(buf, 0)));
	PUSHs(sv_2mortal(newSVpv(buf + strlen(buf) + 1, 0)));
	XSRETURN(2);

char*
nextsave(self)
	SV* self
    PREINIT:
	AV* av;
	HV* hv;
	HXFILE *hp;
	int len;
	int size;
	char *buf, *txt;
	SV** svp;
    PPCODE:
	hv = (HV*)SvRV(self);
	svp = hv_fetch(hv, "_i_hxfile", 9, 0);
	hp = (HXFILE*) SvIV(*svp);

	size = hxmaxrec(hp);
	buf = alloca(size);

	len = hxnext(hp, buf, size);
	if (len < 0)
	    croak("Error calling next: %d\n", len);
	else if (len == 0)
	    XSRETURN_UNDEF;
	txt = alloca(1024);
	if (hx_save(hp, buf, len, txt, 1024) > 0)
	    XPUSHs(sv_2mortal(newSVpv(txt, strlen(txt))));
	else
	    XSRETURN_UNDEF;

int
_check(self)
	SV*	self
    PREINIT:
	HV*	hv;
	SV**	svp;
	HXFILE	*hp;
	FILE	*fp;
    CODE:
	hv  = (HV*)SvRV(self);
	svp = hv_fetch(hv, "_i_hxfile", 9, 0);
	hp  = (HXFILE*) SvIV(*svp);
	fp  = tmpfile();
	RETVAL = hxcheck(hp, 0, 0, fp);
	fclose(fp);
    OUTPUT: 
	RETVAL
