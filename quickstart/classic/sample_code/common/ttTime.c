/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * ttTime.c
 */

#ifdef TTCLASSES
#define IN_DLL
#endif /* TTCLASSES */

/* Contains functions for performing elapsed-time calculations
   in a portable manner */

#include <ttTime.h>

#ifdef _WIN32

#include <stdio.h>
#include <mapiutil.h>

/*-----------------*/
/* WINDOWS VERSION */
/*-----------------*/

/*********************************************************************
 *
 *  FUNCTION:       ttGetThreadTimes
 *
 *  DESCRIPTION:    This function sets the supplied parameter's
 *                  user and kernel time for the current thread.
 *
 *  PARAMETERS:     ttThreadTimes* timesP   thread time structure
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void IMPORTEXPORT
ttGetThreadTimes(ttThreadTimes* timesP)
{
  BOOL rc;
  HANDLE curThread;
  FILETIME creationTime;
  FILETIME exitTime;
  FILETIME kTime;
  FILETIME uTime;

  memset (&kTime, 0, sizeof (FILETIME));
  memset (&uTime, 0, sizeof (FILETIME));

  curThread = GetCurrentThread();
  rc = GetThreadTimes(curThread,
                      &creationTime,
                      &exitTime,
                      &kTime,
                      &uTime);

  timesP->kernelTime = kTime;
  timesP->userTime = uTime;

}

/*********************************************************************
 *
 *  FUNCTION:       ttCalcElapsedThreadTimes
 *
 *  DESCRIPTION:    This function calculates the user and kernel
 *                  time deltas.
 *
 *  PARAMETERS:     ttThreadTimes* beforeP  beginning timestamp (IN)
 *                  ttThreadTimes* afterP   ending timestamp (IN)
 *                  double* kernelDeltaP    kernel time delta (OUT)
 *                  double* userDeltaP      user time delta (OUT)
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void IMPORTEXPORT
ttCalcElapsedThreadTimes(ttThreadTimes* beforeP,
                         ttThreadTimes* afterP,
                         double* kernelDeltaP,
                         double* userDeltaP)
{
  static const double secPerHi = (double) 4.294967296; /* 2**32 * 10**-9 */
  FILETIME *before, *after;

  before = &beforeP->kernelTime;
  after = &afterP->kernelTime;
  *kernelDeltaP = (double) ((after->dwHighDateTime - before->dwHighDateTime) * secPerHi
                           + (after->dwLowDateTime - before->dwLowDateTime) * 100e-9);
  before = &beforeP->userTime;
  after = &afterP->userTime;
  *userDeltaP = (double) ((after->dwHighDateTime - before->dwHighDateTime) * secPerHi
                         + (after->dwLowDateTime - before->dwLowDateTime) * 100e-9);
}

/*********************************************************************
 *
 *  FUNCTION:       ttGetWallClockTime
 *
 *  DESCRIPTION:    This function gets the current wall-clock time.
 *
 *  PARAMETERS:     ttWallClockTime* timeP  tms time structure (OUT)
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void IMPORTEXPORT
ttGetWallClockTime(ttWallClockTime* timeP)
{
  LARGE_INTEGER frequency;
  if ( QueryPerformanceFrequency(&frequency) ) {
    QueryPerformanceCounter(&(timeP->time64));
  }
  else {
    _ftime(&(timeP->notSoLargeTime));
  }
}

/*********************************************************************
 *
 *  FUNCTION:       ttCalcElapsedWallClockTime
 *
 *  DESCRIPTION:    This function calculates the elapsed wall-clock
 *                  time in msec.
 *
 *  PARAMETERS:     ttWallClockTime* beforeP        starting timestamp
 *                  ttWallClockTime* afterP         ending timestamp
 *                  double* nmillisecondsP          elapsed time (OUT)
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void IMPORTEXPORT
ttCalcElapsedWallClockTime(ttWallClockTime* beforeP,
                           ttWallClockTime* afterP,
                           double* nmillisecondsP)
{
  LARGE_INTEGER frequency;

  if ( QueryPerformanceFrequency(&frequency) ) {
    *nmillisecondsP = 1000 * ((double) (afterP->time64.QuadPart
                                       - beforeP->time64.QuadPart))
      / frequency.QuadPart;

  }
  else {
    double start;
    double end;

    start = (double) beforeP->notSoLargeTime.time * 1000. +
      (double) beforeP->notSoLargeTime.millitm;
    end   = (double) afterP->notSoLargeTime.time  * 1000. +
      (double) afterP->notSoLargeTime.millitm;

    *nmillisecondsP = (double) (end - start);
  }
}

void IMPORTEXPORT
ttMillisleep (int millis)
{
  if (millis <= 0) return;

  Sleep(millis) ;
}

#else /* _WIN32 */

