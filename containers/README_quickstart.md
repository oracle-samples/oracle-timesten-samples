# TimesTen on Docker Quick Start

## Pre-requisites

* A host system running Docker or an equivalent, compatible, container management system (such as podman).

* This package (Dockerfile, associated scripts and content) extracted on the host. From now on we will refer to the top level directory (containers) as ***pkgdir***.

* The ability to login to the [Oracle Container Registry](https://container-registry.oracle.com) and pull images. You will need your login credentials during the image build process. This implies that (a) you have a valid oracle.com login (free to create) and (b) that you have logged into the [Oracle Container Registry](https://container-registry.oracle.com) and accepted the license agreement.

## Important Note

It is not _required_ to use any of the provided convenience scripts to build the image or run the container. The scripts are provided as a convenience that leverages the central point of configuration (the **container.cfg** file) and they also serve to illustrate the required docker commands for the various functions.
 
## Build configuration

Before starting to build the TimesTen image and container, you need to perform some simple configuration steps.

1. In a terminal session, change directory to ***pkgdir***

2. If you want to use a different name for the Docker volume used to provide persistent storage for the database, edit the **container.cfg** file and change the value defined for the **DOCKER_VOLUME** variable. Leave everything else in the file with the default settings.

## Building the image and container

1. If this is the very first time you are building the image on this host, run the **crvolume** script and make sure that it runs successfully.

        $ ./crvolume
        ttdb
        $

2. Run the **build** script as follows:

        $ ./build --login
        ...
        $

    When prompted, enter your credentials for the Oracle Container Registry (required in order to pull the base image for the container). Wait for the build to complete. It only takes a short time.
    
## Running the container
    
1. Start the container by running the **ttstart** script:

        $ ./ttstart
        $
    
2.  When the container has started (it may take a little while on the first run), you can login to the container using the script **ttconnect**:

        $ ./ttconnect
        Last login: Mon Jan 17 13:54:40 UTC 2022`
        NOTE: No Java version found in PATH, setting environment for Java version 8

        LD_LIBRARY_PATH set to /timesten/ttinst/install/lib:/timesten/ttinst/install/ttoracle_home/instantclient

        PATH set to /timesten/ttinst/bin:/timesten/ttinst/install/bin:/timesten/ttinst/install/ttoracle_home/instantclient:/timesten/ttinst/install/ttoracle_home/instantclient/sdk:/usr/local/bin:/usr/bin:/usr/local/sbin:/usr/sbin

        CLASSPATH set to /timesten/ttinst/install/lib/ttjdbc8.jar:/timesten/ttinst/install/lib/orai18n.jar:/timesten/ttinst/install/lib/timestenjmsxla.jar:/timesten/ttinst/install/3rdparty/jms1.1/lib/jms.jar:.

        TNS_ADMIN set to

        TIMESTEN_HOME set to /timesten/ttinst

        [oracle@tthost ~]$

3. Now you can run TimesTen utilities:

        [oracle@tthost ~]$ ttVersion
        TimesTen Release 22.1.1.1.0 (64 bit Linux/x86_64) (ttinst:6624) 2021-11-17T12:18:47Z
          Instance admin: oracle
          Instance home directory: /timesten/ttinst
          Group owner: oinstall
          Daemon home directory: /timesten/ttinst/info
          PL/SQL enabled.
        [oracle@tthost ~]$ ttIsql sampledb
     
        Copyright (c) 1996, 2021, Oracle and/or its affiliates. All rights reserved.
        Type ? or "help" for help, type "exit" to quit ttIsql.

        connect "DSN=sampledb";
        Connection successful: DSN=sampledb;UID=oracle;DataStore=/timesten/db/sampledb;DatabaseCharacterSet=AL32UTF8;ConnectionCharacterSet=AL32UTF8;LogFileSize=128;LogBufMB=128;PermSize=1024;TempSize=128;ForceDisconnectEnabled=1;
        (Default setting AutoCommit=1)
        Command> select * from APPUSER.SIMPLETAB;
        < 1, This is row 1, 2022-01-17 14:06:10.000000 >
        1 row found.
        Command> quit
        Disconnecting...
        Done.
        [oracle@tthost ~]$ exit
        logout
        $
 
## Stopping the container 
     
1. Finally, stop the container. Your database will be ready the next time you start it (unless you successfully run the **rmvolume** script in the meantime):

        $ ./ttstop
     
## Using the container

Please consult the [README_usage](./README_usage.md) file for full details on using the container.

## Cleaning up

If you want to remove everything you have created (including your database and any data within it) run the following commands:

        $ ./rmcontainer
        $ ./rmimage
        $ ./rmvolume
        $ ./rmnetwork