/*
 * Copyright (c) 1999, 2021, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "ttgetopt.h"
#include "tt_version.h"
#define TTC_VERSION_MAJOR1 TTMAJOR1
#define TTC_VERSION_MAJOR2 TTMAJOR2
#define TTC_VERSION_MAJOR3 TTMAJOR3
#define TTC_VERSION_PATCH TTPATCH
#define TTC_VERSION_PORTPATCH TTPORTPATCH

#ifdef _WIN32
# define WINDOWS 1
#endif

#ifdef WINDOWS
# include <windows.h>
# define strncasecmp _strnicmp
# define strcasecmp  _stricmp
# define snprintf    _snprintf
# define vsnprintf   _vsnprintf
#else
# include <unistd.h>
#endif

/* Info about options stored in list of the following structures. */
typedef struct _Option {
  char* option;    /* the option string */
  char* option_orig; /* the original option string (before lower-case) */
  int   argType;   /* i for int, s for string, 0 for nothing, ... */
  void* argPtr;    /* where to store option values */
  int   argLen;    /* length of buffer or array */
  int   numArgs;   /* number in array so far */
  int*  numArgs_P; /* ptr to user's number in array so far */
  int   strlen;    /* length of opt */
  void* callback_arg;    /* option to be sent to the callback function */
  struct _Option *next;  /* ptr to next option in list */
} Option;

typedef struct {
  Option *head, *tail;        /* ptrs to first/last options */
  Ttc_getopt_options options;   /* mask of options */
  char* errmsgbuf;            /* error messages stored here */
  int errmsgbuflen;           /* size of errmsgbuf */
  const char* cmdname;        /* from argv[0] or filename */
  Option **sorted;            /* indexes sorted by option name,
                               *  normal options only */
  int n_sorted;               /* number of items in sorted */
  Option* dsn_opt;            /* set if see <DSN> */
  Option* connstr_opt;        /* set if see <CONNSTR> */
  int got_dsn;                /* set if got -dsn */
  int got_connstr;            /* set if got -connstr */
  int (*str_eq)(const char*,const char*);
  int (*str_cmp)(const char*,const char*);
} OptionList;


static int getOptions(const int argc, char* const argv[],
                      Ttc_getopt_options options,
                      char* errmsgbuf, int errmsgbuflen,
                      const char* cmdname,
                      va_list ap);

static void init_errmsg(char* errmsgbuf, int errmsgbuflen);
static int getArgsFromFile(const char *fileName,
                           int *pargc, char ***pargv,
                           char* errmsgbuf, int errmsgbuflen, int do_print); 
static int getArgsFromStr(const char *str, const char* name,
                          int *pargc, char ***pargv,
                          char* errmsgbuf, int errmsgbuflen, int do_print);
static int str_equal(const char* a, const char* b);
static int str_equal_ignore(const char* a, const char* b);
static int looks_like_dsn(const char* opt);
static int looks_like_connstr(const char* opt);







/****************************************************************/
/****************************************************************/
/**** Entry point functions.                                 ****/
/****************************************************************/
/****************************************************************/


/**
 * Extract the command name from argv0 (argv[0]),
 * removing any leading path components,
 * the leading underscore,
 * and any trailing 'Cmd' or '.exe'.
 *
 * @param argv0   argv[0]
 * @param buf     Destination buffer.
 * @param buflen  Size of destination buffer.
 *
 * @return pointer to buf.
 */
TTC_DllExport const char*
ttc_getcmdname(const char* argv0, char* buf, int buflen)
{
  const char* cmd;
  int len;
  char* trailer;
  int trailer_len;

  /* try to get the basename */
#ifdef WINDOWS
  cmd = strrchr(argv0, '\\');
  if (cmd == NULL) {
    cmd = strrchr(argv0, '/');
  }
#else
  cmd = strrchr(argv0, '/');
#endif
  if (cmd == NULL) {
    cmd = argv0;
  } else {
    ++cmd;
  }

  if (*cmd == '_')
    ++cmd;

  len = strlen(cmd);

  /* strip off trailing .exe or Cmd */
#ifdef WINDOWS
  trailer = ".exe";
#else
  trailer = "Cmd";
#endif
  trailer_len = strlen(trailer);
  if ( (len > trailer_len) &&
       str_equal_ignore(cmd+len-trailer_len, trailer) ) {
    if (buflen >= len-(trailer_len-1)) {
      strncpy(buf, cmd, len-trailer_len);
      buf[len-trailer_len] = '\0';
      cmd = buf;
    } else {
      cmd = argv0;
    }
  } else {
    if (buflen > len) {
      strcpy (buf, cmd);
      cmd = buf;
    } else {
      cmd = argv0;
    }
  }
  return cmd;
} /* ttc_getcmdname */


#ifndef PRODUCT_NAME
# define PRODUCT_NAME "TimesTen"
#endif

/**
 * Return a consistent form of the version name,
 * looking like 'TimesTen Release 5.6.7'.
 * Fills the specified buffer up to the specified length.
 *
 * @param buf     Destination buffer.
 * @param buflen  Size of destination buffer.
 *
 * @return pointer to buf.
 */
TTC_DllExport const char*
ttc_get_version_name(char* buf, int buflen)
{
  snprintf(buf, buflen,
               "%s Release %d.%d.%d.%d.%d",
               PRODUCT_NAME,
               TTC_VERSION_MAJOR1, TTC_VERSION_MAJOR2, TTC_VERSION_MAJOR3,
               TTC_VERSION_PATCH, TTC_VERSION_PORTPATCH);
  return buf;
} /* ttc_get_version_name */



/**
 * Parse the given argv list according to the given specifications.
 *
 * @param argc          Argument count.
 * @param argv          Argument list.
 * @param options       Mask of TTC_OPT_ values.
 * @param errmsgbuf     Buffer for error messages.
 * @param errmsgbuflen  Size of errmsgbuf.
 *
 * @return  Index of the next argument after the options arguments,
 *          or -1 for errors.
 *
 * @see  ttc_vgetoptions()
 * @see  ttc_filegetoptions()
 * @see  ttc_strgetoptions()
 */
TTC_DllExport int
ttc_getoptions(int argc, char* const argv[],
             Ttc_getopt_options options,
             char* errmsgbuf, int errmsgbuflen,
             ...)
{
  va_list ap;
  int n;
  char cmdbuf[80];
  const char* cmd = ttc_getcmdname(argv[0], cmdbuf, sizeof(cmdbuf));
  char* tthome;
  va_start(ap, errmsgbuflen);

  tthome = getenv("TIMESTEN_HOME");
  if (tthome == NULL) {
    strcpy(errmsgbuf, "TIMESTEN_HOME environment variable not set");
    return -1;
  }
  

  init_errmsg(errmsgbuf, errmsgbuflen);
  n = getOptions(argc, argv, options, errmsgbuf, errmsgbuflen, cmd, ap);
  va_end(ap);
  return n;
} /* ttc_getoptions */



/**
 * Parse the given argv list according to the given specifications.
 * This version takes a va_list for the specifications.
 *
 * @param argc          Argument count.
 * @param argv          Argument list.
 * @param options       Mask of TTC_OPT_ values.
 * @param errmsgbuf     Buffer for error messages.
 * @param errmsgbuflen  Size of errmsgbuf.
 * @param ap            va_list of specifications.
 *
 * @return  Index of the next argument after the options arguments,
 *          or -1 for errors.
 * @see ttc_getoptions()
 */
TTC_DllExport int
ttc_vgetoptions(int argc, char* const argv[],
              Ttc_getopt_options options,
              char* errmsgbuf, int errmsgbuflen,
              va_list ap)
{
  int n;
  char cmdbuf[80];
  const char* cmd = ttc_getcmdname(argv[0], cmdbuf, sizeof(cmdbuf));
  init_errmsg(errmsgbuf, errmsgbuflen);
  n = getOptions(argc, argv, options, errmsgbuf, errmsgbuflen, cmd, ap);
  return n;
} /* ttc_vgetoptions */


/**
 * Parse the given argv list according to the given specifications.
 * This version takes the argv list from a file,
 * for instance, ttendaemon.options.
 * The routine allocates an argv list, which the user
 * should maintain during argument handling.  The user
 * should call ttc_filegetoptions_free() when the argv list
 * is no longer needed.
 *
 * @param filename      Name of input file.  If NULL, stdin is used.
 * @param pargc         Argument count (output parameter).
 * @param pargv         Argument list (output parameter).
 * @param options       Mask of TTC_OPT_ values.
 * @param errmsgbuf     Buffer for error messages.
 * @param errmsgbuflen  Size of errmsgbuf.
 *
 * @return  Index of the next argument after the options arguments,
 *          or -1 for errors.
 * @see ttc_getoptions()
 * @see ttc_filegetoptions_free()
 */
