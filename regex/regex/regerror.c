#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include "regex.h"

#include "utils.h"
#include "regerror.ih"

/*
 = #define	REG_OKAY	 0
 = #define	REG_NOMATCH	 1
 = #define	REG_BADPAT	 2
 = #define	REG_ECOLLATE	 3
 = #define	REG_ECTYPE	 4
 = #define	REG_EESCAPE	 5
 = #define	REG_ESUBREG	 6
 = #define	REG_EBRACK	 7
 = #define	REG_EPAREN	 8
 = #define	REG_EBRACE	 9
 = #define	REG_BADBR	10
 = #define	REG_ERANGE	11
 = #define	REG_ESPACE	12
 = #define	REG_BADRPT	13
 = #define	REG_EMPTY	14
 = #define	REG_ASSERT	15
 = #define	REG_INVARG	16
 = #define	REG_ATOI	255	// convert name to number (!)
 = #define	REG_ITOA	0400	// convert number to name (!)
 */
static struct rerr {
	int code;
	char *name;
	char *explain;
} rerrs[] = {
	REG_OKAY,	"REG_OKAY",	"no errors detected",
	REG_NOMATCH,	"REG_NOMATCH",	"regexec() failed to match",
	REG_BADPAT,	"REG_BADPAT",	"invalid regular expression",
	REG_ECOLLATE,	"REG_ECOLLATE",	"invalid collating element",
	REG_ECTYPE,	"REG_ECTYPE",	"invalid character class",
	REG_EESCAPE,	"REG_EESCAPE",	"trailing backslash (\\)",
	REG_ESUBREG,	"REG_ESUBREG",	"invalid backreference number",
	REG_EBRACK,	"REG_EBRACK",	"brackets ([ ]) not balanced",
	REG_EPAREN,	"REG_EPAREN",	"parentheses not balanced",
	REG_EBRACE,	"REG_EBRACE",	"braces not balanced",
	REG_BADBR,	"REG_BADBR",	"invalid repetition count(s)",
	REG_ERANGE,	"REG_ERANGE",	"invalid character range",
	REG_ESPACE,	"REG_ESPACE",	"out of memory",
	REG_BADRPT,	"REG_BADRPT",	"repetition-operator operand invalid",
	REG_EMPTY,	"REG_EMPTY",	"empty (sub)expression",
	REG_ASSERT,	"REG_ASSERT",	"\"can't happen\" -- you found a bug",
	REG_INVARG,	"REG_INVARG",	"invalid argument to regex routine",
	-1,		"",		"*** unknown regexp error code ***",
};

/*
 - regerror - the interface to error numbers
 = extern size_t regerror(int, const regex_t *, char *, size_t);
 */
/* ARGSUSED */
size_t
regerror(regerrcode, preg, errbuf, errbuf_size)
int regerrcode;
const regex_t *preg;
char *errbuf;
size_t errbuf_size;
{
	register struct rerr *r;
	register size_t len;
	register int target = regerrcode &~ REG_ITOA;
	register char *s;
	char convbuf[50];

	if (regerrcode == REG_ATOI)
		s = regatoi(preg, convbuf, 50);
	else {
		for (r = rerrs; r->code >= 0; r++)
			if (r->code == target)
				break;
	
		if (regerrcode&REG_ITOA) {
			if (r->code >= 0)
#if __STDC_WANT_SECURE_LIB__
				(void) strcpy_s(convbuf, 50, r->name);
#else
				(void) strcpy(convbuf, r->name);
#endif
			else
#if __STDC_WANT_SECURE_LIB__
				sprintf_s(convbuf, 50, "REG_0x%x", target);
#else
				sprintf(convbuf, "REG_0x%x", target);
#endif
			assert(strlen(convbuf) < sizeof(convbuf));
			s = convbuf;
		} else
			s = r->explain;
	}

	len = strlen(s) + 1;
	if (errbuf_size > 0) {
		if (errbuf_size > len)
#if __STDC_WANT_SECURE_LIB__
			(void) strcpy_s(errbuf, errbuf_size, s);
#else
			(void) strcpy(errbuf, s);
#endif
		else {
#if __STDC_WANT_SECURE_LIB__
			(void) strncpy_s(errbuf, errbuf_size, s, errbuf_size-1);
#else
			(void) strncpy(errbuf, s, errbuf_size-1);
#endif
			errbuf[errbuf_size-1] = '\0';
		}
	}

	return(len);
}

/*
 - regatoi - internal routine to implement REG_ATOI
 == static char *regatoi(const regex_t *preg, char *localbuf, size_t localsize);
 */
static char *
regatoi(preg, localbuf, localsize)
const regex_t *preg;
char *localbuf;
size_t localsize;
{
	register struct rerr *r;

	for (r = rerrs; r->code >= 0; r++)
		if (strcmp(r->name, preg->re_endp) == 0)
			break;
	if (r->code < 0)
		return("0");

#if __STDC_WANT_SECURE_LIB__
	sprintf_s(localbuf, localsize, "%d", r->code);
#else
	snprintf(localbuf, localsize, "%d", r->code);
#endif
	return(localbuf);
}
