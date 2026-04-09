Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.

Before running these demos, set up the environment and data sources as described in the Installation Guide. The script ttquickstartenv.[c]sh can be used to set the environment variables needed to run the sample programs.

If you are running csh or tcsh, do

    source ttquickstartenv.csh

If you are running one of the other shells (e.g., ksh or sh), do

    . ttquickstartenv.sh 

These files will update your PATH, your CLASSPATH and your platform-specific library search path environment variables.

The sample programs in this directory are:


.inp files - SQL scripts which can be used with the ttIsql and ttIsqlCS utilities. For example:

    ttIsql -f ttIsql.inp sampledb