TTC_DllExport int
ttc_filegetoptions(const char* filename,
                 int* pargc, char*** pargv,
                 Ttc_getopt_options options,
                 char* errmsgbuf, int errmsgbuflen,
                 ...)
{
  va_list ap;
  int n;
  int argc;
  char** argv;

  init_errmsg(errmsgbuf, errmsgbuflen);
  if (! getArgsFromFile(filename, &argc, &argv, errmsgbuf, errmsgbuflen,
                        options & TTC_OPT_PRINT_ERRS)) {
    return -1;
  }
  
  va_start(ap, errmsgbuflen);
  n = getOptions(argc, argv, options, errmsgbuf, errmsgbuflen,
                 filename ? filename : "<stdin>",
                 ap);
  va_end(ap);

  if (pargc != NULL) *pargc = argc;
  if (pargv == NULL) {
    /* free argv, don't need it anymore */
    ttc_filegetoptions_free(argv);
  } else {
    *pargv = argv;
  }

  return n;
} /* ttc_filegetoptions */


/**
 * Parse the given argv list according to the given specifications.
 * This version takes the argv list from a string,
 * for instance, ttendaemon.options.
 * The routine allocates an argv list, which the user
 * should maintain during argument handling.  The user
 * should call ttc_strgetoptions_free() when the argv list
 * is no longer needed.
 *
 * @param str           String containing options.
 * @param name          Name to print as cmd name in error msgs.
 * @param pargc         Argument count (output parameter).
 * @param pargv         Argument list (output parameter).
 * @param options       Mask of TTC_OPT_ values.
 * @param errmsgbuf     Buffer for error messages.
 * @param errmsgbuflen  Size of errmsgbuf.
 *
 * @return  Index of the next argument after the options arguments,
 *          or -1 for errors.
 * @see ttc_getoptions()
 * @see ttc_strgetoptions_free()
 */
TTC_DllExport int
ttc_strgetoptions(const char* str, const char* name,
                int* pargc, char*** pargv,
                Ttc_getopt_options options,
                char* errmsgbuf, int errmsgbuflen,
                ...)
{
  va_list ap;
  int n;
  int argc;
  char** argv;

  init_errmsg(errmsgbuf, errmsgbuflen);
  if (! getArgsFromStr(str, name, &argc, &argv, errmsgbuf, errmsgbuflen,
                       options & TTC_OPT_PRINT_ERRS)) {
    return -1;
  }

  va_start(ap, errmsgbuflen);
  n = getOptions(argc, argv, options, errmsgbuf, errmsgbuflen, name, ap);
  va_end(ap);

  if (pargc != NULL) *pargc = argc;
  if (pargv == NULL) {
    /* free argv, don't need it anymore */
    ttc_filegetoptions_free(argv);
  } else {
    *pargv = argv;
  }

  return n;
} /* ttc_strgetoptions */


/**
 * Free any storage allocated by ttc_filegetoptions().
 * The caller must be sure that they don't access
 * any of this storage again after this.
 *
 * @param argv   The argument list allocated by ttc_filegetoptions().
 * @see ttc_filegetoptions()
 */
TTC_DllExport void
ttc_filegetoptions_free(char** argv)
{
  if (argv != NULL) {
    char** av;
    for (av = argv; *av != NULL; ++av) {
      free((void*)*av);
    }
    free((void*)argv);
  }
} /* ttc_filegetoptions_free */


/**
 * Free any storage allocated by ttc_strgetoptions().
 * The caller must be sure that they don't access
 * any of this storage again after this.
 *
 * @param argv   The argument list allocated by ttc_strgetoptions().
 * @see ttc_strgetoptions()
 */
TTC_DllExport void
ttc_strgetoptions_free(char** argv)
{
  if (argv != NULL) {
    /* argv[0] string we copied */
    free((void*)argv[0]);
    /* also free the argv list itself */
    free((void*)argv);
  }
} /* ttc_strgetoptions_free */


/**
 * Return a true value if the argument looks like a DSN.
 *
 * @param str   String to check.
 */
TTC_DllExport int
ttc_opt_looks_like_dsn(const char* str)
{
  return looks_like_dsn(str);
} /* ttc_opt_looks_like_dsn */


/**
 * Return a true value if the argument looks like a connection string.
 *
 * @param str   String to check.
 */
TTC_DllExport int
ttc_opt_looks_like_connstr(const char* str)
{
  return looks_like_connstr(str);
} /* ttc_opt_looks_like_connstr */







/****************************************************************/
/****************************************************************/
/**** General utility routines.                              ****/
/****************************************************************/
/****************************************************************/

/* Initially clear the error message buffer. */
static void
init_errmsg(char* errmsgbuf, int errmsgbuflen)
{
  if ( (errmsgbuf != NULL) && (errmsgbuflen > 0) ) {
    *errmsgbuf = '\0';
  }
} /* init_errmsg */


/* Put error message into the given buffer. */
static void
pr_error(char* errmsgbuf, int errmsgbuflen, int do_print,
         const char* fmt, va_list ap)
{
  if (errmsgbuf != NULL) {
    int len;
    len = strlen(errmsgbuf);
    if (len > 0) {
      snprintf(errmsgbuf + len, errmsgbuflen - len, "\n");
      len = strlen(errmsgbuf);
    }
    vsnprintf(errmsgbuf + len, errmsgbuflen - len, fmt, ap);
    if (do_print) {
      fprintf(stderr, "%s", errmsgbuf + len); /* ap no longer valid at this point */
      fputs("\n", stderr);
    }
  }
  else {
    if (do_print) {
      vfprintf(stderr, fmt, ap);
      fputs("\n", stderr);
    }
  }
} /* pr_error */


/* Put error message into the given buffer. */
static void
msgbuf_error(char* errmsgbuf, int errmsgbuflen, int do_print,
             const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  pr_error(errmsgbuf, errmsgbuflen, do_print, fmt, ap);
  va_end(ap);
} /* msgbuf_error */


/*
 * Stick an error message into the buffer the user specified
 * for that purpose.  Return false so the function can be called
 * like:  return opt_error(ol, ...)
 */
static int
opt_error(OptionList* ol, const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  pr_error(ol->errmsgbuf, ol->errmsgbuflen, ol->options & TTC_OPT_PRINT_ERRS, fmt, ap);
  va_end(ap);
  return 0;
} /* opt_error */


/*
 * str_equal - return true value if both pointers are non-null
 * and they point to identical strings.
 */
static int
str_equal(const char* a, const char* b)
{
  return (a != NULL) && (b != NULL) && (strcmp(a,b) == 0);
} /* str_equal */


/*
 * str_equal_ignore - return true value if both pointers are non-null
 * and they point to identical strings, ignoring case.
 */
static int
str_equal_ignore(const char* a, const char* b)
{
  return (a != NULL) && (b != NULL) && (strcasecmp(a,b) == 0);
} /* str_equal_ignore */







/****************************************************************/
/****************************************************************/
/**** Option list functions.                                 ****/
/****************************************************************/
/****************************************************************/


/* Initialize the list of options. */
static void
init_option_list(OptionList *ol, Ttc_getopt_options options,
                 char* errmsgbuf, int errmsgbuflen,
                 const char* cmdname)
{
  ol->head = ol->tail = NULL;
  ol->options = options;
  ol->errmsgbuf = errmsgbuf;
  ol->errmsgbuflen = errmsgbuflen;
  ol->cmdname = cmdname;
  ol->sorted = NULL;
  ol->str_eq = (options & TTC_OPT_CASE_SENSITIVE) ? str_equal : str_equal_ignore;
  ol->str_cmp = (options & TTC_OPT_CASE_SENSITIVE) ? strcmp : strcasecmp;
  ol->dsn_opt = ol->connstr_opt = NULL;
  ol->got_dsn = ol->got_connstr = 0;
} /* init_option_list */


/* Make lower-case duplicate of string. */
static char*
str_lc_dup(const char* src)
{
  const char* p;
  char* q;
  char* dst = (char*)malloc(strlen(src)+1);
  if (dst == NULL) {
    return NULL;
  }
  p = src;
  q = dst;
  while (*p) {
    if (isupper(*p)) {
      *q++ = tolower(*p++);
    } else {
      *q++ = *p++;
    }
  }
  *q++ = '\0';
  return dst;
} /* str_lc_dup */


/*
 * Insert new option info into list.
 * Must be inserted in order, so precedence is kept straight.
 */
