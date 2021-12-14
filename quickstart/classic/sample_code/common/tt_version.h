/*
 * TimesTen demo version file.
 *
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

#define TTRELEASE "22.1.1.1.0"
#undef TTVERSION
#define TTVERSION "221"
#define TTMAJOR1 22
#define TTMAJOR2 1
#define TTMAJOR3 1
#define TTPATCH 1
#define TTPORTPATCH 0
#define TTVERSION_STRING "TimesTen Release 22.1.1.1.0"

#define TTPRODUCT "TimesTen"

/* definitions for the DSNs used by the quickstart programs */ 
#ifdef TTCLIENTSERVER
#define DEMODSN "sampledbCS"
#else
#define DEMODSN "sampledb"
#endif

#define UID             "UID=appuser"
#define UIDNAME         "appuser"
#define XLAUID          "UID=xlauser"
#define XLA_TABLE       "mytable"
#define ADMINUID        "UID=adm"
#define ADMINNAME       "adm"
#define DEMO_PERMSIZE   40

#define MAX_USERNAME_SIZE 31
#define MAX_PASSWORD_SIZE 31

