/*
 * Copyright (c) 1999, 2019, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * Project:    TimesTen Driver Manager
 * Version:    18.1.1
 * Date:       19th February 2021
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
 * TTDM specific option and values for SQLGetEnvAttr
 */

#define    SQL_ATTR_TTDM_VERSION       20000
#define    SQL_ATTR_TTDM_CAPABILITIES  20001

#define    SQL_ATTR_TTDM_CLIENT        0x01 // Client driver functions are available
#define    SQL_ATTR_TTDM_DIRECT        0x02 // Direct driver functions are available
#define    SQL_ATTR_TTDM_XLA           0x04 // XLA functions are available
#define    SQL_ATTR_TTDM_ROUTING       0x08 // Routing API functions are available
#define    SQL_ATTR_TTDM_UTILITY       0x10 // Utility API functions are available

/*
 * TTDM specific option and return values for SQLGetConnectOption
 * and SQLGetConnectAttr
 */

#define    TTDM_CONNECTION_TYPE    (SQL_CONNECT_OPT_DRVR_START+3000)

#define    TTDM_CONN_NONE          0
#define    TTDM_CONN_DIRECT        1
#define    TTDM_CONN_CLIENT        2

#endif /* _TT_DRVMGR_H */