static int
add_option_to_list(OptionList*  ol,
                   char*        opt,
                   int          argType,
                   void*        arg,
                   int          len,
                   int*         num_P,
                   void*        callback_arg)
{
  Option* newOpt;

  for (newOpt = ol->head; newOpt != NULL; newOpt = newOpt->next) {
    if (str_equal(newOpt->option, opt)) {
      opt_error(ol, "%s: option '%s' is a duplicate",
                ol->cmdname, opt);
      free(opt);
      return 0;
    }
  }

  /*
   * MALLOC - it will be freed in free_opt_list.
   */
  newOpt = (Option*)malloc(sizeof(Option));
  if (newOpt == NULL) {
    return opt_error(ol, "%s: Out of memory allocating %d bytes.", ol->cmdname, sizeof(Option));
  }
  newOpt->option_orig  = opt;
  if ( (ol->options & TTC_OPT_CASE_SENSITIVE) || (strlen(opt) == 1) ) {
    newOpt->option = newOpt->option_orig;
  } else {
    newOpt->option  = str_lc_dup(opt);
    if (newOpt->option == NULL) {
      opt_error(ol, "%s: Out of memory copying %d bytes.",
                ol->cmdname, strlen(opt));
      free(newOpt); /* Parfait-detected memory leak */
      return 0;
    }
  }
  newOpt->argType = argType;
  newOpt->argPtr  = arg;
  newOpt->argLen  = len;
  newOpt->numArgs = 0;
  newOpt->numArgs_P = num_P;
  if (num_P != NULL) *num_P = 0;
  newOpt->callback_arg = callback_arg;
  newOpt->strlen  = strlen(opt);
  newOpt->next = NULL;
  if (ol->tail) {
    ol->tail->next = newOpt;
  }
  ol->tail = newOpt;
  if (ol->head == NULL) {
    ol->head = newOpt;
  }

  if (str_equal(opt, "<DSN>")) ol->dsn_opt = newOpt;
  else if (str_equal(opt, "<CONNSTR>")) ol->connstr_opt = newOpt;

  return 1;
} /* add_option_to_list */


/* Separate optionname/type and add to list. */
static int
add_option(OptionList*  ol,
           char*        opt,
           void*        arg,
           char*        sep,
           int          len,
           int*         num_P,
           void*        callback_arg)
{
  char  arg_type;

  if (sep == NULL) {
    if (str_equal(opt, "<DSN>") || str_equal(opt, "<CONNSTR>")) {
      /* implicit =s for DSN and CONNSTR */
      arg_type = 's';
    } else if (str_equal(opt, "<HELPFUNC>") || str_equal(opt, "<VERSFUNC>")) {
      /* implicit =x for HELPFUNC and VERSFUNC */
      arg_type = 'x';
    } else {
      int slen = strlen(opt);
      if (opt[slen-1] == '!') {
        arg_type = '!';
        opt[slen-1] = '\0';
      } else {
        arg_type = '\0';
      }
    }
  } else {
    arg_type = *(sep+1);
    *sep = '\0';
  }

  return add_option_to_list (ol, opt, arg_type, arg, len, num_P, callback_arg);
} /* add_option */


/* case-sensitive compare routine for qsort */
static int
sortcmp_sens(const void* a, const void* b)
{
  /* a and b are actually Option** */
  Option* oa = *(Option**)a;
  Option* ob = *(Option**)b;
  return strcmp(oa->option, ob->option);
} /* sortcmp_sens */


/*
 * Make sorted list for use when matching abbreviated options.
 */
static int
make_sorted(OptionList* ol)
{
  int n;
  Option* opt;

  /* see how many there are */
  for (opt = ol->head, n = 0; opt != NULL; opt = opt->next) {
    if (*(opt->option) != '<') {
      ++n;
    }
  }

  /* Initialize sorted array */
  ol->n_sorted = n;
  if (n == 0) {
    n = 1;  /* some platforms (aix) don't like malloc of 0 bytes */
  }
  ol->sorted = (Option**)malloc(n * sizeof(Option*));
  if (ol->sorted == NULL) {
    return opt_error(ol, "%s: Out of memory allocating %d*%d bytes.",
                     ol->cmdname, n, sizeof(Option*));
  }
  for (opt = ol->head, n = 0; opt != NULL; opt = opt->next) {
    if (*(opt->option) != '<') {
      ol->sorted[n++] = opt;
    }
  }

  /* sort them */
  qsort(ol->sorted, n, sizeof(Option*), sortcmp_sens);

  return 1;
} /* make_sorted */



/*
 * Add all given option specifiers to the options list.
 */
static int
add_options(OptionList* ol, va_list ap)
{
  for (;;) {
    char* next_opt; /* next option in the va_list */
    char* opt;      /* copy of next option */
    char* arg;      /* usually a ptr to variable */
    int len = 0;    /* for string type, length of buffer,
                     * for array type, length of array */
    char* sep;      /* location of the '=' */
    int*  num_P = NULL;  /* where to put number of array vals stored */
    void* callback_arg = NULL;    /* argument for some callbacks */

    next_opt = (char*)va_arg(ap, char*);
    if (next_opt == NULL) {
      /* reached the end */
      break;
    }

    /*
     * MALLOC
     * Some options may come from variables, others are string constants,
     * others might be stack local storage.
     * Since we don't know which it is, and we want to be able to clean
     * up everything later, we malloc a copy here.  It will be freed
     * in free_opt_list.
     */
    opt = strdup(next_opt);
    if (opt == NULL) {
      return opt_error(ol, "%s: Out of memory duping '%s'.", ol->cmdname, next_opt);
    }

    arg = (void*)va_arg(ap, void*);
    sep = strchr(opt, '=');
    if (sep != NULL) {
      char* p = sep + 1;
      /* string and array options take a length */
      if (str_equal(p, "I") || str_equal(p, "D") || str_equal(p, "s") || str_equal(p, "S") || str_equal(p, "z")) {
        len = (int)va_arg(ap, int);
      }
      /* array options take a ptr to a var to hold number of values */
      /* 'z' type also gets a pointer to an int argument */
      if (str_equal(p, "I") || str_equal(p, "D") || str_equal(p, "S") || str_equal(p, "z")) {
        num_P = (int*)va_arg(ap, int*);
      }
      /* 't' type gets a value to be passed along */
      if (str_equal(p, "t")) {
        callback_arg = (void*)va_arg(ap,  void*);
      }
    } else if (str_equal(opt, "<DSN>") || str_equal(opt, "<CONNSTR>")) {
      /* <DSN> and <CONNSTR> have an implicit =s */
      len = (int)va_arg(ap, int);
    }

    if (! add_option(ol, opt, arg, sep, len, num_P, callback_arg)) {
      return 0;
    }
  }

  if (! make_sorted(ol)) {
    return 0;
  }

  return 1;
} /* add_options */


/* Free storage for the option list. */
static void
free_opt_list(OptionList *ol)
{
  Option *o, *next;
  for (o=ol->head; o != NULL; o = next) {
    next = o->next;
    if (o->option) {
      if (o->option_orig != o->option) {
        free(o->option_orig);
      }
      free(o->option);
    }
    free((void*)o);
  }
  ol->head = ol->tail = NULL;
  if (ol->sorted != NULL) {
    free((void*)ol->sorted);
  }
} /* free_opt_list */


/*
 * Make sure it looked numeric (based on the strto* output),
 * and has no trailing junk.  Return true value if OK.
 */
static int
chk_numeric_arg(OptionList* ol, char* p, const char* optstr, const char* opt,
                int quiet)
{
#ifndef _WIN32
  if (errno == ERANGE) {
    if (! quiet) {
      opt_error(ol, "%s: option %s: numeric argument '%s' out of range\n",
                ol->cmdname, opt, optstr);
    }
    return 0;
  }
#endif /* not for Windows */

  if (p == optstr) {
    if (! quiet) {
      opt_error(ol, "%s: option %s: bad numeric argument '%s'\n",
                ol->cmdname, opt, optstr);
    }
    return 0;
  }

  if (*p != '\0') {
    if (! quiet) {
      opt_error(ol, "%s: option %s: trailing garbage in numeric argument '%s'\n",
                ol->cmdname, opt, optstr);
    }
    return 0;
  }

  return 1;
} /* chk_numeric_arg */


/*
 * Handle a numeric option.
 */
