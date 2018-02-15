//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
//
// Licensed under the Universal Permissive License v 1.0 as shown
// at http://oss.oracle.com/licenses/upl
//
// testprog_utils.cpp: routines shared by all of the TTClasses sample programs
//
//////////////////////////////////////////////////////////////////////

#include "testprog_utils.h"
#include <ttclasses/TTInclude.h>

#ifdef _WIN32
# include <sys/timeb.h>
#else
# include <sys/times.h>
# include <sys/time.h>
# include <stdio.h>
#endif /* _WIN32 */

#include <signal.h>

#include "tt_version.h"

#define DSNNAME DEMODSN

/*----------------------------------------------------------------------------*/
/* signal handling stuff                                                      */
/*----------------------------------------------------------------------------*/

static int sigReceived;
static int sigReported;

/*
 * Signal handling
 */
void
RequestStop(int sig)
{
  sigReceived = sig;
  sigReported = 0;
}

void
StopRequestClear(void)
{
  sigReceived = 0;
  sigReported = 0;
}

int
StopRequested(void)
{
  if (sigReceived != 0) {
    if (!sigReported) {
      fprintf(stderr,
              "\n*** Stop request received (signal %d).  Terminating. ***\n",
              sigReceived);
      sigReported = 1; /* only print this message once */
    }
    return 1;
  }
  return 0;
}

int
SignalReceived(void)
{
  return (sigReceived);
}

#ifdef _WIN32
/*
 * WIN32's signal is not the BSD signal(), it's the old SYSV signal().
 *
 * Since SYSV signal() is not really robust, and since signal() is not
 * really a native WIN32 API, we use SetConsoleCtrlHandler instead to do
 * things the Windows way.
 */
static BOOL WINAPI
ConCtrlHandler(DWORD ctrlType)
{
  switch(ctrlType) {
  case CTRL_C_EVENT :
    RequestStop (SIGINT) ;
    return TRUE; /* This "signal" is handled. */

  case CTRL_BREAK_EVENT :
    RequestStop (SIGBREAK) ;
    return TRUE; /* This "signal" is also handled. */

  case CTRL_CLOSE_EVENT :
  case CTRL_LOGOFF_EVENT :
  case CTRL_SHUTDOWN_EVENT :
  default:
    return FALSE; /* Let someone else handle this */
  }
  return FALSE;
}

int HandleSignals(void)
{
  int rc;
  rc = SetConsoleCtrlHandler(ConCtrlHandler, TRUE);
  /* Our rc and theirs are inverted */
  if (rc) {
    return 0;
  }
  return 1;
}

#else
/*
 * Signal handler
 * No printf, no malloc / strdup, no longjump
 */
static void
sigHandler(int sig)
{
  RequestStop (sig) ;
}

static int
sigHandlerSet(int sig, handler_t handler)
{
  struct sigaction new_action;
  struct sigaction old_action;

  memset(&new_action, 0, sizeof(struct sigaction));
  new_action.sa_handler = handler;
  /* Temporarily block all other signals during handler execution */
  sigfillset (&new_action.sa_mask);
  /* Auto restart syscalls after handler return */
#if !defined(SB_P_OS_LYNX)
  new_action.sa_flags = SA_RESTART;
#endif /* Not on Lynx */

  if (sigaction(sig, NULL, &old_action) != 0) {
    return -1;
  }
  /* Honor inherited SIG_IGN that's set by some shell's */
  if (old_action.sa_handler != SIG_IGN) {
    if (sigaction(sig, &new_action, NULL) != 0) {
      return -1;
    }
  }

  return 0;
}

int HandleSignals(void)
{
  if (sigHandlerSet(SIGINT, (handler_t)sigHandler) != 0) {
    perror("Unable to set signal handler for SIGINT");
    return 1;
  }
  if (sigHandlerSet(SIGTERM, (handler_t)sigHandler) != 0) {
    perror("Unable to set signal handler for SIGTERM");
    return 1;
  }
  if (sigHandlerSet(SIGHUP, (handler_t)sigHandler) != 0) {
    perror("Unable to set signal handler for SIGHUP");
    return 1;
  }
  if (sigHandlerSet(SIGPIPE, (handler_t)sigHandler) != 0) {
    perror("Unable to set signal handler for SIGPIPE");
    return 1;
  }
  return 0;
}
#endif /* Handling ctrl-c on Windows and Unix */

