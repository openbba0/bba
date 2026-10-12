/* $Id: snprintf.c,v 1.9 2008/01/20 14:02:00 holger Exp $ */

/*
 * Copyright (c) 1995 Patrick Powell.
 *
 * This code is based on code written by Patrick Powell <papowell@astart.com>.
 * It may be used for any purpose as long as this notice remains intact on all
 * source code distributions.
 */

/*
 * Copyright (c) 2008 Holger Weiss.
 *
 * This version of the code is maintained by Holger Weiss <holger@jhweiss.de>.
 * My changes to the code may freely be used, modified and/or redistributed for
 * any purpose.  It would be nice if additions and fixes to this file (including
 * trivial code cleanups) would be sent back in order to let me include them in
 * the version available at <http://www.jhweiss.de/software/snprintf.html>.
 * However, this is not a requirement for using or redistributing (possibly
 * modified) versions of this file, nor is leaving this notice intact mandatory.
 */

/*
 * History
 *
 * 2014-05-01 Ole André Vadla Ravnås <oleavr@nowsecure.com> for frida-gum:
 *
 *   - Use frida-gum's memory allocator
 *   - Drop locale support
 *   - Use GLib
 *   - Rely on modern toolchains
 *
 * 2008-01-20 Holger Weiss <holger@jhweiss.de> for C99-snprintf 1.1:
 *
 *   Fixed the detection of infinite floating point values on IRIX (and
 *   possibly other systems) and applied another few minor cleanups.
 *
 * 2008-01-06 Holger Weiss <holger@jhweiss.de> for C99-snprintf 1.0:
 *
 *   Added a lot of new features, fixed many bugs, and incorporated various
 *   improvements done by Andrew Tridgell <tridge@samba.org>, Russ Allbery
 *   <rra@stanford.edu>, Hrvoje Niksic <hniksic@xemacs.org>, Damien Miller
 *   <djm@mindrot.org>, and others for the Samba, INN, Wget, and OpenSSH
 *   projects.  The additions include: support the "e", "E", "g", "G", and
 *   "F" conversion specifiers (and use conversion style "f" or "F" for the
 *   still unsupported "a" and "A" specifiers); support the "hh", "ll", "j",
 *   "t", and "z" length modifiers; support the "#" flag and the (non-C99)
 *   "'" flag; use localeconv(3) (if available) to get both the current
 *   locale's decimal point character and the separator between groups of
 *   digits; fix the handling of various corner cases of field width and
 *   precision specifications; fix various floating point conversion bugs;
 *   handle infinite and NaN floating point values; don't attempt to write to
 *   the output buffer (which may be NULL) if a size of zero was specified;
 *   check for integer overflow of the field width, precision, and return
 *   values and during the floating point conversion; use the OUTCHAR() macro
 *   instead of a function for better performance; provide asprintf(3) and
 *   vasprintf(3) functions; add new test cases.  The replacement functions
 *   have been renamed to use an "rpl_" prefix, the function calls in the
 *   main project (and in this file) must be redefined accordingly for each
 *   replacement function which is needed (by using Autoconf or other means).
 *   Various other minor improvements have been applied and the coding style
 *   was cleaned up for consistency.
 *
 * 2007-07-23 Holger Weiss <holger@jhweiss.de> for Mutt 1.5.13:
 *
 *   C99 compliant snprintf(3) and vsnprintf(3) functions return the number
 *   of characters that would have been written to a sufficiently sized
 *   buffer (excluding the '\0').  The original code simply returned the
 *   length of the resulting output string, so that's been fixed.
 *
 * 1998-03-05 Michael Elkins <me@mutt.org> for Mutt 0.90.8:
 *
 *   The original code assumed that both snprintf(3) and vsnprintf(3) were
 *   missing.  Some systems only have snprintf(3) but not vsnprintf(3), so
 *   the code is now broken down under HAVE_SNPRINTF and HAVE_VSNPRINTF.
 *
 * 1998-01-27 Thomas Roessler <roessler@does-not-exist.org> for Mutt 0.89i:
 *
 *   The PGP code was using unsigned hexadecimal formats.  Unfortunately,
 *   unsigned formats simply didn't work.
 *
 * 1997-10-22 Brandon Long <blong@fiction.net> for Mutt 0.87.1:
 *
 *   Ok, added some minimal floating point support, which means this probably
 *   requires libm on most operating systems.  Don't yet support the exponent
 *   (e,E) and sigfig (g,G).  Also, fmtint() was pretty badly broken, it just
 *   wasn't being exercised in ways which showed it, so that's been fixed.
 *   Also, formatted the code to Mutt conventions, and removed dead code left
 *   over from the original.  Also, there is now a builtin-test, run with:
 *   gcc -DTEST_SNPRINTF -o snprintf snprintf.c -lm && ./snprintf
 *
 * 2996-09-15 Brandon Long <blong@fiction.net> for Mutt 0.43:
 *
 *   This was ugly.  It is still ugly.  I opted out of floating point
 *   numbers, but the formatter understands just about everything from the
 *   normal C string format, at least as far as I can tell from the Solaris
 *   2.5 printf(3S) man page.
 */