static int
get_numeric_arg(OptionList* ol, Option* o, const char* opt, int* idxP,
                int argc, char* const * argv, const char* optarg)
{
  const char* optstr;
  char* p;

  if ( (*idxP >= argc) && !optarg) {
    return opt_error(ol, "%s: option %s missing argument\n", ol->cmdname, opt);
  }
  optstr = optarg ? optarg : argv[*idxP];
  switch (o->argType) {
    case 'i':
      {
        errno = 0;
        int i = (int)strtol(optstr, &p, 0);
        if (! chk_numeric_arg(ol, p, optstr, opt, 0)) return 0;
        if (o->argPtr != NULL) {
          *(int*)o->argPtr = i;
        }
        break;
      }
    case 'u':
      {
        unsigned int u;
        errno = 0;
        u = (unsigned int)strtoul(optstr, &p, 0);
        if (! chk_numeric_arg(ol, p, optstr, opt, 0)) return 0;
        if (o->argPtr != NULL) {
          *(unsigned int*)o->argPtr = u;
        }
        break;
      }
    case 'l':
      {
        long i;
        errno = 0;
        i = strtol(optstr, &p, 0);
        if (! chk_numeric_arg(ol, p, optstr, opt, 0)) return 0;
        if (o->argPtr != NULL) {
          *(long*)o->argPtr = i;
        }
        break;
      }
    case 'd':
      {
        double d; 
        errno = 0;
        d = strtod(optstr, &p);
        if (! chk_numeric_arg(ol, p, optstr, opt, 0)) return 0;
        if (o->argPtr != NULL) {
          *(double*)o->argPtr = d;
        }
        break;
      }
    default:
      return opt_error(ol, "%s: bad numeric option type '%c'", ol->cmdname, o->argType);
  }

  if (optarg == NULL) {
    ++*idxP;
  }
  return 1;
} /* get_numeric_arg */


/*
 * Handle an int (=I) array option.
 */
static int
get_int_array_arg(OptionList* ol, Option* o, const char* opt, int* idxP,
                  int argc, char* const * argv)
{
  const char* optstr;
  char* p;
  int n_added = 0;
  int n;
  int rc;

  if (*idxP >= argc) {
    return opt_error(ol, "%s: option %s missing argument\n", ol->cmdname, opt);
  }

  for (;;) {
    if (*idxP >= argc) {
      /* at least one must have been found */
      rc = 1;
      break;
    }
    optstr = argv[*idxP];
    errno = 0;
    n = (int)strtol(optstr, &p, 0);
    if (! chk_numeric_arg(ol, p, optstr, opt, 1)) {
      /* If none have been found, it is an error. */
      if (n_added == 0) {
        opt_error(ol, "%s: No arguments to %s", ol->cmdname, o->option_orig);
        rc = 0;
      } else {
        rc = 1;
      }
      rc = (n_added > 0);
      break;
    }
    /* got another numeric arg, add it */
    if (o->argPtr != NULL) {
      int* arr;
      /*
       * o->argPtr  points to an array of ints,
       * o->argLen  is the size of the array
       * o->numArgs is the number there so far
       */
      if (o->numArgs >= o->argLen) {
        opt_error(ol, "%s: too many %s options specified",
                  ol->cmdname, o->option_orig);
        rc = 0;
        break;
      }
      arr = (int*)(o->argPtr);
      arr[o->numArgs] = n;
      ++o->numArgs;
      if (o->numArgs_P != NULL) *(o->numArgs_P) = o->numArgs;
    }
    ++*idxP;
    ++n_added;
  }

  return rc;
} /* get_int_array_arg */


/*
 * Handle a double (=D) array option.
 */
static int
get_dbl_array_arg(OptionList* ol, Option* o, const char* opt, int* idxP,
                  int argc, char* const * argv)
{
  const char* optstr;
  char* p;
  int n_added = 0;
  double n;
  int rc;

  if (*idxP >= argc) {
    return opt_error(ol, "%s: option %s missing argument\n", ol->cmdname, opt);
  }

  for (;;) {
    if (*idxP >= argc) {
      /* at least one must have been found */
      rc = 1;
      break;
    }
    optstr = argv[*idxP];
    errno = 0;
    n = (double)strtod(optstr, &p);
    if (! chk_numeric_arg(ol, p, optstr, opt, 1)) {
      /* If none have been found, it is an error. */
      if (n_added == 0) {
        opt_error(ol, "%s: No arguments to %s", ol->cmdname, o->option_orig);
        rc = 0;
      } else {
        rc = 1;
      }
      rc = (n_added > 0);
      break;
    }
    /* got another numeric arg, add it */
    if (o->argPtr != NULL) {
      double* arr;
      /*
       * o->argPtr  points to an array of ints,
       * o->argLen  is the size of the array
       * o->numArgs is the number there so far
       */
      if (o->numArgs >= o->argLen) {
        opt_error(ol, "%s: too many %s options specified",
                  ol->cmdname, o->option_orig);
        rc = 0;
        break;
      }
      arr = (double*)(o->argPtr);
      arr[o->numArgs] = n;
      ++o->numArgs;
      if (o->numArgs_P != NULL) *(o->numArgs_P) = o->numArgs;
    }
    ++*idxP;
    ++n_added;
  }

  return rc;
} /* get_dbl_array_arg */


/*
 * Handle an string array option (=S).
 * Mallocs space for the strings, it is up to the user
 * to free it.
 * Ends with first option that begins with a '-' (also '/'
 * for windows).
 */
static int
get_str_array_arg(OptionList* ol, Option* o, const char* opt, int* idxP,
                  int argc, char* const * argv)
{
  const char* optstr;
  int n_added = 0;
  int rc;
  int last_argc;

  if ( ((ol->dsn_opt != NULL) || (ol->connstr_opt != NULL)) &&
       !(ol->got_dsn || ol->got_connstr) ) {
    last_argc = argc-1;
  } else {
    last_argc = argc;
  }

  if (*idxP >= argc) {
    if (ol->options & TTC_OPT_EMPTY_LIST_OK) {
      return 1;
    } else {
      return opt_error(ol, "%s: option %s missing argument\n", ol->cmdname, opt);
    }
  }

  for (;;) {
    if (*idxP >= last_argc) {
      /* at least one must have been found */
      rc = 1;
      break;
    }
    optstr = argv[*idxP];
    if ( (*optstr == '-')
#if defined(WINDOWS)
         || (*optstr == '/')
#endif
                             ) {
      /* If none have been found, it is an error. */
      if ( (n_added == 0) && !(ol->options & TTC_OPT_EMPTY_LIST_OK) ) {
        opt_error(ol, "%s: No arguments to %s", ol->cmdname, o->option_orig);
        rc = 0;
      } else {
        rc = 1;
      }
      break;
    }

    /* got another string arg, add it */
    if (o->argPtr != NULL) {
      char** arr;
      /*
       * o->argPtr  points to an array of ints,
       * o->argLen  is the size of the array
       * o->numArgs is the number there so far
       */
      if (o->numArgs >= o->argLen) {
        opt_error(ol, "%s: too many %s options specified",
                  ol->cmdname, o->option_orig);
        rc = 0;
        break;
      }
      arr = (char**)(o->argPtr);
      arr[o->numArgs] = strdup(optstr);
      if (arr[o->numArgs] == NULL) {
        opt_error(ol, "%s: Out of memory, dup of %s.", ol->cmdname, optstr);
        rc = 0;
        break;
      }
      ++o->numArgs;
      if (o->numArgs_P != NULL) *(o->numArgs_P) = o->numArgs;
    }
    ++*idxP;
    ++n_added;
  }

  return rc;
} /* get_str_array_arg */


/*
 * Handle a string+int array option (=z).
 * Copy the value and increment the index.
 */
static int
get_str_int_arg(OptionList* ol, Option* o, const char* opt, int* idxP,
                  int argc, char* const * argv)
{
  int n;
  char* p;

  /* make sure the option arguments are there */
  if (*idxP+1 >= argc) {
    return opt_error(ol, "%s: option %s missing argument\n",
                     ol->cmdname, opt);
  }

  /* copy the string argument */
  if (o->argPtr != NULL) {
    /* make sure destination string is big enough */
    if (strlen(argv[*idxP]) >= (size_t)(o->argLen)) {
      return opt_error(ol, "%s: option %s is too long (max %d)\n",
                       ol->cmdname, opt, o->argLen);
    }

    /* copy it */
    strcpy((char*)o->argPtr, argv[*idxP]);
  }

  /* increment the index */
  ++*idxP;

  /* now the int argument */
  errno = 0;
  n = (int)strtol(argv[*idxP], &p, 0);
  if (! chk_numeric_arg(ol, p, argv[*idxP], opt, 0)) return 0;
  if (o->numArgs_P != NULL) {
    /* NOTE: numArgs_P overridden for the bogus 'z' type */
    *(int*)o->numArgs_P = n;
  }


  /* increment the index */
  ++*idxP;
  return 1;
} /* get_str_int_arg */


/*
 * Handle a string argument.
 * Copy the value and increment the index.
 */
