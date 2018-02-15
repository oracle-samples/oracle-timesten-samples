/* 
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * portable_thread_unix.c : unix-specific methods for the portable thread
 *                          abstraction layer defined in <portable_thread.h>
 */



#ifndef _WIN32

#include <portable_thread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#ifdef HPUXIPF
#ifdef  __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* HPUXIPF */

/*----------------------------------------------------------------------*/
/*                         PortableThreadCreate                         */
/*----------------------------------------------------------------------*/
#ifndef LYNXOS
/*----------------------------------------------------------------------*/
/*                         Typical UNIX version                         */
/*----------------------------------------------------------------------*/

int
PortableThreadCreate(void*(*start_address)(void*),
                     unsigned stack_size,
                     void* argP, 
                     portable_thread* threadP /* OUT */)
{
  pthread_attr_t   tAttr;
  int              rc;
  int              ret = 0;

  rc = pthread_attr_init(&tAttr);
  if (rc != 0) {
    return -1;
  }

  /* Make sure each thread runs as a separate LWP to ensure SMP */
  rc = pthread_attr_setscope(&tAttr, PTHREAD_SCOPE_SYSTEM);
  if (rc != 0) {
    ret = -1;
    goto PortableThreadCreateExit;
  }

#ifdef HPUX
  /* HPUX default stack size is 64KB --> need to increase to a usable size */
  if (stack_size < 1024000) stack_size = 1024000;
#endif /* HPUX */

  if (stack_size > 0) {
    rc = pthread_attr_setstacksize(&tAttr, stack_size);
    if (rc != 0) {
      ret = -1;
      goto PortableThreadCreateExit;
    }
  }

  rc = pthread_create(threadP, &tAttr,
                      (thread_signature)start_address, /* void *(*)(void *))start_address, */
                      (void *)argP);

  /*
   * On HPUX-10, errno "may" be set.
   * On HPUX-11, it is not set.
   * So, let's just print both.
   */
  if (rc != 0) {
    fprintf(stderr, "Error spawning thread. rc = %d, errno = %d",
            rc, errno);
    /* this message will show a null context */
    ret = -1;
  }

PortableThreadCreateExit:

  rc = pthread_attr_destroy(&tAttr);
  if (rc != 0) {
  }

  return ret;                               /* Pass the pthread handle back. */
}

/*----------------------------------------------------------------------*/
/*                         PortableThreadCreate                         */
/*----------------------------------------------------------------------*/
#elif defined(LYNXOS) && (LYNX_MAJOR==3) && (LYNX_MINOR==0)
/*----------------------------------------------------------------------*/
/*                         LYNXOS 3.0 version                           */
/*----------------------------------------------------------------------*/

int
PortableThreadCreate(void*(*start_address)(void*),
                     unsigned stack_size,
                     void* argP, 
                     portable_thread* threadP /* OUT */)
{
  pthread_attr_t   tAttr;
  int              rc;
  int              ret = 0;

  rc = pthread_attr_create(&tAttr);
  if (rc != 0) {
    return -1;
  }

  if (stack_size > 0) {
    rc = pthread_attr_setstacksize(&tAttr, stack_size);
    if (rc != 0) {
      ret = -1;
      goto PortableThreadCreateExit;
    }
  }

  /* Lynx stack has trouble growing it seems
   */
  if (stack_size == 0) {
    rc = pthread_attr_setstacksize(&tAttr, 64*1024);
    if (rc != 0) {
      ret = -1;
      goto PortableThreadCreateExit;
    }
  }

  rc = pthread_create(threadP, tAttr,
                      (void *(*)(void *))start_address,
                      (void *)argP);

  /*
   * On HPUX-10, errno "may" be set.
   * On HPUX-11, it is not set.
   * So, let's just print both.
   */
  if (rc != 0) {
    fprintf(stderr, "Error spawning thread. rc = %d, errno = %d",
            rc, errno);
    /* this message will show a null context */
    ret = -1;
  }



PortableThreadCreateExit:

  rc = pthread_attr_delete(&tAttr);
  if (rc != 0) {
  }

  return ret;                         /* Pass the pthread handle back. */
}
/*---------------------------------------------------------------------*/
#endif /* ifndef LYNXOS */
/*---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*/
/*                          PortableThreadJoin                         */
/*---------------------------------------------------------------------*/
#ifndef LYNXOS
/*---------------------------------------------------------------------*/
/*                         Typical UNIX version                        */
/*---------------------------------------------------------------------*/

int
PortableThreadJoin(portable_thread t, void** statusPP, int* errorP)
{
  int rc = 0;

  rc = pthread_join(t, statusPP);
  if (rc != 0 && errorP) {
    *errorP = rc;
  }

  return rc;
}
/* End of Typical UNIX version */

/*---------------------------------------------------------------------*/
/*                          PortableThreadJoin                         */
/*---------------------------------------------------------------------*/
#elif defined (LYNXOS) && (LYNX_MAJOR==3) && (LYNX_MINOR==0)
/*---------------------------------------------------------------------*/
/*                         LYNXOS 3.0 version                          */
/*---------------------------------------------------------------------*/

int
PortableThreadJoin(portable_thread t, void** statusPP, int* errorP)
{
  int rc = 0;

  if (statusPP) {
    rc = pthread_join(t, statusPP);
  } 
  else {
    void *status = NULL;           /* Lynx *must* have status pointer */
    rc = pthread_join(t, &status);
  }
  if (errorP) {         /* If user cares about error, give it to them */
    *errorP = errno;
  }
  pthread_detach(&t);                        /* Free up the resources */

  return rc;
}
/* End of LYNXOS 3.0 version */

/*---------------------------------------------------------------------*/
/*                          PortableThreadJoin                         */
/*---------------------------------------------------------------------*/
#else
/*---------------------------------------------------------------------*/
/*                         LYNXOS 3.1 version                          */
/*---------------------------------------------------------------------*/

int
PortableThreadJoin(portable_thread t, void** statusPP, int* errorP)
{
  int rc = 0;

  if (statusPP) {
    rc = pthread_join(t, statusPP);
  } 
  else {
    void *status = NULL;            /* Lynx *must* have status pointer */
    rc = pthread_join(t, &status);
  }
  if (errorP) {          /* If user cares about error, give it to them */
    *errorP = errno;
  }
  pthread_detach(t);                          /* Free up the resources */

  return rc;
}
/* End of LYNXOS 3.1 version */

/*---------------------------------------------------------------------*/
#endif /* ifndef LYNXOS */
/*---------------------------------------------------------------------*/

#ifdef HPUXIPF
#ifdef __cplusplus
}; /* extern "C" */
#endif /* __cplusplus */
#endif /* HPUXIPF */

#endif /* !_WIN32 */




