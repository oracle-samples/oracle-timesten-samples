/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

#ifndef __SBRAND_H_
#define __SBRAND_H_

#include "timesten.h"

#if defined(WIN32)

double drand48(void);       /* Return a random double from 0.0 to 1.0 */
tt_ptrint lrand48(void);    /* Return random tt_ptrint from 0 to 2**31 */
tt_ptrint mrand48(void);    /* Return random tt_ptrint from -2**31 to 2**31 */
int    srand48(tt_ptrint);  /* Another way to seed the generator */

#else

#include <stdlib.h>

#endif /* NT or Not */

#endif /* __SBRAND_H_ */

/* Emacs variable settings */
/* Local Variables: */
/* tab-width:8 */
/* indent-tabs-mode:nil */
/* c-basic-offset:2 */
/* End: */