static int
copy_str_arg(OptionList* ol, Option* o, const char* opt,
             int* idxP, int argc, char* const * argv, const char* optarg)
{
  /* make sure the option argument is there */
  if ( (*idxP >= argc) && (optarg == NULL) ) {
    return opt_error(ol, "%s: option %s missing argument\n",
                     ol->cmdname, opt);
  }

  if (o->argPtr != NULL) {
    /* make sure destination string is big enough */
    const char* optstr = optarg ? optarg : argv[*idxP];
    if (strlen(optstr) >= (size_t)(o->argLen)) {
      return opt_error(ol, "%s: option %s is too long (max %d)\n",
                       ol->cmdname, opt, o->argLen);
    }

    /* copy it and increment the index */
    strcpy((char*)o->argPtr, optstr);
  }

  if (optarg == NULL) {
    ++*idxP;
  }
  return 1;
} /* copy_str_arg */


/*
 * Handle a string argument with a callback function.
 * Copy the value and increment the index.
 */
static int
do_callback_str(OptionList* ol, Option* o, const char* opt,
                int* idxP, int argc, char* const * argv, const char* optarg)
{
  const char* optstr = optarg ? optarg : argv[*idxP];
  void (*callback_func)(const char*, const char*);

  /* make sure the option argument is there */
  if ( (*idxP >= argc) && (optarg != NULL) ) {
    return opt_error(ol, "%s: option %s missing argument\n",
                     ol->cmdname, opt);
  }

  if (o->argPtr != NULL) {
    callback_func = (void (*)(const char*, const char*))(o->argPtr);
    callback_func(opt, optstr);
  }

  if (optarg == NULL) {
    ++*idxP;
  }
  return 1;
} /* do_callback_str */


/*
 * Handle a string argument with a callback function
 * which gets an argument, returns a value, and can
 * set the error string.
 */
static int
do_callback_val(OptionList* ol, Option* o, const char* opt,
                int* idxP, int argc, char* const * argv, const char* optarg)
{
  const char* optstr = optarg ? optarg : argv[*idxP];
  int (*callback_func)(const char*, const char*, void*, const char*, size_t);
  int rc = 1;
  char errbuf[256];

  /* make sure the option argument is there */
  if ( (*idxP >= argc) && (optarg != NULL) ) {
    return opt_error(ol, "%s: option %s missing argument\n",
                     ol->cmdname, opt);
  }

  if (o->argPtr != NULL) {
    callback_func = (int (*)(const char*, const char*, void*, const char*, size_t))(o->argPtr);
    rc = callback_func(opt, optstr, o->callback_arg, errbuf, sizeof(errbuf));
    if (! rc) {
      opt_error(ol, "%s: %s", ol->cmdname, errbuf);
    }
  }

  if (optarg == NULL) {
    ++*idxP;
  }
  return rc;
} /* do_callback_val */



/*
 * Found a match between the current option and one
 * in the option list.
 * Return a true value if there are no errors.
 */
static int
handle_option(OptionList* ol, Option* o, const char* opt, int* idxP,
              int argc, char* const * argv, int invert, const char* optarg)
{
  switch (o->argType) {
    case '\0':
      if (o->argPtr != NULL) {
        *(int*)o->argPtr = 1;
      }
      return 1;

    case '!':
      if (o->argPtr != NULL) {
        *(int*)o->argPtr = invert ? 0 : 1;
      }
      return 1;

    case 'i':  /* int */
    case 'u':  /* unsigned int */
    case 'l':  /* long */
    case 'd':  /* double */
      return get_numeric_arg(ol, o, opt, idxP, argc, argv, optarg);

    case 'I':  /* array of ints */
      return get_int_array_arg(ol, o, opt, idxP, argc, argv);

    case 'D':  /* array of doubles */
      return get_dbl_array_arg(ol, o, opt, idxP, argc, argv);

    case 'S':  /* array of strings */
      return get_str_array_arg(ol, o, opt, idxP, argc, argv);

    case 's':  /* string */
      return copy_str_arg(ol, o, opt, idxP, argc, argv, optarg);

    case 'Z':  /* string with callback */
      return do_callback_str(ol, o, opt, idxP, argc, argv, optarg);

    case 't':  /* string with callback, returns value */
      return do_callback_val(ol, o, opt, idxP, argc, argv, optarg);

    case 'z':  /* string+int */
      return get_str_int_arg(ol, o, opt, idxP, argc, argv);

    default:
      break;
  }
  return opt_error(ol, "%s: unknown option type %c\n",
                   ol->cmdname, o->argType);
} /* handle_option */

/**
 * Print the help message, replacing any '\<CMD\>' in the string
 * with the command name.  Doesn't add a trailing newline.
 *
 * @param fp        Output file for help message.
 * @param cmdname   Command name to replace \<CMD\>.
 * @param msg       Help message.
 */
void
ttc_dump_help(FILE* fp, const char* cmdname, const char* msg)
{
#define HELP_CMD "<CMD>"
#define HELP_CMD_LEN 5
  const char* p = msg;

  while (*p) {
    char* q = strstr(p, HELP_CMD);
    if (q == NULL) {
      /* no <CMD>, just print the rest and leave */
      fputs(p, fp);
      break;
    } else {
      /* print the part up to the <CMD> */
      fwrite(p, q-p, 1, fp);
      /* print the command name */
      fputs(cmdname, fp);
      /* skip past the <CMD> */
      p = q + HELP_CMD_LEN;
    }
  }
} /* ttc_dump_help */



/*
 * Check current option vs the special options like <INTEGER>.
 * Return true if it matches one.
 */
static int
chk_special_option(OptionList* ol, Option* o, const char* opt, int* idxP,
                   int argc, char* const * argv)
{
  char* p;
  if (str_equal_ignore(o->option, "<INTEGER>")) {
    /*
     * <INTEGER> matches if option string looks like an integer,
     * for instance -123 or -0x3f.  If there are any trailing
     * non-matching characters, it does not match.
     */
    int n = (int)strtol(opt, &p, 0);
    if ( (p != opt) && (*p == '\0') ) {
      if (o->argPtr != NULL) {
        *(int*)(o->argPtr) = n;
      }
      return 1;
    }
  } else if (str_equal_ignore(o->option, "<REAL>")) {
    /*
     * <REAL> matches if option string looks like a float,
     * for instance -1.23 or -0x3f.  If there are any trailing
     * non-matching characters, it does not match.
     * NOTE: integers also match this, so if you have both
     * <INTEGER> and <REAL>, the first takes precedence.
     */
    double n = (double)strtod(opt, &p);
    if ( (p != opt) && (*p == '\0') ) {
      if (o->argPtr != NULL) {
        *(double*)(o->argPtr) = n;
      }
      return 1;
    }
  } else if (str_equal_ignore(o->option, "<HELP>")) {
    /*
     * <HELP> matches -?, -h, or -help.
     * If we get a match, print the help message,
     * replacing any '<CMD>' in the string with the
     * command name, and exit (unless TTC_OPT_NO_EXIT given).
     */
    if (str_equal(opt, "?") || ol->str_eq(opt, "h") ||
        ol->str_eq(opt, "help")) {
      ttc_dump_help(stdout, ol->cmdname, (char*)o->argPtr);
      if (! (ol->options & TTC_OPT_NO_EXIT)) {
        fflush(stdout);
        fflush(stderr);
        exit(0);
      }
      return 1;
    }
  } else if (str_equal_ignore(o->option, "<HELPFUNC>")) {
    /*
     * <HELPFUNC> is like <HELP>, but invokes a callback
     * instead of printing the string.
     */
    if (str_equal(opt, "?") || ol->str_eq(opt, "h") ||
        ol->str_eq(opt, "help")) {
      void (*func) () = (void (*)()) (o->argPtr);
      func();
      return 1;
    }
  } else if (str_equal_ignore(o->option, "<VERSION>")) {
    /*
     * <VERSION> matches -V or -version.
     * If we get a match, print the version message and exit
     * (unless TTC_OPT_NO_EXIT given).
     */
    if (str_equal(opt, "V") || ol->str_eq(opt, "version")) {
      if (o->argPtr == NULL) {
        char buf[80];
        printf("%s\n", ttc_get_version_name(buf, sizeof(buf)));
      } else {
        printf("%s\n", (char*)o->argPtr);
      }
      if (! (ol->options & TTC_OPT_NO_EXIT)) {
        fflush(stdout);
        fflush(stderr);
        exit(0);
      }
      return 1;
    }
  } else if (str_equal_ignore(o->option, "<VERSFUNC>")) {
    /*
     * <VERSFUNC> is like <VERSION>, but invokes a callback
     * instead of printing the string.
     */
    if (str_equal(opt, "V") || ol->str_eq(opt, "version")) {
      void (*func) () = (void (*)()) (o->argPtr);
      func();
      return 1;
    }
  } else if (str_equal_ignore(o->option, "<DSN>")) {
    /*
     * <DSN> matches -dsn.
     * If we get a match, set the given variable and got_dsn.
     */
    if (ol->str_eq(opt, "dsn")) {
      if (copy_str_arg(ol, o, argv[*idxP - 1], idxP, argc, argv, NULL)) {
        ol->got_dsn = 1;
        return 1;
      } else {
        return 0;
      }
    }
  } else if (str_equal_ignore(o->option, "<CONNSTR>")) {
    /*
     * <CONNSTR> matches at least -connstr or -connStr regardless of
     * TTOPT_IGNORE_CASE.
     * If we get a match, set the given variable and got_connstr.
     */
    if (ol->str_eq(opt, "connstr") || str_equal(opt, "connstr") || str_equal(opt, "connStr")) {
      if (copy_str_arg(ol, o, argv[*idxP - 1], idxP, argc, argv, NULL)) {
        ol->got_connstr = 1;
        return 1;
      } else {
        return 0;
      }
    }

  }
  return 0;
} /* chk_special_option */

