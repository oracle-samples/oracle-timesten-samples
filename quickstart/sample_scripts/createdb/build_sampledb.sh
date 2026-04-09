#!/bin/sh
# (C) Copyright (C) 1999, 2009, Oracle. All rights reserved.
#  
# Build sample database for running the TimesTen 11.2.2 sample programs 
#
# This script will remove the existing sample database and 
# rebuild the database schema from the beginning 
#
# This script must be run by the TimesTen instance administrator
# The TimesTen daemon must be started before running this script
#  
echo 
echo Checking the quickstart environment 
echo
if [[ "${QUICKSTART_HOME}" == "" ]]
then
    echo "*** QuickStart environment is not setup."
    echo "*** Please 'source' ../../ttquickstartenv.sh"
    echo
    exit 1
fi
echo 
echo Removing existing sample database
echo
ttdestroy sampledb
echo 
echo Building new sample database
echo
ttisql -f build_db.sql -connstr "dsn=sampledb" 
ttbulkcp -i -connstr "dsn=sampledb" appuser.print_media populate_lob_data.dump
exit 0
