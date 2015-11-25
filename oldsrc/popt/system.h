/**
 * \file popt/system.h
 */

#include "popt_hack.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined (__GLIBC__) && defined(__LCLINT__)
/*@-declundef@*/
/*@unchecked@*/
extern __const __int32_t *__ctype_tolower;
/*@unchecked@*/
extern __const __int32_t *__ctype_toupper;
/*@=declundef@*/
#endif

#include <ctype.h>

/* XXX isspace(3) has i18n encoding signednesss issues on Solaris. */
#define	_isspaceptr(_chp)	isspace((int)(*(unsigned char *)(_chp)))

#include <errno.h>
#include <fcntl.h>
#include <limits.h>

#ifdef HAVE_MCHECK_H
#include <mcheck.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(HAVE_UNISTD_H) && !defined(__LCLINT__)
#include <unistd.h>
#endif

#ifdef __NeXT
/* access macros are not declared in non posix mode in unistd.h -
 don't try to use posix on NeXTstep 3.3 ! */
#include <libc.h>
#endif

/*@-incondefs@*/
/*@mayexit@*/ /*@only@*/ /*@out@*/ /*@unused@*/
void * xmalloc (size_t size)
	/*@globals errno @*/
	/*@ensures maxSet(result) == (size - 1) @*/
	/*@modifies errno @*/;

/*@mayexit@*/ /*@only@*/ /*@unused@*/
void * xcalloc (size_t nmemb, size_t size)
	/*@ensures maxSet(result) == (nmemb - 1) @*/
	/*@*/;

/*@mayexit@*/ /*@only@*/ /*@unused@*/
void * xrealloc (/*@null@*/ /*@only@*/ void * ptr, size_t size)
	/*@ensures maxSet(result) == (size - 1) @*/
	/*@modifies *ptr @*/;

/*@mayexit@*/ /*@only@*/ /*@unused@*/
char * xstrdup (const char *str)
	/*@*/;
/*@=incondefs@*/

#if !defined(HAVE_STPCPY)
/* Copy SRC to DEST, returning the address of the terminating '\0' in DEST.  */
static inline char * stpcpy (char *dest, const char * src) {
    register char *d = dest;
    register const char *s = src;

    do
	*d++ = *s;
    while (*s++ != '\0');
    return d - 1;
}
#endif

/* Memory allocation via macro defs to get meaningful locations from mtrace() */
#if defined(HAVE_MCHECK_H) && defined(__GNUC__)
#define	vmefail()	(fprintf(stderr, "virtual memory exhausted.\n"), exit(EXIT_FAILURE), NULL)
#define	xmalloc(_size) 		(malloc(_size) ? : vmefail())
#define	xcalloc(_nmemb, _size)	(calloc((_nmemb), (_size)) ? : vmefail())
#define	xrealloc(_ptr, _size)	(realloc((_ptr), (_size)) ? : vmefail())
#define xstrdup(_str)   (strcpy((malloc(strlen(_str)+1) ? : vmefail()), (_str)))
#else
#define	xmalloc(_size) 		malloc(_size)
#define	xcalloc(_nmemb, _size)	calloc((_nmemb), (_size))
#define	xrealloc(_ptr, _size)	realloc((_ptr), (_size))
#define	xstrdup(_str)	strdup(_str)
#endif  /* defined(HAVE_MCHECK_H) && defined(__GNUC__) */

#if !defined(__LCLINT__)
#if defined(HAVE_SECURE_GETENV)
#define	getenv(_s)	secure_getenv(_s)
#elif defined(HAVE___SECURE_GETENV)
#define	getenv(_s)	__secure_getenv(_s)
#endif
#endif

#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x) 
#endif
#define UNUSED(x) x __attribute__((__unused__))

#include "popt.h"