/*
 * Got something like '-opt', but it there's no corresponding
 * option specifier.  Try checking for specifier like 'o:s'
 * or 'o', 't'.  Don't bother checking for 'o', 'p:s'.
 * Only :s and :i are allowed with bundling.
 * Return a true value if there is a match.
 */
static int
handle_single_char(OptionList* ol, const char* opt, int* idxP,
                   int argc, char* const * argv)
{
  Option* o;
  const char* p = opt;
  char* q;

  for (;;) {
    int matched = 0;
    if (*p == '\0') {
      /* we matched everything: success */
      return 1;
    }
    for (o = ol->head; o != NULL; o = o->next) {
      if (o->strlen > 1) continue;
      /* got a single-char option, see if it matches */
      if (*(o->option) != *p) continue;

      /* matches, check the type */
      if (o->argType == 'i') {
        /* the rest of the string has to be numeric */
        int n = (int)strtol(p+1, &q, 0);
        if ( (q == p+1) || (*q != '\0') ) {
          /* give bad numeric/trailing junk message,
           * just give an unknown option message */
          return 0;
        } else {
          /* got the numeric value */
          if (o->argPtr != NULL) {
            *(int*)o->argPtr = n;
          }
          return 1;
        }

      } else if (o->argType == 's') {
        /* the rest of the string is the option argument */
        int len = strlen(p+1);
        if (len == 0) {
          return 0; /* nothing there */
        } else if (len >= o->argLen) {
          return opt_error(ol, "%s: option %s is too long (max %d)\n",
                           ol->cmdname, p, o->argLen);
        } else {
          strcpy((char*)o->argPtr, p+1);
          return 1;
        }

      } else if (o->argType == '\0') {
        /* set value, check for plain bundled option */
        if (o->argPtr != NULL) {
          *(int*)o->argPtr = 1;
        }
        ++p;
        matched = 1;
        break; /* from for loop */
      }
    }
    if (! matched) {
      /* nothing matched */
      break;
    }
  }
  return 0;
} /* handle_single_char */

/*
 * See if opt matches one of the normal options.
 * Return pointer to the option structure if so,
 * otherwise return NULL.
 */
static Option*
exact_match(OptionList* ol, const char* opt, int* errflagP)
{
  int lo, hi, mid;
  int cmp;
  char* opt_alloced = NULL;
  const char* opt_cmp;
  int len;

  *errflagP = 0;
  if (ol->sorted == NULL) return NULL;

  len = strlen(opt);
  if ( (len == 1) || (ol->options & TTC_OPT_CASE_SENSITIVE) ) {
    opt_cmp = opt;  /* use as is */
  } else {
    /* convert to lower case before comparing */
    opt_alloced = str_lc_dup(opt);
    if (opt_alloced == NULL) {
      *errflagP = 1;
      opt_error(ol, "%s: Out of memory.\n", ol->cmdname);
      return 0;
    }
    opt_cmp = opt_alloced;
  }

  /* find range via binary search */
  lo = 0;
  hi = ol->n_sorted-1;
  while (lo <= hi) {
    mid = (lo + hi) / 2;
    cmp = strcmp(opt_cmp, ol->sorted[mid]->option);
    if (cmp < 0) {
      hi = mid-1;
    } else if (cmp > 0) {
      lo = mid+1;
    } else {
      /* got a match */
      if (opt_alloced) free(opt_alloced);
      return ol->sorted[mid];
    }
  }
  if (opt_alloced) free(opt_alloced);
  return 0; /* no match */
} /* exact_match */


/*
 * See if opt is a unique abbreviation of one option.
 * Return pointer to the option structure if so,
 * otherwise return NULL.
 */
static Option*
abbrev_match(OptionList* ol, const char* opt)
{
  int lo, hi, mid;
  int len = strlen(opt);
  int cmp;
  int (*cmpfunc)(const char*, const char*, size_t);

  if (ol->sorted == NULL) return NULL;

  if (ol->options & TTC_OPT_CASE_SENSITIVE) {
    cmpfunc = strncmp;
  } else {
    cmpfunc = strncasecmp;
  }

  /* find range via binary search */
  lo = 0;
  hi = ol->n_sorted-1;
  while (lo <= hi) {
    mid = (lo + hi) / 2;
    cmp = cmpfunc(opt, ol->sorted[mid]->option, len);
    if (cmp < 0) {
      hi = mid-1;
    } else if (cmp > 0) {
      lo = mid+1;
    } else {
      /* got a match - see if it matches anything else also */
      if ( ( (mid > 0) && (cmpfunc(opt, ol->sorted[mid-1]->option, len) == 0) ) ||
           ( (mid < ol->n_sorted-1) && (cmpfunc(opt, ol->sorted[mid+1]->option, len) == 0) ) ) {
        return 0; /* ambiguous */
      } else {
        return ol->sorted[mid]; /* got a single match */
      }
    }
  }
  return 0; /* no match */
} /* abbrev_match */



/*
 * match to find the given option string in the list.
 * Return a true value if it is found and there are
 * no errors.
 * If multi_only is set, only allow multi-char options.
 */
static int
find_option(OptionList* ol, const char* opt, int* idxP,
            int multi_only,
            int argc, char* const * argv)
{
  Option* o;
  int rc = 0;
  char* eq;
  int err;

  if (multi_only && (strlen(opt) == 1)) {
    /* only accepting multi-character options */
    return 0;
  }

  /* Look through the normal options */
  o = exact_match(ol, opt, &err);
  if (err) {
    return 0;
  } else if (o != NULL) {
    return handle_option(ol, o, opt, idxP, argc, argv, 0, NULL);
  }

  /* Not found, try for special options */
  for (o = ol->head; o != NULL; o = o->next) {
    if (*(o->option) == '<') {
      if (chk_special_option(ol, o, opt, idxP, argc, argv)) {
        return 1;
      }
    }
  }

  /*
   * Didn't find an exact match, but something like '-opt'
   * could be interpreted as '-o -p -t', or as '-o' with an
   * option-argument of 'pt'.  Check for those.
   */
  if (! multi_only) {
    if (strlen(opt) > 1) {
      if (handle_single_char(ol, opt, idxP, argc, argv)) {
        return 1;
      }
    }
  }

  /*
   * Check for a negate-able option.
   */
  if ( ( (ol->options & TTC_OPT_CASE_SENSITIVE) && (strncmp(opt, "no", 2) == 0) ) ||
       ( !(ol->options & TTC_OPT_CASE_SENSITIVE) && (strncasecmp(opt, "no", 2) == 0) ) ) {
    o = exact_match(ol, opt+2, &err);
    if (err) {
      return 0;
    } else if (o != NULL) {
      return handle_option(ol, o, opt+2, idxP, argc, argv, 1, NULL);
    }
  }

  /*
   * If that didn't work either, and TTC_OPT_ABBREV mode is on,
   * see if it matches a unique abbreviation.
   */
  if (ol->options & TTC_OPT_ABBREV) {
    o = abbrev_match(ol, opt);
    if (o != NULL) {
      return handle_option(ol, o, opt, idxP, argc, argv, 0, NULL);
    }
  }

  /*
   * Could be --option=value
   */
  eq = strchr(opt, '=');
  if ( (eq != NULL) && (*(eq+1) != '\0') ) {
    *eq = '\0';
    o = exact_match(ol, opt, &err);
    *eq = '=';
    if (err) {
      return 0;
    } else if (o != NULL) {
      return handle_option(ol, o, opt, idxP, argc, argv, 0, eq+1);
    }
  }

  return rc;
} /* find_option */


