/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * sample_utils.h : routines shared by all of the TTClasses sample programs
 */

#ifndef __TESTPROG_UTILS_H_
#define __TESTPROG_UTILS_H_


#ifdef _WIN32
# include <windows.h>
#else
// BEGIN: won't work with ODBC Driver Managers
# include <sqlunix.h>
// END: won't work with ODBC Driver Managers
#endif /* _WIN32 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// BEGIN: won't work with ODBC Driver Managers
#include <sql.h>
#include <sqlext.h>
// END: won't work with ODBC Driver Managers

/* Cross-platform macros for specifying 64-bit integer constants in programs. */

#ifdef _WIN32
#define TTC_INT8_CONST(_x) _x ## i64
#define TTC_UINT8_CONST(_x) _x ## Ui64
#else
#define TTC_INT8_CONST(_x) _x ## L
#define TTC_UINT8_CONST(_x) _x ## UL
#endif /* _WIN32 */

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef _WIN32
  typedef void (*handler_t) (int);
#endif /* !_WIN32 */

  /* Request a stop (called by the signal handler) */
  extern void RequestStop (int sig);
  
  /* Checking to see if a signal has been received */
  extern void StopRequestClear() ;
  extern int  StopRequested() ; 
  extern int  SignalReceived() ; 
  
  /* Initializing the signal handler */
  extern int  HandleSignals();
  
#ifdef  __cplusplus
  const int MAX_CMDLINE_ARGUMENTS = 100;
  const int MAX_CMDLINE_LABEL_LEN = 50;

  class ParamParser {
    
   private:
    int _howmany; // <= MAX_CMDLINE_ARGUMENTS, >= 0
    char usage_string[10240];
    
    // input values : specifying what those arguments should look like
    char label[MAX_CMDLINE_ARGUMENTS][MAX_CMDLINE_LABEL_LEN+1]; // argument label
    bool  reqd[MAX_CMDLINE_ARGUMENTS];     // is this argument required?
    int maxlen[MAX_CMDLINE_ARGUMENTS];     // what is max length of argument?
    int twinof[MAX_CMDLINE_ARGUMENTS];     // is this the same as another param?
    // NB: if maxlen == 0, then argument simply is true/false
    
    // output values : values parsed from the command line
    char * arg_value[MAX_CMDLINE_ARGUMENTS];
    
    // These four are internal routines of processArgs()
    void usage(const char * prog, bool err = true);
    int matchesZeroParamArg(const char * arglabel);
    int matchesOneParamArg(const char * arglabel);
    void setArgValue(int which, const char * value);

    ParamParser(); // intentionally private, should never be used

   public:

    // The only ctor allowable
    ParamParser(const char * usage_str);
    ~ParamParser();

    // setArg: user program specifies each viable argument, one by one
    // len==0 ==> argument does not have an associated value, it's just a "flag"
    // (e.g., "-verbose" or "-help")
    void setArg(const char * arglabel, bool required, int len) ;

    // setTwinArg: user program specifies a twin argument as meaning
    // the same (syntactic sugar) as the first argument
    void setTwinArg(const char * firstArg, const char * twinArg) ;

    // Process the program's arguments
    void processArgs(int argc, 
                     char * argv[], 
                     char * conn_string);
    
    // argUsed: after argument parsing, was this argument used on the command-line?
    bool argUsed(const char * arglabel);

    // getArgValue: after argument parsing, get this argument's command-line value
    const char * getArgValue(const char * arglabel);
    
  }; // class ParamParser

} /* extern "C" */
#endif

#endif /* __TESTPROG_UTILS_H_ */



