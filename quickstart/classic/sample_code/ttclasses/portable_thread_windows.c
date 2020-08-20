/* 
 * portable_thread_windows.c : windows-specific methods for the simple thread
 *                             abstraction layer defined in <portable_thread.h>
 *
 * Copyright (C) 1999, 2007, Oracle. All rights reserved.
 * All rights reserved. 
 */

#ifdef _WIN32

#include "portable_thread.h"

/*----------------------------------------------------------------------*/

int
PortableThreadCreate(void*(*start_address)(void*),
               unsigned stack_size,
               void* arg,
               portable_thread* threadP /* OUT */)
{
  *threadP = (HANDLE)_beginthread((void (__cdecl*) (void*))start_address, 
                                  stack_size, arg);

  if ( *threadP != (HANDLE)-1 )
    return 0;
  else
    return -1;
}

/*----------------------------------------------------------------------*/

int
PortableThreadJoin(portable_thread t, void** statusPP, int* errorP)
{
  int rc;
  int ret = 0;
  int err;

  rc = WaitForSingleObject((HANDLE) t, INFINITE);
  if (rc == WAIT_FAILED) {
    err = GetLastError();
    if (err == ERROR_INVALID_HANDLE) { /* Handle is invalid if the
                                          thread has already exited. */
      ret = 0;
    }
    else {
      if (errorP) 
        *errorP = GetLastError();
      ret = -1;
    }
  }
  else {
    if (rc == WAIT_OBJECT_0) {
      if (statusPP != NULL)
        GetExitCodeThread((HANDLE)t, (LPDWORD) statusPP);
      ret = 0;
    }
    else {
      if (errorP)
        *errorP = 666;
      ret = -1;
    }
  }
  return ret;
}

/*----------------------------------------------------------------------*/

#endif /* _WIN32 */
