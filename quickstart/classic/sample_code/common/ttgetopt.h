/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

#ifndef TTC_GETOPTIONS_H_INCLUDED
#define TTC_GETOPTIONS_H_INCLUDED

/**
 * structures
 */

#ifndef TTC_DllExport
#define TTC_DllExport
#endif

#ifndef TTC_DllImport
#define TTC_DllImport
#endif

#include <stdarg.h>

/*
 * Options sent to ttc_getoptions.
 * They are a bitmask, so each value should be a power of two.
 */

/** TTC_OPT_NONE means accept the defaults. */
#define TTC_OPT_NONE 0

/**
 * If TTC_OPT_ABBREV is specified, options can be abbreviated
 * to the point of uniqueness.  For instance, "-n", "-nu",
 * "-num", etc. match a specification of "number=i", unless there
 * is some other specification that begins with "n", "nu", or "num".
 *
 * NOTE: abbreviations are not used for negate-able options, or for
 * special options like \<HELP\>.
 */
#define TTC_OPT_ABBREV 1

/**
 * Normally matches are not case sensitive for multi-character options
 * (they are for single character options).  TTC_OPT_CASE_SENSITIVE
 * makes all comparisons case sensitive.
 */
#define TTC_OPT_CASE_SENSITIVE 2

/**
 * If TTC_OPT_IGNORE_UNKNOWNS is given, unknown options will
 * just be skipped over, and no error generated.
 * However, there are a couple of potential problems.
 * First, the index returned is no longer very useful as an
 * index into the argv array.  The functions do not strip off
 * the known options and leave others behind.  Also, if
 * the unknown option has an option-argument, that will usually
 * cause the functions to think the options have ended.
 */
#define TTC_OPT_IGNORE_UNKNOWNS 4

/**
 * Normally \<HELP\> and \<VERSION\> exit after printing their messages.
 * With TTC_OPT_NO_EXIT, they do not exit.
 */
#define TTC_OPT_NO_EXIT 8

/**
 * If TTC_OPT_PRINT_ERRS is given, error messages are printed
 * out to stderr (normally they are just put into the error
 * message buffer).
 */
#define TTC_OPT_PRINT_ERRS 16

/**
 * Normally an error will be given if \<DSN\> or \<CONNSTR\> is specified,
 * but no dsn or connection string were found in the argv list.  With
 * TTC_OPT_NO_CONNSTR_OK, no error is flagged.
 */
#define TTC_OPT_NO_CONNSTR_OK 32

/**
 * Normally an error will be given if a =S option is specified, and the
 * option was given without any arguments.  With TTC_OPT_EMPTY_LIST_OK,
 * no error is signalled.
 */
#define TTC_OPT_EMPTY_LIST_OK 64

/**
 * Normally if \<DSN\> or \<CONNSTR\> is specified, but no dsn or
 * connection string were found, the environment variable TT_CONNSTR
 * is considered to see if it has a valid dsn. With TTC_OPT_NO_CONNSTR_ENV,
 * the environment variable is ignored.
 */
#define TTC_OPT_IGNORE_CONNSTR_ENV 128

/**
 * Mask of TTC_OPT_ constants.
 */
typedef int Ttc_getopt_options;

/*
 * Example:
 *   int main(int argc, char * const argv[])
 *   {
 *     char errmsg[256];
 *     ac = ttc_getoptions(argc, argv,
 *                       TTC_OPT_NONE,
 *                       errmsg, sizeof(errmsg),
 *                       "v",         &verbose,
 *                       "n=i",       &n1,
 *                       "<INTEGER>", &n2,
 *                       "dbl=d",     &d1,
 *                       "<REAL>",    &d2,
 *                       "uval=u",    &u,
 *                       "s=s",       &s, S_SIZE,
 *                       "H=p",       printInfo,
 *                       "<VERSION>", "6.6Beta6",
 *                       "<HELP>",    usage String,
 *                       NULL);
 *     if (ac == -1) { ... handle error ... }
 *     for (; ac < argc ++ac) { ... handle argv[ac] ... }
 *     ...
 *   }
 */

/*
 * ttc_getoptions -
 *   Parse the given argv array using the given list
 *   of option specifiers.
 */