/****************************************************************/
/****************************************************************/
/**** Option parsing functions.                              ****/
/****************************************************************/
/****************************************************************/

/*
 * If string looks like an option,
 * i.e., begins with a '--' or '-', or on windows,
 * begins with either a '--' or '-' or '/',
 * return pointer to char after the -- or - or /.
 * Otherwise, return NULL.
 * Set *dblP to true value if starts with --.
 */
static const char*
get_optname(const char* opt, int* dblP)
{
  /* look for --opt first */
  if ( (*opt == '-') && (*(opt+1) == '-') ) {
    if (dblP != NULL) {
      *dblP = 1;
    }
    return opt+2;
  }

  if (dblP != NULL) {
    *dblP = 0;
  }

  /* now -opt */
  if (*opt == '-') {
    return opt+1;
  }

#if defined(WINDOWS)
  /* now /opt on windows only */
  if (*opt == '/') {
    return opt+1;
  }
#endif

  return NULL;
} /* get_optname */


/*
 * should be pretty much anything other than []{}(),;?*=!@\
 * / added to distinguish paths
 */
static int
is_idchar(char c)
{
  char* badchars = "[]{}(),;?*=!@\\/";
  return (strchr(badchars, c) == NULL);
}


/*
 * Return true value if string looks like it could be a DSN.
 */
static int
looks_like_dsn(const char* opt)
{
  const char* p;
  int n_nonblanks = 0;
  for (p = opt; *p; ++p) {
    if (!is_idchar(*p)) {
      return 0;
    }
    if (! isspace(*p)) ++n_nonblanks; /* dsn needs at least 1 non-space char */
  }
  return (n_nonblanks > 0);
} /* looks_like_dsn */


/*
 * Return true value if strings looks like it could be a connection string.
 * Should be a sequence of name=value;name=value..., value could be empty.
 */
static int
looks_like_connstr(const char* opt)
{
  const char* p = opt;
  int match = 0;

  /* allow a single leading semicolon */
  if (*p == ';') {
    ++p;
  }

  for (;;) {
    /* first need a name */
    for ( ; *p; ++p) {
      if (!is_idchar(*p)) break;
    }

    /* then an equal sign */
    if (*p++ != '=') break;

    /* then an optional value */
    while (*p && (*p != ';')) ++p;

    /* then either the end of the string, or a semicolon */
    if (*p == ';') {
      ++p;
      /* allow a single trailing semicolon */
      if (! *p) {
        match = 1;  /* looks good */
        break;
      }
    } else if (*p == '\0') {
      match = 1;  /* looks good */
      break;
    } else {
      break; /* not so good */
    }
  }

  return match;
} /* looks_like_connstr */


/*
 * Parse the given argv list.
 * Return pointer to the arg terminating the parse
 * (either a non-option argument or the terminating NULL)
 * if successful, or -1 on errors.
 */
static int
parse_args(OptionList* ol, int argc, char* const * argv)
{
  int idx;
  int nerrs = 0;
  const char* optname;
  int len, errmsglen = 0;

  idx = 1;
  while (idx < argc) {
    const char* opt = argv[idx];
    int dbl;
    ++idx;
    if (str_equal(opt, "--")) {
      /* A '--' by itself signals end of options. */
      break;
    }
    optname = get_optname(opt, &dbl);
    if (optname == NULL) {
      /* must have reached the end of the options */
      --idx;
      break;
    }
    if (! find_option(ol, optname, &idx, dbl, argc, argv)) {
      len = strlen(ol->errmsgbuf);
      if (ol->options & TTC_OPT_IGNORE_UNKNOWNS) {
        /* Ignore this one. */
      } else {
        if (len == errmsglen) {
          /* if len != errmsglen, already output an error message for this one */
          opt_error(ol, "%s: unknown option '%s'", ol->cmdname, opt);
          len = strlen(ol->errmsgbuf);
        }
        ++nerrs;
      }
      errmsglen = len;
    }
  }

  /* handle <DSN> and <CONNSTR> */
  if ( (nerrs == 0) &&
       ((ol->dsn_opt != NULL) || (ol->connstr_opt != NULL)) &&
       !(ol->got_dsn || ol->got_connstr) ) {
    /* Look at first argument left over, see if it looks like
     * a DSN or a connstr. */
    int ok = 1;
    if (idx >= argc) {
      ok = 0;
    } else {
      optname = argv[idx];
      if ((ol->dsn_opt != NULL) && looks_like_dsn(optname)) {
        if (! copy_str_arg(ol, ol->dsn_opt, argv[idx], &idx, argc, argv, NULL)) {
          ++nerrs;
        }
      } else if ((ol->connstr_opt != NULL) && looks_like_connstr(optname)) {
        if (! copy_str_arg(ol, ol->connstr_opt, argv[idx], &idx, argc, argv, NULL)) {
          ++nerrs;
        }
      } else {
        ok = 0;
      }
    }
    if (! ok && !(ol->options & TTC_OPT_IGNORE_CONNSTR_ENV) ) {
      const char* tt_connstr_env = getenv("TT_CONNSTR");
      if (tt_connstr_env != NULL && tt_connstr_env[0] != '\0') {
        /* make sure destination string is big enough */
        if (strlen(tt_connstr_env) >= (size_t) (ol->connstr_opt->argLen)) {
          opt_error(ol, "%s: TT_CONNSTR connection string value is too long (max %d)\n",
                       ol->cmdname, ol->connstr_opt->argLen);
          ++nerrs;
        } else {
          /* copy the value to ol->connstr_opt */
          strcpy((char*)ol->connstr_opt->argPtr, tt_connstr_env);
          ok = 1;
        }
      }
    }        
    if ( (! ok) && !(ol->options & TTC_OPT_NO_CONNSTR_OK) ) {
      if ((ol->dsn_opt != NULL) && (ol->connstr_opt != NULL)) {
        opt_error(ol, "%s: No DSN or connection string given.", ol->cmdname);
      } else if (ol->dsn_opt != NULL) {
        opt_error(ol, "%s: No DSN given.", ol->cmdname);
      } else {
        opt_error(ol, "%s: No connection string given.", ol->cmdname);
      }
      ++nerrs;
    }
  }

  return (nerrs == 0) ? idx : -1;
} /* parse_args */



/*
 * Do the main work of constructing the option specifier
 * list and parsing the argv list.
 */
static int
getOptions(const int argc, char* const argv[],
           Ttc_getopt_options options,
           char* errmsgbuf, int errmsgbuflen,
           const char* cmdname,
           va_list ap)
{
  OptionList optlist;
  int nac;

  /* parse the option definitions */
  init_option_list(&optlist, options, errmsgbuf, errmsgbuflen, cmdname);

  /* add options from given list to optlist */
  if (! add_options(&optlist, ap)) {
    free_opt_list(&optlist);
    return -1;
  }

  nac = parse_args(&optlist, argc, argv);
  free_opt_list(&optlist);
  return nac;
} /* getOptions */


/*
 * Skip whitespace at start of string.
 * Return pointer to first non-whitespace char.
 */
static char*
skipSpaceForward(char *start)
{
  char *p = start;
  while (isspace(*p)) {
    ++p;
  }
  return p;
} /* skipSpaceForward */


/*
 * Trim trailing whitespace at end of string;
 * don't go farther back than 'stop'.
 * Replaces first of trailing whitespace chars with
 * a NUL, and returns pointer to the NUL.
 */
static char*
trimSpaceBackward(char *start, char *stop, char *bufbegin)
{
  char *p = start - 1;
  if (p < bufbegin) {
    // can't happen if there's a newline, only with empty line
    p = bufbegin;
  }
  while (isspace(*p) && p >= stop) {
    --p;
  }
  if (p < stop) {
    p = stop;
  } else {
    *++p = '\0';
  }
  return p;
} /* trimSpaceBackward */


/*
 * Skip to end of string.
 * Return pointer to the NUL at the end of the string.
 */
static char*
skipToEnd(char *start)
{
  char *p = start;
  while (*p) {
    ++p;
  }
  return p;
} /* skipToEnd */


/*
 * Return pointer to next word in input.
 * (assume leading whitespace has been trimmed)
 * Might be a quoted string.
 * (don't allow partially quoted like: ab"cde")
 * Set *pend to first character after the word.
 */