// ---------------------------------------------------------------------
// class ParamParser -- parameter parsing stuff
// ---------------------------------------------------------------------

ParamParser::ParamParser(const char * usage_str) : _howmany(0)
{
  strncpy(usage_string, usage_str, 10240-1);

  for (int i = 0 ; i < MAX_CMDLINE_ARGUMENTS ; i++)
     arg_value[i] = NULL ;
}

ParamParser::~ParamParser()
{
  for (int i = 0; i < _howmany; i++) {
    delete [] arg_value[i];
  }
}

void
ParamParser::setArg(const char * arglabel, bool required, int len)
{
  if (_howmany == MAX_CMDLINE_ARGUMENTS) {
    cerr << "Too many possible arguments specified." << endl ;
    cerr << "Increase the value of the MAX_CMDLINE_ARGUMENTS global" << endl ;
    cerr << "in file testprog_utils.h and recompile." << endl ;
    return;
  }
  strncpy(label[_howmany], arglabel, MAX_CMDLINE_LABEL_LEN);
  reqd  [_howmany] = required;
  maxlen[_howmany] = len;
  twinof[_howmany] = -1;
  if (len > 0) {
    arg_value[_howmany] = new char [len];
  } else {
    arg_value[_howmany] = new char [1];
  }
  *(arg_value[_howmany]) = '\0';
  _howmany++;
}

void
ParamParser::setTwinArg(const char * origArg, const char * twinArg)
{
  if (_howmany == MAX_CMDLINE_ARGUMENTS) {
    cerr << "Too many possible arguments specified." << endl ;
    cerr << "Increase the value of the MAX_CMDLINE_ARGUMENTS global" << endl ;
    cerr << "in file testprog_utils.h and recompile." << endl ;
    return;
  }

  for (int i = 0 ; i < _howmany; i++)
  {
    if (!tt_strcasecmp(origArg,label[i])) {
      strncpy(label[_howmany], twinArg, MAX_CMDLINE_LABEL_LEN);
      reqd  [_howmany] = false;
      maxlen[_howmany] = 1;
      twinof[_howmany] = i ; // says that "twinArg" should be treated same as "origArg"
      arg_value[_howmany] = NULL; // delete [] NULL is OK
      _howmany++;
      return;
    }
  }

  cerr << "*** Bad use of ParamParser::setTwinArg():" << endl ;
  cerr << "*** No such arg label registered: '" << origArg << "'" << endl ;
}

void
ParamParser::usage(const char * prog, bool err)
{
  const char *progname;

  /* Get the name of the program (sans path). */
  progname = (const char *)strrchr(prog, '/');

  if (progname != NULL) {
    ++progname;
  }
  else  {
#ifdef _WIN32
    progname = strrchr(prog, '\\');
    if (progname != NULL)
      ++progname;
    else
#endif /* _WIN32 */
      progname = prog;
  }

  (err ? cerr : cout) << endl <<
    "Usage: " << endl <<
    " " << progname << " [-h | -help | -?]" << endl <<
    " " << progname << " [-V | -version]" << endl <<
    usage_string <<
    "  -h, -help, -?       Print this message and exit." << endl <<
    "  -V, -version        Print product version number and exit." << endl <<
    "  <DSN>               DSN to connect to." << endl <<
    "  -connstr <connstr>  Connection string to be used." << endl;


  if (err)
    exit(-2);
}

