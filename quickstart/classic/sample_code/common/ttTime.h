/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * ttTime.h
 *
 */

#ifndef __TT_TIME
#define __TT_TIME

#ifdef TTCLASSES
#include <TT__Header.h> /* only include this header when compiling TTClasses library */
#else
#ifndef IMPORTEXPORT
#define IMPORTEXPORT    /* for C-based demo programs, there is no DLL */
#endif
#endif /* TTCLASSES */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/****************************************************************************/
#ifdef _WIN32

/* WINDOWS Version */

#include <windows.h>
#include <sys/types.h>
#include <sys/timeb.h>

typedef struct {
  FILETIME kernelTime;
  FILETIME userTime;
} ttThreadTimes;


typedef union {
  LARGE_INTEGER time64;
  struct _timeb notSoLargeTime;
} ttWallClockTime;

/****************************************************************************/
#else

/* UNIX version */

#include <sys/times.h>
#ifdef LYNXOS
#include <time.h>
#else
#include <sys/time.h>
#endif /* LYNXOS */

typedef struct tms ttThreadTimes;

typedef struct timeval ttWallClockTime;

#endif /* _WIN32 */
/****************************************************************************/

void IMPORTEXPORT ttGetThreadTimes(ttThreadTimes* timesP);
void IMPORTEXPORT ttCalcElapsedThreadTimes(ttThreadTimes* beforeP,
                                           ttThreadTimes* afterP,
                                           double* kernelDeltaP,
                                           double* userDeltaP);

void IMPORTEXPORT ttGetWallClockTime(ttWallClockTime* timeP);
void IMPORTEXPORT ttCalcElapsedWallClockTime(ttWallClockTime* beforeP,
                                             ttWallClockTime* afterP,
                                             double* nmillisecondsP);
void IMPORTEXPORT ttMillisleep (int millis) ;

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __TT_TIME */

/* Emacs variable settings */
/* Local Variables: */
/* tab-width:8 */
/* indent-tabs-mode:nil */
/* c-basic-offset:2 */
/* End: */


