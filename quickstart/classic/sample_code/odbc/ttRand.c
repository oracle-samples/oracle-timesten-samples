/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

#if defined(WIN32)

#include "ttRand.h"
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <math.h>

extern int _finite( double x );
#define tt_finite(x) _finite(x)

/* Take a long and initialize the random number generator */

/*********************************************************************
 *
 *  FUNCTION:       srand48
 *
 *  DESCRIPTION:    This function takes a seed as a starting point
 *                  and returns a random integer.
 *
 *  PARAMETERS:     long seed       starting seed #
 *
 *  RETURNS:        int
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

int
srand48(tt_ptrint seed)
{
  srand(seed);
  return 0;
}

/*********************************************************************
 *
 *  FUNCTION:       drand48
 *
 *  DESCRIPTION:    This function returns a random double.
 *
 *  PARAMETERS:     NONE
 *
 *  RETURNS:        double
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

double
drand48()
{
  unsigned int a;
  unsigned int b;
  double d;
  tt_uptrint bignum;
  tt_uptrint wow = (RAND_MAX << 16) | RAND_MAX;

 retry:
  a = rand();
  b = rand();
  bignum = (a << 16) | b;        /* Got a 32-bit unsigned */
  d = (double) bignum / (double) wow;
  if (!tt_finite(d)) goto retry;
  return d;
}

/*********************************************************************
 *
 *  FUNCTION:       lrand48
 *
 *  DESCRIPTION:    This function returns a random long from 0 to
 *                  2**31.
 *
 *  PARAMETERS:     NONE
 *
 *  RETURNS:        long
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

tt_ptrint
lrand48()
{
  unsigned int a;
  unsigned int b;
  tt_ptrint bignum;
  a = rand();
  b = rand();
  bignum = (a << 16) | b;
  return bignum;
}

/*********************************************************************
 *
 *  FUNCTION:       mrand48
 *
 *  DESCRIPTION:    This function returns a random tt_ptrint from -2**31 to
 *                  2**31.
 *
 *  PARAMETERS:     NONE
 *
 *  RETURNS:        tt_ptrint
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

tt_ptrint
mrand48()
{
  tt_ptrint bignum = (rand() << 15) | rand();
  tt_ptrint sign = rand() & 0x00000001;

  if (sign)
    return bignum;
  else
    return -bignum;
}

#endif /* WIN 32 */


/* Emacs variable settings */
/* Local Variables: */
/* tab-width:8 */
/* indent-tabs-mode:nil */
/* c-basic-offset:2 */
/* End: */