extern TTC_DllImport
int
ttc_getoptions(int argc,                   /* # args in argv */
               char * const argv[],        /* argument array */
               Ttc_getopt_options options, /* bitwise-OR of TTOPT values */
               char* errmsgbuf,            /* buffer for error messages */
               int errmsgbuflen,           /* size of errmsgbuf */
               ...                         /* NULL-terminated list of option
                                            * specifiers and pointers */
               );

/*
 * ttc_vgetoptions -
 *   Same as ttc_vgetoptions, but taking a va_list.
 */
extern TTC_DllImport
int
ttc_vgetoptions(int argc,                  /* # args in argv */
                char * const argv[],       /* argument array */
                Ttc_getopt_options options,  /* bitwise-OR of TTOPT values */
                char* errmsgbuf,           /* buffer for error messages */
                int errmsgbuflen,          /* size of errmsgbuf */
                va_list ap                 /* NULL-terminated va-list of option
                                            * specifiers and pointers */
                );

/*
 * ttc_filegetoptions -
 *   Same as ttc_getoptions, but arguments from a file instead of
 *   an argv list.  *pargc and *pargv are filled in with argc/argv
 *   values read from the file.
 */
extern TTC_DllImport
int
ttc_filegetoptions(const char* filename,       /* name of input file */
                   int* pargc,                 /* # args in argv (output) */
                   char*** pargv,              /* argument array (output) */
                   Ttc_getopt_options options, /* bitwise-OR of TTOPT values */
                   char* errmsgbuf,            /* buffer for error messages */
                   int errmsgbuflen,           /* size of errmsgbuf */
                   ...                         /* NULL-terminated list of option
                                                * specifiers and pointers */
                   );

/*
 * ttc_filegetoptions_free
 *   Free argv list obtained from ttc_filegetoptions.
 */
extern TTC_DllImport
void
ttc_filegetoptions_free(char** argv);


/*
 * ttc_strgetoptions -
 *   Same as ttc_getoptions, but arguments from a string instead of
 *   an argv list.  *pargc and *pargv are filled in with argc/argv
 *   values read from the file.  One way this can be used is to
 *   initialize an argv list from an environment variable (as in
 *   ttIsql).
 */
extern TTC_DllImport
int
ttc_strgetoptions(const char* str,            /* input string */
                  const char* name,           /* name used as argv[0] */
                  int* pargc,                 /* # args in argv (output) */
                  char*** pargv,              /* argument array (output) */
                  Ttc_getopt_options options, /* bitwise-OR of TTOPT values */
                  char* errmsgbuf,            /* buffer for error messages */
                  int errmsgbuflen,           /* size of errmsgbuf */
                  ...                         /* NULL-terminated list of option
                                               * specifiers and pointers */
                  );


/*
 * ttc_strgetoptions_free
 *   Free argv list obtained from ttc_strgetoptions.
 */
extern TTC_DllImport
void
ttc_strgetoptions_free(char** argv);


/*
 * ttc_cleanargv
 *   Looks for and hides password arguments in the command line.
 *   Note that this doesn't actually accomplish anything on many
 *   platforms, for example modern Solarises, HP-UX, and Windows.
 *   The intent is to make any passwords given on the command line
 *   not visible by users running ps.
 */
extern TTC_DllImport
void
ttc_cleanargv(int argc, char** argv, ...);


/*
 * ttc_dump_help
 *   Dump a usage message, replacing any occurrence of <CMD>
 *   with the given cmdname.
 */
extern TTC_DllImport
void
ttc_dump_help(FILE* fp, const char* cmdname, const char* msg);


/*
 * ttc_getcmdname
 *   Return argv[0] with any leading path information, and any
 *   trailing 'Cmd' or '.exe' stripped off.  Fills the specified
 *   buffer up to the specified length.
 */
extern TTC_DllImport
const char*
ttc_getcmdname(const char* argv0, char* buf, int buflen);


/*
 * ttc_get_version_name
 *   Return a consistent form of the version name,
 *   looking like 'TimesTen Release 5.6.7'.
 *   Fills the specified buffer up to the specified length.
 */
extern TTC_DllImport
const char*
ttc_get_version_name(char* buf, int buflen);


/*
 * Return a true value if the argument looks like a DSN.
 */
extern TTC_DllImport
int
ttc_opt_looks_like_dsn(const char* str);


/*
 * Return a true value if the argument looks like a connection string.
 */
extern TTC_DllImport
int
ttc_opt_looks_like_connstr(const char* str);

#endif /* TTC_GETOPTIONS_H_INCLUDED */