void
ParamParser::processArgs(int argc, char * argv[], char * conn_string)
{
  int i = 1;
  int argnum = -1;


  while (i < argc)
  {
    if (!tt_strcasecmp(argv[i], "-h") || !tt_strcasecmp(argv[i], "-help") ||
        !strcmp(argv[i], "?")) {
      usage(argv[0], false);
      exit(0);
    }
    else if (!strcmp(argv[i], "-V") || !tt_strcasecmp(argv[i], "-version")) {
      cout << TTVERSION_STRING << endl;
      exit(0);
    }
    else if (!tt_strcasecmp(argv[i], "-connStr")) {
      if (argc < i+2 ) {
        cerr << "*** Parameter '-connStr' specified without argument." << endl ;
        usage(argv[0]);
        exit(1);
      }
      strcpy(conn_string, argv[i+1]);
      i += 2;
      continue;
    }
    else if (!tt_strcasecmp(argv[i], "-dsn")) {
      if (argc < i+2 ) {
        cerr << "*** Parameter '-dsn' specified without argument." << endl ;
        usage(argv[0]);
        exit(1);
      }
      sprintf(conn_string, "dsn=%s", argv[i+1]);
      i += 2;
      continue;
    }
    else if ( (argnum = matchesZeroParamArg(argv[i])) >= 0) {
      setArgValue(argnum, argv[i+1]);
      i += 1;
    }
    else if ( (argnum = matchesOneParamArg(argv[i])) >= 0) {
      if (argc < i+2 ) {
        cerr << "*** Valid parameter '" << argv[i] << "' specified without "
             << "required argument." << endl;
        usage(argv[0]);
        exit(1);
      }
      setArgValue(argnum, argv[i+1]);
      i += 2;
    }
    else if ((i == argc-1) && (!*conn_string)) {
      // must be a DSN
      sprintf(conn_string, "dsn=%s", argv[i]);
      i += 2;
      continue;
    }
    else {
      cerr << "*** Invalid program argument: '" << argv[i] << "'" << endl ;
      usage(argv[0]);
      exit(1);
    }
  }

  /* If connection string is not specified, default it */
  if (*conn_string == '\0') {

    /* Default the DSN and UID */
    sprintf(conn_string, "dsn=%s;%s", DSNNAME, UID);
  }

  /* Verify all required parameters have been supplied */

  bool ok_params = true;
  for (i = 0 ; i < _howmany; i++)
  {
    if (reqd[i] && *(arg_value[i]) == '\0') {
      cerr << endl << "Required argument '" << label[i] << "' not supplied." << endl;
      ok_params = false;
    }
  }

  if (!ok_params) {
//    cerr << "Missing some required parameters, aborting." << endl ;
    usage(argv[0]) ;
  }

}

void
ParamParser::setArgValue(int which, const char * value)
{
  if (maxlen[which] > 0)
    strncpy(arg_value[which], value, maxlen[which]);
  else
    *(arg_value[which]) = 'y';
}

int
ParamParser::matchesZeroParamArg(const char * arglabel)
{
  int idx = -1;

  for (int i = 0 ; i < _howmany; i++)
  {
    if (!tt_strcasecmp(arglabel,label[i])) {
      idx = i;
      if (twinof[i] >= 0)
        idx = twinof[i];
      if (maxlen[idx] == 0) // i.e., no parameter value is supplied with this param
        return idx;
    }
  }
  return -1;
}

int
ParamParser::matchesOneParamArg(const char * arglabel)
{
  int idx = -1;

  for (int i = 0 ; i < _howmany; i++)
  {
    if (!tt_strcasecmp(arglabel,label[i])) {
      idx = i;
      if (twinof[i] >= 0)
        idx = twinof[i];
      if (maxlen[idx] > 0) // i.e., some parameter value is supplied with this param
        return idx;
    }
  }
  return -1;
}

bool
ParamParser::argUsed(const char * arglabel)
{
  for (int i = 0 ; i < _howmany; i++)
  {
    if (!tt_strcasecmp(arglabel,label[i])) {
      if (*(arg_value[i]) == '\0')
        return false;
      else
        return true;
    }
  }

  cerr << "*** Bad use of ParamParser::argSeen():" << endl ;
  cerr << "*** No such arg label registered: '" << arglabel << "'" << endl ;

  return false;
}


const char *
ParamParser::getArgValue(const char * arglabel)
{
  for (int i = 0 ; i < _howmany; i++)
  {
    if (!tt_strcasecmp(arglabel,label[i]))
      return arg_value[i];
  }

  cerr << "*** Bad use of ParamParser::getArg():" << endl ;
  cerr << "*** No such arg label registered: '" << arglabel << "'" << endl ;

  return NULL;
}