static char*
nextWord(char *start, char **pend,
         char* errmsgbuf, int errmsgbuflen, int do_print)
{
  char *p = start;
  char *rs;
  int lastWasBackslash = 0;
  if (*p == '"') {
    rs = ++p;
    while (*p) {
      if (*p == '\\') {
        if (lastWasBackslash)
          lastWasBackslash = 0;
        else
          lastWasBackslash = 1;
      } else if (*p == '"') {
        if (!lastWasBackslash)
          break;
        lastWasBackslash = 0;
      } else {
        lastWasBackslash = 0;
      }
      ++p;
    }
    if (!*p) {
      msgbuf_error(errmsgbuf, errmsgbuflen, do_print,
                   "unterminated quoted string");
      return NULL;
    }
  } else {
    rs = p;
    while (*p) {
      if (*p == '\\') {
        if (lastWasBackslash)
          lastWasBackslash = 0;
        else
          lastWasBackslash = 1;
      } else if (isspace(*p)) {
        if (!lastWasBackslash)
          break;
        lastWasBackslash = 0;
      } else {
        lastWasBackslash = 0;
      }
      ++p;
    }
  }
  if (pend) {
    *pend = p;
  }
  return rs;
} /* nextWord */


/*
 * Add argument to the argv list, doing a realloc if needed.
 * Return true value if successful.
 */
static int
addArg(char *arg, int *pac, int *pmaxac, char ***pav,
       char* errmsgbuf, int errmsgbuflen, int do_print)
{
  if (*pac >= *pmaxac) {
    char** tmppav;
    *pmaxac += 32;
    tmppav = (char**)realloc((void*)*pav, *pmaxac * sizeof(char*));
    if (tmppav == NULL) {
      msgbuf_error(errmsgbuf, errmsgbuflen, do_print, "Realloc failed.");
      return 0;
    }
    *pav = tmppav;
  }
  if (arg == NULL) {
    (*pav)[*pac] = NULL;
  } else {
    (*pav)[*pac] = strdup(arg);
    ++*pac;
  }
  return 1;
} /* addArg */


/*
 * Get an argv list from a file.
 */
static int
getArgsFromFile(const char *fileName, int *pargc, char ***pargv,
                char* errmsgbuf, int errmsgbuflen, int do_print)
{
  FILE *fp;
#define BUFLEN 1024
  char buf[BUFLEN];
  int i, ac, maxac;
  char **av = NULL;

  if (fileName == NULL) {
    fp = stdin;
  } else {
    fp = fopen(fileName, "r");
    if (fp == NULL) {
      msgbuf_error(errmsgbuf, errmsgbuflen, do_print,
                   "Can't open %s: error %d", fileName, errno);
      goto failXit;
    }
  }

  ac = 1;
  maxac = 32;
  av = (char**)malloc(maxac * sizeof(char*));
  if (av == NULL) {
    msgbuf_error(errmsgbuf, errmsgbuflen, do_print, "Malloc failed.");
    goto failXit;
  }
  if (fileName == NULL) {
    av[0] = strdup("<stdin>");
  } else {
    av[0] = strdup(fileName);
  }
  if (av[0] == NULL) {
    msgbuf_error(errmsgbuf, errmsgbuflen, do_print, "Malloc Failed.");
    goto failXit;
  }

  while (fgets(buf, BUFLEN, fp) != NULL) {
    char *p, *q, *name, *val;
    p = skipSpaceForward(buf);
    q = trimSpaceBackward(skipToEnd(buf), p, buf);

    /* skip comments and blank lines */
    if (p == q || *p == '#') {
      continue;
    }

    name = nextWord(p, &val, errmsgbuf, errmsgbuflen, do_print);
    if (name == NULL) goto failXit;
    *val++ = '\0';
    val = skipSpaceForward(val);
    if (*val) {
      char *endP;
      val = nextWord(val, &endP, errmsgbuf, errmsgbuflen, do_print);
      *endP = '\0';
    } else {
      val = NULL;
    }

    if (! addArg(name, &ac, &maxac, &av, errmsgbuf, errmsgbuflen, do_print)) goto failXit;
    if (val != NULL) {
      if (! addArg(val, &ac, &maxac, &av, errmsgbuf, errmsgbuflen, do_print)) goto failXit;
    }
  }

  if (fp != stdin) {
    fclose(fp);
    fp = NULL;
  }

  if (! addArg(NULL, &ac, &maxac, &av, errmsgbuf, errmsgbuflen, do_print)) goto failXit;

  *pargc = ac;
  *pargv = av;

  return 1;

 failXit:

  if (fp && fp != stdin)
    fclose(fp);
  if (av) {
    for (i = 0; i < ac; ++i) {
      if (av[i])
        free(av[i]);
    }
    free(av);
  }
  return 0;
} /* getArgsFromFile */

static int
add_arg_to_argv(char*** avP, int* max_argsP, int* n_argsP, char* str,
               const char* name, char* errmsgbuf, int errmsgbuflen, int do_print)
{
  if (*n_argsP >= *max_argsP) {
    *max_argsP += 100;
    *avP = (char**)realloc(*avP, *max_argsP);
    if (*avP == NULL) {
      msgbuf_error(errmsgbuf, errmsgbuflen, do_print,
                   "%s: out of memory", name);
      return 0;
    }
  }
  (*avP)[*n_argsP] = str;
  ++*n_argsP;
  return 1;
} /* add_arg_to_argv */


/*
 * Get an argv list from a string.
 */
static int
getArgsFromStr(const char *str, const char* name,
               int *pargc, char ***pargv,
               char* errmsgbuf, int errmsgbuflen, int do_print)
{
  int len = strlen(name);
  char* p;
  int max_args, n_args;
  char** av;

  /* Get a string with the name and all options.
   * The argv array will be carved from it. */
  p = (char*)malloc(len + strlen(str) + 2);
  if (p == NULL) {
    msgbuf_error(errmsgbuf, errmsgbuflen, do_print,
                 "%s: out of memory", name);
    return 0;
  }
  strcpy(p, name);
  strcpy(p+len+1, str);

  max_args = 100;
  n_args = 0;
  av = (char**)malloc(max_args * sizeof(char*));
  if (av == NULL) {
    msgbuf_error(errmsgbuf, errmsgbuflen, do_print,
                 "%s: out of memory", name);
    free(p); /* Parfait-detected memory leak */
    return 0;
  }
  if (!add_arg_to_argv(&av, &max_args, &n_args, p,
                       name, errmsgbuf, errmsgbuflen, do_print)) {
    return 0;
  }
  p += len+1;

  /*
   * Extract each word from the string.  Quoted substrings count as
   * a single word, and quote characters are stripped from the output.
   * For instance, with input <<-s "a b c" -t " " -u ab"1 2 \"3\""cd
   * the argv array will be:
   *    <<-s>>
   *    <<a b c>>
   *    <<-t>>
   *    << >>
   *    <<-u>>
   *    <<ab1 2 "3"cd>>
   */
  for (;;) {
    char* arg_start;
    int qchar = 0;
    char* out;

    /* skip any whitespace */
    while (*p && isspace(*p)) ++p;
    if (! *p) {
      /* reached the end of the string */
      break;
    }
    arg_start = p;

    /* Look for the end of the word. */
    for (out=p ; *p ; ++p) {
      int last_escaped = 0; /* last character was escape character */
      /* Remember if there's a quote character. */
      if (*p == '"' || *p == '\'') {
        qchar = *p;
        /* Look for the matching quote character. */
        for (++p; *p; ++p) {
          if ( (*p == qchar) && !last_escaped) break;
          if (*p == '\\') {
            if (last_escaped) {
              *out++ = '\\';
            }
            last_escaped = !last_escaped;
          } else {
            *out++ = *p;
            last_escaped = 0;
          }
        }
        if (*p != qchar) {
          msgbuf_error(errmsgbuf, errmsgbuflen, do_print,
                       "%s: unmatched %c", name, qchar);
          return 0;
        }
      } else if (isspace(*p)) {
        /* reached end of the word */
        break;
      } else {
        /* not a quote character or end of word character */
        if (*p == '\\') {
          if (last_escaped) {
            *out++ = '\\';
          }
          last_escaped = !last_escaped;
        } else {
          *out++ = *p;
          last_escaped = 0;
        }
      }
    }
    /* Add word to argv list */
    if (!add_arg_to_argv(&av, &max_args, &n_args, arg_start,
                         name, errmsgbuf, errmsgbuflen, do_print)) {
      return 0;
    }
    if (*p) {
      /* insert a NUL and skip past it */
      *p++ = '\0';
    }
    *out = '\0';
  }
  /* put a NULL pointer at the end */
  if (!add_arg_to_argv(&av, &max_args, &n_args, NULL,
                       name, errmsgbuf, errmsgbuflen, do_print)) {
    return 0;
  }
  *pargc = n_args-1;
  *pargv = av;
  return 1;
} /* getArgsFromStr */

