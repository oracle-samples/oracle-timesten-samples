# TimesTen Kubernetes Operator

The TimesTen Kubernetes Operator creates, configures and manages Oracle TimesTen In-Memory Databases running on Kubernetes. This sample roughly follows the steps in the [TimesTen Kubernetes Operator User's Guide](https://docs.oracle.com/database/timesten-18.1/TTKUB/toc.htm) which is part of the [TimesTen Documentation set](https://docs.oracle.com/en/database/other-databases/timesten/).

Some of the files used in the sample are provided in the [examples](./examples) directory for your convenience. In some cases they may need minor customization to match your environment.

## Requirements

* A suitable TimesTen software distribution ZIP file. Typically this will be the latest **Linux x64** version available on the [TimesTen Downloads page](https://www.oracle.com/database/technologies/timesten-downloads.html).
* A Kubernetes cluster, running a supported version of Kubernetes, that you have access too. Please consult the release notes for the specific TimesTen release that you are using for information on the Kubernetes versions supported by that release. 
* A suitable 'client' machine with a compatible version of the **kubectl** command line tool. This is used to build the required Docker images and to access/control the Kubernetes cluster. Typically this may be a macOS system or a Linux system (or VM).
* A functional Docker installation on the client machine, which provides the **docker** command line tool.
* Access to a suitable container registry that is accessible from both the client machine and the Kubernetes cluster.
* The client machine is configured suitably to access both the Kubernetes cluster and the container registry.

In this example:

* The Kubernetes cluster is provisioned in Oracle Kubernetes Environment (OKE) running on Oracle Cloud Infrastructure (OCI).
* The container registry is the Oracle Container Image Registry (OCIR).
* The client system is a macOS system running Docker Desktop for macOS.
* The TimesTen version being used is 18.1.4.19.0.
* The example assumes that you are using the Kubernetes 'default' namespace. If you are using a different namespace then substitute that for 'default' where relevant.

The example is not specific to the above environment and should work with any Kubernetes environment and client system with only very minor customization.

## Overview

This example walks you step by step through the following topics. It is intended to be completed in sequence, but you can stop at any point and then resume later if required.

1. Preparing the TimesTen distribution and operator
1. Building the TimesTen Operator image
1. Deploying the TimesTen Operator
1. Building the TimesTen container image
1. Preparing the configuration metadata files and objects
1. Deploying the TimesTenClassic object
1. Monitoring deployment and general status checks
1. Verify the Active and Standby databases
1. Trigger a database failover and recovery
1. Create and configure a client pod
1. Cleaning up

## Preparing the TimesTen distribution and operator

First we need to extract the TimesTen Operator files from the TimesTen software distribution and then use some of the files to create the Operator service account and TimesTen Operator CRD within our cluster.

1. Create a suitable sub-directory which will become your working directory, and then change directory to that directory:

        $ mkdir -p <my-work-dir>
        $ cd <my-work-dir>

2. Copy the TimesTen software distribution from wherever you saved it when you downloaded it to the current directory:

        $ cp <download-location>/timesten1814190.server.linux8664.zip .

3. Unzip the TimesTen software installation to create a tt18.1.4.19.0 directory:

        $ unzip timesten1814190.server.linux8664.zip
 
 Do _not_ delete the software distriubution ZIP file as you will need it later.
 
4. Create a directory for the operator files and change directory to there:

        $ mkdir timesten-operator
        $ cd timesten-operator
   
5. Unzip the operator.zip from from the TimesTen software distribution into this directory:

        $ unzip ../tt18.1.4.19.0/kubernetes/operator.zip
          
6. Change directory to the **deploy** directory, and then create the service account in your Kubernetes cluster:

        $ cd deploy
        $ kubectl create -f service_account.yaml
        role.rbac.authorization.k8s.io/timestenclassic-operator created
        serviceaccount/timestenclassic-operator created
        rolebinding.rbac.authorization.k8s.io/timestenclassic-operator created
 
7. Deploy the TimesTen Operator Custom Resource Definition (CRD):

         $ kubectl create -f crd.yaml
         customresourcedefinition.apiextensions.k8s.io/timestenclassics.timesten.oracle.com created
         
The next step is to prepare the container image for the TimesTen Operator.
         
## Building the TimesTen Operator image

1. Change directory to the extracted **operator** directory:

        $ cd ../operator

2. Copy the Timesten software distribution into this directory:

        $ cp ../../timesten1814190.server.linux8664.zip .
        
3. Using your favourite text editor, edit the **Dockerfile** and set the value for **TT_DISTRO** to the exact filename of the TimesTen software distribution then save the file:

        ...
        ARG TT_DISTRO=timesten1814190.server.linux8664.zip
        ...
        
4. Use the docker command to build the container image for the operator. You can use any name you like but in this example we will name it **ttclassic-operator:1**:

 
        $ docker build -t ttclassic-operator:1 .
        [+] Building 4.3s (9/9) FINISHED
        => [internal] load build definition from Dockerfile
        => => transferring dockerfile: 37B
        ...
        => exporting to image
        => => exporting layers
        => => writing image sha256:8688c64b2bbc03d0432f1338553633546f82d20da9df0ceac3a026a172335a96
        => => naming to docker.io/library/ttclassic-operator:1

5. Use the docker command to tag the Operator image. You should substitute the details of your image registry and account for the ones used in this example:

        $ docker tag ttclassic-operator:1 phx.ocir.io/<youraccount>/ttclassic-operator:1
        
6. Push the image to the container image registry:

        $ docker push phx.ocir.io/<youraccount>/ttclassic-operator:1
        The push refers to repository [phx.ocir.io/i<youraccount>/ttclassic-operator]
        d33ff8e26eb0: Layer already exists
        ...
        1: digest: sha256:3c627d80035c7fb05718067a30dd9762924b28174b4f1ceb5b33d9869e02de06 size: 1166
        
You have successfully built the TimesTen operator container image and pushed it to your chosen container image registry. Next we will deploy the operator to the cluster.

## Deploying the TimesTen Operator

If you have not already done so, you will need to create a Kubernetes secret in the cluster with the necessary information to allow the cluster to pull images from your container image registry. For example:

    $ kubectl create secret docker-registry ocirsecret \
                      --docker-server=phx.ocir.io \
                      --docker-username='<registry-username>' \
                      --docker-password='<registry-auth-token>' \
                      --docker-email='<your-email-address>'

In order to deploy the operator we need to customize some of the parameters in the **operator.yaml** file in the **deploy** directory. The parameters that you need to customize are as follows:

**replicas** This should be set to the number of replicas you want for the operator. Set it to > 1 to enable high-availability for the operator. In this example we will leave it set at 1.

**imagePullSecrets/name** Set this to the name of your registry secret (in this example **ocirsecret**).

**containers/image** set this to the registry path of the image (in this example **phx.ocir.io/*youraccount*/ttclassic-operator:1**)

Once you have made these changes to the file, you can deploy the operator.

1. Deploy the operator:

        $ kubectl create -f operator.yaml
        deployment.apps/timestenclassic-operator created
        
2. Monitor the status until the operator enters the Running state:

        $ kubectl get pods
        NAME                                      READY   STATUS              RESTARTS   AGE
        timestenclassic-operator-d847f5cd-djwls   0/1     ContainerCreating   0          10s
        ...
        $ kubectl get pods
        NAME                                      READY   STATUS    RESTARTS   AGE
        timestenclassic-operator-d847f5cd-djwls   1/1     Running   0          41s
        
You have successfully deployed the TimesTen Operator to the cluster.

## Building the TimesTen container image

Now we need to build the image for the containers that will run TimesTen and host our database in an active-standby high availability configuration.

1. Change directory to the extracted **ttimage** directory:

        $ cd ../ttimage

2. Copy the TimesTen software distribution into this directory:

        $ cp ../../timesten1814190.server.linux8664.zip .
        
3. Using your favorite text editor, edit the **Dockerfile** and set the value for **TT_DISTRO** to the exact filename of the TimesTen software distribution then save the file:

        ...
        ARG TT_DISTRO=timesten1814190.server.linux8664.zip
        ...
        
4. Use the docker command to build the container image for the operator. You can use any name you like but in this example we will name it **tt1814190:1**:

 
        $ docker build -t tt1814190:1 .
        [+] Building 131.7s (12/12) FINISHED
        => [internal] load build definition from Dockerfile
        => => transferring dockerfile: 37B
        ...
        => exporting to image
        => => exporting layers
        => => writing image sha256:478a669ae5ec4404ecdf3bf6183cb55cba9e07730fbea65d710c19fea2097894
        => => naming to docker.io/library/tt1814190:1

5. Use the docker command to tag the operator image. You should substitute the details of your image registry and account for the ones used in this example:

        $ docker tag tt1814190:1 phx.ocir.io/<youraccount>/tt1814190:1
        
6. Push the image to the container image registry:

        $ docker push phx.ocir.io/<youraccount>/tt1814190:1
        The push refers to repository [phx.ocir.io/i<youraccount>/tt1814190]
        ae69d21a0796: Pushed
        ...
        1: digest: sha256:0050899c50729f1036ceeff12c78411baed7830e673dccf9542f312ec67545d8 size: 1788
        
You have successfully built the TimesTen container image and pushed it to your chosen container image registry.

## Preparing the configuration metadata files and objects

In order to easily configure the TimesTen instances and database to meet your needs, there are a number of metadata files that can be used to provide configuration information. For example the parameters for the database, wallets to hold the TLS certificates used to encrypt client-server and/or replication traffic, the name and password for a database admin user, a SQL file to initialise the database by creating additional users, tables, indexes and so on.

Full details of the available metadata files can be found in the [documentation](https://docs.oracle.com/database/timesten-18.1/TTKUB/confmeta.htm#TTKUB147). In this sample we will focus on just the following files:

**adminUser** This file should contain the name and password for a database user which will be created and assigned ADMIN privilege. Such a user is mandatory. The file is a text file containing a single line with the format:

    username/password
    
In this example we will use:

    ttadmin/secret123

**db.ini** This text file defines the parameters for the TimesTen database. You can use almost all of the standard TimesTen DSN attributes in this file _except_ for **DataStore** and **LogDir**, as the location for the checkpoint files and transaction logs is controlled by the operator. The name of the generated DSN is the same as the name you use when you create the TimesTenClassic object that will manage the database (more on this later). In this example we will create the db.ini file with the following contents:

    PermSize=512
    TempSize=128
    LogFileSize=128
    LogBufMB=128
    DatabaseCharacterSet=AL32UTF8
    ConnectionCharacterSet=AL32UTF8
    
**schema.sql** This file is optional but we will use it for this example. We will create the file with the following contents:

    create user appuser identified by appuser;
    grant create session, create table to appuser;
    create table appuser.testtab
    (
        pkey      number(8,0) not null primary key,
        col1      varchar2(40)
    );
    insert into appuser.testtab values (1, 'Row 1');

In order for these files to be used when the TimesTen containers are instantiated, we must ensure that these files get placed into the **/ttconfig** directory within the containers. The TimesTen Operator does not mandate any specific way for you to accomplish this, but Kubernetes provides a number of suitable mechanisms such as **init containers**, **config maps** and **secrets**. In this example we will use a config map:

1. Change to the ttimage directory, create a new directory called ttconfig and change to that directory:

        $ cd <my-work-dir>/timesten-operator/ttimage
        $ mkdir ttconfig
        $ cd ttconfig
        
2. Using your favourite text editor, create the three text files **adminUser**, **db.ini** and **schema.sql** with the contents exactly as shown above.

3. Change directory back up to the **ttimage** directory and then create a Kubernetes config map from the contents of the **ttconfig** directory. We will call the config map **sample**:

        $ cd ..
        $ kubectl create configmap sample --from-file=ttconfig
        configmap/sample created
        
4. Display the contents of the config map and verify its contents are correct:

        $ kubectl describe configmap sample
        Name:         sample
        Namespace:    default
        Labels:       <none>
        Annotations:  <none>
        
        Data
        ====
        schema.sql:
        ----
        create user appuser identified by appuser;
        grant create session, create table to appuser;
        create table appuser.testtab
        (
            pkey      number(8,0) not null primary key,
            col1      varchar2(40)
        );
        insert into appuser.testtab values (1, 'Row 1');

        adminUser:
        ----
        ttadmin/secret123

        db.ini:
        ----
        PermSize=512
        TempSize=128
        LogFileSize=128
        LogBufMB=128
        DatabaseCharacterSet=AL32UTF8
        ConnectionCharacterSet=AL32UTF8

        Events:  <none>
        
You will reference this config map when you create the TimesTenClassic object that will define the database deployment.

## Deploying the TimesTenClassic object

Each separate deployment of a TimesTen active-standby pair requires you to create a Kubernetes TimesTenClassic object to describe the required configuration to the TimesTen Operator. The format and content of a TimesTenClassic object is defined by the TimesTen Operator's Custom Resource Definition (CRD), which we created earlier.

In this example we will create a TimesTenClassic object named **sample** (which will create a database named **sample**). To do this we will create a text file named **sample.yaml** with the following contents:

    apiVersion: timesten.oracle.com/v1
    kind: TimesTenClassic
    metadata:
      name: sample
    spec:
      ttspec:
        storageClassName: oci
        storageSize: 50G
        image: phx.ocir.io/<youraccount>/tt1814190:1
        imagePullSecret: ocirsecret
        dbConfigMap:
        - sample

1. Change directory to the deploy directory:

        $ cd <my-work-dir>/timesten-operator/deploy
        
2. Using your favorite text editor, create a text file named **sample.yaml** with the contents as shown above (substitite the name of your registry pull secret and the registry image info for the values shown).

3. Initiate the deployment of the TimesTen active-standby pair and database by creating the TimesTenClassic object named **sample** using the file you just created:

        $ kubectl create -f sample.yaml
        timestenclassic.timesten.oracle.com/sample created
  
Now we need to monitor the deployment and check that everything was successfully created.

## Monitoring deployment and general status checks

Depending on the cluster hardware etc. the process of deploying the active-standby pair and database will take a little while. As the process progresses we can monitor it to see that all is well and to determine when it has finished.

1. Check the high level status:

        $ kubectl get timestenclassic sample
        NAME     STATE          ACTIVE   AGE
        sample   Initializing   None     26s
        
2. Check the detailed status:

        $ kubectl describe timestenclassic sample
        Name:         sample
        Namespace:    default
        Labels:       <none>
        Annotations:  <none>
        API Version:  timesten.oracle.com/v1
        Kind:         TimesTenClassic
        Metadata:
        Creation Timestamp:  2021-09-16T08:29:58Z
        ...
        Normal  Info         3s    timesten  Pod sample-0 RepScheme Exists
        Normal  StateChange  3s    timesten  Pod sample-0 RepState ACTIVE
        Normal  Info         3s    timesten  Pod sample-0 RepAgent Running
        Normal  Info         2s    timesten  Pod sample-1 Daemon Up
        
You can repeat these commands occasionally until the deployment has completed; the 'get' command shows the state as **Normal** and the active as **sample-0**. Once the deployment has completed you can look at the different aspects of the running environment and the Kubernetes objects that have been created.

    $ kubectl get pods
    NAME                                      READY   STATUS    RESTARTS   AGE
    sample-0                                  2/2     Running   0          18m
    sample-1                                  2/2     Running   0          18m
    timestenclassic-operator-d847f5cd-mp7dr   1/1     Running   0          16h
    
    $ kubectl get statefulset sample
    NAME     READY   AGE
    sample   2/2     16m

    $ kubectl get service sample
    NAME     TYPE        CLUSTER-IP   EXTERNAL-IP   PORT(S)    AGE
    sample   ClusterIP   None         <none>        6625/TCP   17m

    $ kubectl get pvc
    NAME                     STATUS   VOLUME                                                                              CAPACITY   ACCESS MODES   STORAGECLASS   AGE
    tt-persistent-sample-0   Bound    ocid1.volume.oc1.phx.abyhqljtio2fl2guo4tqgre56oqqsglbmknfu32j5urnh56mwnqacnclkchq   50Gi       RWO            oci            16h
    tt-persistent-sample-1   Bound    ocid1.volume.oc1.phx.abyhqljtsfd52rzawqaa4xcdwdiwrloujapr3ah5twy2lnse4lok6cnotu3q   50Gi       RWO            oci            16h
    
## Verify the Active and Standby databases

1. Check which pod is currently hosting the active database:

        $ kubectl get ttc sample
        NAME     STATE    ACTIVE     AGE
        sample   Normal   sample-0   22m
        
     You can see that pod **sample-0** is currently hosting the **active** database. Note that **ttc** is a synonym for **timestenclassic** and can save quite a bit of typing.
     
2. Connect to that pod and switch to the user **oracle**

        $ kubectl exec -it sample-0 -c tt -- /usr/bin/su - oracle
        [oracle@sample-0 ~]$
        
3. Execute some basic commands:

        [oracle@sample-0 ~]$ pwd
        /tt/home/oracle
        [oracle@sample-0 ~]$ ls
        csWallet   installation  replicationWallet  start_host109.log  start_host_init12.log  start_host_init6.log
        datastore  instances     start_host102.log  start_host95.log   start_host_init17.log
        
4. Connect to the database as the application user that we defined, check what tables are present, their contents and the database role:

        [oracle@sample-0 ~]$ ttIsql -connStr "DSN=sample;uid=appuser;pwd=appuser"
         
        Copyright (c) 1996, 2021, Oracle and/or its affiliates. All rights reserved.
        Type ? or "help" for help, type "exit" to quit ttIsql.

        connect "DSN=sample;uid=appuser;pwd=********";
        Connection successful: DSN=sample;UID=appuser;DataStore=/tt/home/oracle/datastore/sample;DatabaseCharacterSet=AL32UTF8;ConnectionCharacterSet=AL32UTF8;AutoCreate=0;LogFileSize=128;LogBufMB=128;PermSize=512;TempSize=128;DDLReplicationLevel=3;ForceDisconnectEnabled=1;
        (Default setting AutoCommit=1)
        Command> tables;
         APPUSER.TESTTAB
        1 table found.
        Command> select * from testtab;
        < 1, Row 1 >
        1 row found.
        Command> call ttRepStateGet;
        < ACTIVE >
        1 row found.
        Command>

5. Insert a new row into the **TESTAB** table. Note that the connection is in 'autocommit' mode so there is no need to commit the insert. Exit from ttIsql:

        Command> insert into testtab values (2, 'Row 2');
        1 row inserted.
        Command> select * from testtab;
        < 1, Row 1 >
        < 2, Row 2 >
        2 rows found.
        Command> quit
        Disconnecting...
        Done.
        [oracle@sample-0 ~]$
 
6. Disconnect from the pod:

        [oracle@sample-0 ~]$ exit
        logout

7. Connect to the pod hosting the standby database (**sample-1**), become the user oracle and connect to the database:

        $ kubectl exec -it sample-1 -c tt -- /usr/bin/su - oracle
        [oracle@sample-1 ~]$ ttIsql -connStr "DSN=sample;uid=appuser;pwd=appuser"
        
        Copyright (c) 1996, 2021, Oracle and/or its affiliates. All rights reserved.
        Type ? or "help" for help, type "exit" to quit ttIsql.

        connect "DSN=sample;uid=appuser;pwd=********";
        Connection successful: DSN=sample;UID=appuser;DataStore=/tt/home/oracle/datastore/sample;DatabaseCharacterSet=AL32UTF8;ConnectionCharacterSet=AL32UTF8;AutoCreate=0;LogFileSize=128;LogBufMB=128;PermSize=512;TempSize=128;DDLReplicationLevel=3;ForceDisconnectEnabled=1;
        (Default setting AutoCommit=1)
        Command>

8. Check that the TESTTAB table exists, verify its contents and then try to insert a new row:

        Command> tables;
         APPUSER.TESTTAB
        1 table found.
        Command> select * from testtab;
        < 1, Row 1 >
        < 2, Row 2 >
        2 rows found.
        Command> insert into testtab values  (3, 'Row 3');
        16265: This database is currently the STANDBY.  Change to APPUSER.TESTTAB not permitted.
        The command failed.
        Command> quit
        Disconnecting...
        Done.
        [oracle@sample-1 ~]$ 
        
9. Disconnect from the pod:

        oracle@sample-1 ~]$ exit
        logout

## Trigger a database failover and recovery

The TimesTen Kubernetes Operator provides automatic monitoring and management for TimesTen Active-Standby HA pairs. If the standby pod fails then it will be automatically recreated and the standby database will be reinstantiated and resync'd from the active.

Similarly if the active database fails, the standby will be promoted to active and then the failed active will be recovered as the new standby. Let's take a look at this latter scenario.

1. Kill the pod currently hosting the active database:

        $ kubectl get ttc sample
        NAME     STATE    ACTIVE     AGE
        sample   Normal   sample-0   45m
        
        $ kubectl delete pod sample-0
        pod "sample-0" deleted
        
2. Observe as the operator performs the necessary actions to promote the standby and recover the failed database as the new standby by repeating the following command until the failover and recovery is completed:

        $ kubectl describe ttc sample
        Name:         sample
        Namespace:    default
        Labels:       <none>
        Annotations:  <none>
        API Version:  timesten.oracle.com/v1
        Kind:         TimesTenClassic
        Metadata:
        Creation Timestamp:  2021-09-16T08:29:58Z
        ...
          Warning  StateChange  78s   timesten  TimesTenClassic was Normal, now ActiveDown
          Normal   StateChange  78s   timesten  Pod sample-0 is Not Ready
          Normal   StateChange  72s   timesten  Pod sample-1 is Not Ready
          Normal   StateChange  72s   timesten  TimesTenClassic was ActiveDown, now ActiveTakeover
          Normal   Info         72s   timesten  Pod sample-1 Database Updatable
          Normal   StateChange  72s   timesten  Pod sample-1 RepState ACTIVE
          Normal   Info         66s   timesten  Pod sample-0 RepScheme Unknown
          Normal   Info         66s   timesten  Pod sample-0 Instance Unknown
          Normal   Info         66s   timesten  Pod sample-0 Daemon Unknown
          Normal   Info         66s   timesten  Pod sample-0 CacheAgent Unknown
          Normal   Info         66s   timesten  Pod sample-0 Instance Exists
          Normal   Info         66s   timesten  Pod sample-0 Agent Up
          Normal   Info         66s   timesten  Pod sample-0 RepAgent Unknown
          Normal   StateChange  66s   timesten  Pod sample-0 RepState Unknown
          Normal   Info         66s   timesten  Pod sample-0 Database Unknown
          Normal   Info         66s   timesten  Pod sample-0 Daemon Down
          Normal   StateChange  65s   timesten  Pod sample-1 is Ready
          Normal   StateChange  65s   timesten  TimesTenClassic was ActiveTakeover, now StandbyDown
          Normal   Info         60s   timesten  Pod sample-0 Daemon Up
          Normal   Info         60s   timesten  Pod sample-0 Database Unloaded
          Normal   Info         53s   timesten  Pod sample-0 Database None
          Normal   Info         25s   timesten  Pod sample-0 CacheAgent Not Running
          Normal   Info         25s   timesten  Pod sample-0 Database Loaded
          Normal   Info         25s   timesten  Pod sample-0 RepScheme Exists
          Normal   Info         25s   timesten  Pod sample-0 Database Not Updatable
          Normal   Info         25s   timesten  Pod sample-0 RepAgent Not Running
          Normal   StateChange  25s   timesten  Pod sample-0 RepState IDLE
          Normal   StateChange  19s   timesten  TimesTenClassic was StandbyDown, now Normal
          Normal   StateChange  19s   timesten  Pod sample-0 RepState STANDBY
          Normal   Info         19s   timesten  Pod sample-0 RepAgent Running
          Normal   StateChange  19s   timesten  Pod sample-0 is Ready
        
3. Verify the state:

        $ kubectl get ttc sample
        NAME     STATE    ACTIVE     AGE
        sample   Normal   sample-1   56m
        
## Create and configure a client pod

TimesTen provides a high performance, low overhead, local connectivity mechanism known as direct mode, so it is very common for applications to run on the same host as the TimesTen database in order to benefit from this mechanism.  In a Kubernetes environment this mechanism can also be used by applications running in separate containers from, but within the same pod as, the TimesTen database.

Sometimes applications need to run elsewhere and connect to TimesTen using a more traditional network based client-server mechanism. Let's explore this by creating an additional pod configured for TimesTen client-server connectivity.

1. Change directory to the **deploy** directory:

        $ cd <my-work-dir>/deploy

2. Using your favorite text editor, create a text file named **client.yaml** with the following contents. 

 **Note:** Be sure to use appropriate values for the **image** attributes.
        
        apiVersion: v1
        kind: Pod
        metadata:
          name: client
          labels:
            name: client
        spec:
          imagePullSecrets:
          - name: ocirsecret
          initContainers:
          - name: init
            image: phx.ocir.io/<youraccount>/tt1814190:1
            imagePullPolicy: Always
            volumeMounts:
            - name: tt
              mountPath: /tt
            command:
            - sh
            - "-c"
            - |
              /bin/bash <<'EOF'
              yum -y install unzip
              mkdir -p /tt/home/oracle/instances
              mkdir -p /tt/home/oracle/installation
              unzip /home/oracle/*.zip -d /tt/home/oracle/installation
              chown -R oracle:oracle /tt
              exit 0
          containers:
          - name: client
            image: phx.ocir.io/<youraccount>/tt1814190:1
            imagePullPolicy: Always
            volumeMounts:
            - name: tt
              mountPath: /tt
            command:
            - sh
            - "-c"
            - |
              /bin/bash <<'EOF'
              while [ 1 ]
              do
                  sleep 100
              done
              EOF
          volumes:
          - name: tt
            emptyDir: {}

3. Create the pod:

        $ kubectl create -f client.yaml
        pod/client created 
     
4. Wait for the pod to be ready by repeating the following until the pod status is **Running**:

        $ kubectl get pods
        NAME                                      READY   STATUS    RESTARTS   AGE
        client                                    1/1     Running   0          42s
        sample-0                                  2/2     Running   0          22m
        sample-1                                  2/2     Running   0          71m
        timestenclassic-operator-d847f5cd-mp7dr   1/1     Running   0          17h
        
5. Connect to the pod as the user oracle:

        $ kubectl exec -it client -c client -- /usr/bin/su - oracle
        -bash-4.2$ pwd
        /tt/home/oracle
        -bash-4.2$ ls
        installation  instances
        -bash-4.2$ ls installation
        tt18.1.4.19.0
        -bash-4.2$ ls instances
        -bash-4.2$

    You have a TimesTen installation but no instance yet.
    
6. Create a TimesTen client instance named **ttclient** in the **instances** directory:

        -bash-4.2$ installation/tt18.1.4.19.0/bin/ttInstanceCreate -clientonly \
                             -name ttclient -location /tt/home/oracle/instances
        Creating instance in /tt/home/oracle/instances/ttclient ...
        INFO: Mapping files from the installation to /tt/home/oracle/instances/ttclient/install
        The 18.1 Release Notes are located here :
          '/tt/home/oracle/installation/tt18.1.4.19.0/README.html'
        -bash-4.2$ ls instances
        ttclient
        -bash-4.2$
 
7. Set your session's environment to point to the instance, and then change directory to the instance's **conf** directory:

        -bash-4.2$ source instances/ttclient/bin/ttenv.sh
        NOTE: No Java version found in PATH, setting environment for Java version 8

        LD_LIBRARY_PATH set to /tt/home/oracle/instances/ttclient/ttclasses/lib:/tt/home/oracle/instances/ttclient/install/lib:/tt/home/oracle/instances/ttclient/install/ttoracle_home/instantclient_12_1

        PATH set to /tt/home/oracle/instances/ttclient/bin:/tt/home/oracle/instances/ttclient/install/bin:/tt/home/oracle/instances/ttclient/install/ttoracle_home/instantclient_12_1:/tt/home/oracle/instances/ttclient/install/ttoracle_home/instantclient_12_1/sdk:/usr/local/bin:/bin:/usr/bin:/usr/local/sbin:/usr/sbin

        CLASSPATH set to /tt/home/oracle/instances/ttclient/install/lib/ttjdbc8.jar:/tt/home/oracle/instances/ttclient/install/lib/orai18n.jar:/tt/home/oracle/instances/ttclient/install/lib/timestenjmsxla.jar:/tt/home/oracle/instances/ttclient/install/3rdparty/jms1.1/lib/jms.jar:.

        TNS_ADMIN set to

        TIMESTEN_HOME set to /tt/home/oracle/instances/ttclient

        -bash-4.2$ cd $TIMESTEN_HOME/conf
        -bash-4.2$ ls
        snmp.ini  sys.odbc.ini  sys.ttconnect.ini  timesten.conf
        -bash-4.2$
        
8. Using a text editor, edit the sys.odbc.ini file so that the file from **[ODBC Data Sources]** onwards looks as follows:

        [ODBC Data Sources]
        sampleCS=TimesTen 18.1 Client Driver

        [sampleCS]
        TTC_SERVER_DSN=sample
        TTC_SERVER=sample-0.sample.default.svc.cluster.local/6625
        TTC_SERVER2=sample-1.sample.default.svc.cluster.local/6625
        ConnectionCharacterSet=AL32UTF8
        
 **NOTE:** If you are using a Kubernetes namespace other than **default** then be sure to substitute the namespace's name in place of ***default*** in the TTC\_SERVER and TTC\_SERVER2 settings when creating the DSN.

9. Connect to the A/S pair database and verify that you have been automatically connected to the current active database:

        $ ttIsqlCS -connStr "DSN=sampleCS;uid=appuser;pwd=appuser"

        Copyright (c) 1996, 2021, Oracle and/or its affiliates. All rights reserved.
        Type ? or "help" for help, type "exit" to quit ttIsql.

        connect "DSN=sampleCS;uid=appuser;pwd=********";
        Connection successful: DSN=sampleCS;TTC_SERVER=sample-1.sample.default.svc.cluster.local;TTC_SERVER_DSN=sample;UID=appuser;DATASTORE=/tt/home/oracle/datastore/sample;DATABASECHARACTERSET=AL32UTF8;CONNECTIONCHARACTERSET=AL32UTF8;AUTOCREATE=0;LOGFILESIZE=128;LOGBUFMB=128;PERMSIZE=512;TEMPSIZE=128;DDLREPLICATIONLEVEL=3;FORCEDISCONNECTENABLED=1;
        (Default setting AutoCommit=1)
        Command> tables;
          APPUSER.TESTTAB
        1 table found.
        Command> call ttRepStateGet;
        < ACTIVE >
        1 row found.
        Command> call ttHostNameGet;
        < SAMPLE-1 >
        1 row found.
        Command> 
        
10. On your controlling client system, kill the pod is currently hosting the active database (sample-1):

        $ kubectl delete pod sample-1
        pod "sample-1" deleted

11. In your existing session ttIsql session in the client pod, execute the same two ttIsql commands again:

        Command> call ttRepStateGet;
        47137: Connection handle invalid due to client failover
        The command failed.
        Command> call ttRepStateGet;
        < ACTIVE >
        1 row found.
        Command> call ttHostNameGet;
        < SAMPLE-0 >
        1 row found.
        Command> 
        
    Depending on the exact timing, you may or may not see the error shown above.
    
12. Quit ttIsqlCS and disconnect from the client pod.

        Command> quit
        Disconnecting...
        Done.
        [oracle@sample-1 ~]$ exit
        logout
      
You have now configured a TimesTen client instance and a client DSN for connection to the database hosted by the active-standby pair. The client DSN is configured for automatic client connection failover.

## Cleaning up

When you have finished experimenting you can remove all of the Kubernetes objects and resources as follows:

    $ cd <my-work-dir>/deploy
    $ kubectl delete -f client.yaml
    pod "client" deleted
    $ kubectl delete -f sample.yaml
    timestenclassic.timesten.oracle.com "sample" deleted
    $ kubectl delete -f operator.yaml
    deployment.apps "timestenclassic-operator" deleted

## Summary

Oracle TimesTen In-Memory Database is a lightweight, fully featured, relational in-memory database that offers unrivalled performance, simple installation and management, and high performance high-availability. It is well suited to running in containerized environments and, combined with the TimesTen Kubernetes Operator, provides a simple and robust solution for high performance, highly available data management in Kubernetes environments.

## Other sources of information and examples

There are various other sources for information and examples relating to the TimesTen Kubernetes Operator.

* The appendices of the [TimesTen In-memory Database Kubernetes Operator User's Guide](https://docs.oracle.com/database/timesten-18.1/TTKUB/toc.htm)
* A YouTube presentation ([part 1](https://youtu.be/wFA0vcGP03U) and [part 2](https://youtu.be/A3hxwEJGQgg))
* A YouTube demo ([part1](https://youtu.be/L6AKU61AWNo) and [part 2](https://youtu.be/v1oxddxc2pM))