/*
 * ToDo
 *
 * - Add wide character support.
 * - Add support for "%a" and "%A" conversions.
 * - Create test routines which predefine the expected results.  Our test cases
 *   usually expose bugs in system implementations rather than in ours :-)
 */

#include "gumprintf.h"

#include "gummemory.h"

#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#if defined (HAVE_UNSIGNED_LONG_LONG_INT) || defined (_MSC_VER)
#define ULLONG unsigned long long int
#else
#define ULLONG unsigned long int
#endif

#if defined (HAVE_LONG_DOUBLE)
# define LDOUBLE long double
# define LDOUBLE_MIN_10_EXP LDBL_MIN_10_EXP
# define LDOUBLE_MAX_10_EXP LDBL_MAX_10_EXP
#else
# define LDOUBLE double
# define LDOUBLE_MIN_10_EXP DBL_MIN_10_EXP
# define LDOUBLE_MAX_10_EXP DBL_MAX_10_EXP
#endif

#if defined (HAVE_LONG_LONG_INT)
# define LLONG long long int
#else
# define LLONG long int
#endif

#ifndef ERANGE
# define ERANGE E2BIG
#endif
#ifndef EOVERFLOW
# define EOVERFLOW ERANGE
#endif

/*
 * Buffer size to hold the octal string representation of UINT128_MAX without
 * nul-termination ("3777777777777777777777777777777777777777777").
 */
#define MAX_CONVERT_LENGTH      43

/* Format read states. */
#define PRINT_S_DEFAULT         0
#define PRINT_S_FLAGS           1
#define PRINT_S_WIDTH           2
#define PRINT_S_DOT             3
#define PRINT_S_PRECISION       4
#define PRINT_S_MOD             5
#define PRINT_S_CONV            6

/* Format flags. */
#define PRINT_F_MINUS           (1 << 0)
#define PRINT_F_PLUS            (1 << 1)
#define PRINT_F_SPACE           (1 << 2)
#define PRINT_F_NUM             (1 << 3)
#define PRINT_F_ZERO            (1 << 4)
#define PRINT_F_QUOTE           (1 << 5)
#define PRINT_F_UP              (1 << 6)
#define PRINT_F_UNSIGNED        (1 << 7)
#define PRINT_F_TYPE_G          (1 << 8)
#define PRINT_F_TYPE_E          (1 << 9)

/* Conversion flags. */
#define PRINT_C_CHAR            1
#define PRINT_C_SHORT           2
#define PRINT_C_LONG            3
#define PRINT_C_LLONG           4
#define PRINT_C_LDOUBLE         5
#define PRINT_C_SIZE            6
#define PRINT_C_PTRDIFF         7
#define PRINT_C_INTMAX          8

#ifndef CHARTOINT
# define CHARTOINT(ch) (ch - '0')
#endif
#ifndef ISDIGIT
# define ISDIGIT(ch) ('0' <= (guchar) ch && (guchar) ch <= '9')
#endif
#ifndef ISNAN
# define ISNAN(x) (x != x)
#endif
#ifndef ISINF
# define ISINF(x) (x != 0.0 && x + x == x)
#endif

#define OUTCHAR(str, len, size, ch)                                            \
do                                                                             \
{                                                                              \
  if (len + 1 < size)                                                          \
    str[len] = ch;                                                             \
  (len)++;                                                                     \
}                                                                              \
while (/* CONSTCOND */ 0)


gint
gum_vsnprintf (gchar * str,
               gsize size,
               const gchar * format,
               va_list args)
{
  return g_vsnprintf (str, size, format, args);
}

gint
gum_vasprintf (gchar ** ret,
               const gchar * format,
               va_list ap)
{
  gsize size;
  gint len;
  va_list aq;

  va_copy (aq, ap);
  len = gum_vsnprintf (NULL, 0, format, aq);
  va_end (aq);
  if (len < 0 || (*ret = g_malloc (size = len + 1)) == NULL)
    return -1;

  return gum_vsnprintf (*ret, size, format, ap);
}

gint
gum_snprintf (gchar * str,
              gsize size,
              const gchar * format,
              ...)
{
  va_list ap;
  gint len;

  va_start (ap, format);
  len = gum_vsnprintf (str, size, format, ap);
  va_end (ap);

  return len;
}

gint
gum_asprintf (gchar ** ret,
              const gchar * format,
              ...)
{
  va_list ap;
  gint len;

  va_start (ap, format);
  len = gum_vasprintf (ret, format, ap);
  va_end (ap);

  return len;
}