/*--------------*/
/* UNIX VERSION */
/*--------------*/

#include <unistd.h>

/*********************************************************************
 *
 *  FUNCTION:       ttGetThreadTimes
 *
 *  DESCRIPTION:    This function sets the supplied parameter's
 *                  tms structure.
 *
 *  PARAMETERS:     ttThreadTimes* timesP   tms time structure
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void
ttGetThreadTimes(ttThreadTimes* timesP)
{
  (void) times(timesP);
}

/*********************************************************************
 *
 *  FUNCTION:       ttCalcElapsedThreadTimes
 *
 *  DESCRIPTION:    This function calculates the user and kernel
 *                  time deltas.
 *
 *  PARAMETERS:     ttThreadTimes* beforeP  beginning timestamp (IN)
 *                  ttThreadTimes* afterP   ending timestamp (IN)
 *                  double* kernelDeltaP    kernel time delta (OUT)
 *                  double* userDeltaP      user time delta (OUT)
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void
ttCalcElapsedThreadTimes(ttThreadTimes* beforeP,
                         ttThreadTimes* afterP,
                         double* kernelDeltaP,
                         double* userDeltaP)
{
  double ticks = (double)sysconf(_SC_CLK_TCK);

  *kernelDeltaP = (afterP->tms_stime - beforeP->tms_stime) / ticks;
  *userDeltaP = (afterP->tms_utime - beforeP->tms_utime) / ticks;
}

/*********************************************************************
 *
 *  FUNCTION:       ttGetWallClockTime
 *
 *  DESCRIPTION:    This function gets the current wall-clock time.
 *
 *  PARAMETERS:     ttWallClockTime* timeP  tms time structure (OUT)
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void
ttGetWallClockTime(ttWallClockTime* timeP)
{
  gettimeofday(timeP, NULL);
}

/*********************************************************************
 *
 *  FUNCTION:       ttCalcElapsedWallClockTime
 *
 *  DESCRIPTION:    This function calculates the elapsed wall-clock
 *                  time is msec.
 *
 *  PARAMETERS:     ttWallClockTime* beforeP        starting timestamp
 *                  ttWallClockTime* afterP         ending timestamp
 *                  double* nmillisecondsP          elapsed time (OUT)
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void
ttCalcElapsedWallClockTime(ttWallClockTime* beforeP,
                           ttWallClockTime* afterP,
                           double* nmillisP)
{
  *nmillisP = (afterP->tv_sec - beforeP->tv_sec)*1000.0 +
    (afterP->tv_usec - beforeP->tv_usec)/1000.0;
}

void
ttMillisleep (int millis)
{
  struct timeval t;

  if (millis <= 0) return;

  t.tv_sec  =  millis / 1000 ;
  t.tv_usec = (millis % 1000) * 1000 ;
  select(0, NULL, NULL, NULL, &t);
}

#endif /* _WIN32 */

/* Emacs variable settings */
/* Local Variables: */
/* tab-width:8 */
/* indent-tabs-mode:nil */
/* c-basic-offset:2 */
/* End: */

