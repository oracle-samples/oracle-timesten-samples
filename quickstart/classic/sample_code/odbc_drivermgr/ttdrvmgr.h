/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * Project:    TimesTen Driver Manager
 * Version:    2.1
 * Date:       25th January 2018
 * Author:     chris.jenkins@oracle.com
 *
 * This is the header file for the TimesTen Driver Manager (ttdrvmgr).
 * Applications that use the DM can include it if they wish to do so but
 * it is not essential.
 */

#ifndef _TT_DRVMGR_H
#define _TT_DRVMGR_H

#include <timesten.h>

/*
 * Additional native error codes
 */
#define    tt_ErrDMNoMemory        90000
#define    tt_ErrDMDriverLoad      90001
#define    tt_ErrDMNotDisconnected 90002
#define    tt_ErrDMInvalidArg      90003

/*
 * TTDM specific option and return values for SQLGetConnectOption
 */

#define    TTDM_CONNECTION_TYPE    (SQL_CONNECT_OPT_DRVR_START+1000)

#define    TTDM_CONN_NONE          0
#define    TTDM_CONN_DIRECT        1
#define    TTDM_CONN_CLIENT        2

#endif /* _TT_DRVMGR_H */

