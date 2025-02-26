 Copyright (c) 2010, 2025, Oracle and/or its affiliates.

# TptbmAS Application Server Demo

This directory contains the source files and build scripts for the TptbmAS demo program. TptbmAS is a multi-user throughput benchmark that demonstrates how to configure and run the Oracle TimesTen In-Memory Database with several different J2EE application servers and object-relational mapping (ORM) frameworks. For application server configurations, TptbmAS uses an EJB 3.0 client-server application architecture. For ORM configurations, a stand-alone application uses the ORM provider's native API or the EJB 3.0 Java Persistence API (JPA) directly.

TptbmAS can be deployed within the following software environments:

J2EE (EJB 3.0) application servers:

- Oracle WebLogic Server 12c (12.1.2.0)

- Oracle GlassFish Server 3

- JBoss Enterprise Application Platform 6.2

- WebSphere Application Server 8.5
  
ORM frameworks:

- JPA with EclipseLink 2

- Hibernate 3

- JPA with Hibernate 3

- Hibernate 4

- Hibernate 6

- JPA with Hibernate 4  

- JPA with OpenJPA 2

## TptbmAS Overview

The TptbmAS demo measures the performance of SQL SELECT, INSERT and UPDATEoperations against a database table called TPTBM. The demo operates at the Java object level by manipulating instances of a class called Tptbm. The application server or ORM framework translates these object operations into corresponding SQL operations.

The Ant build scripts for each unique TptbmAS configuration are located in the root directory. Each build script configures, builds and executes the TptbmAS demo based on the properties of the application server or ORM framework installation. The relevant files include:

**build.properties**  - Each build*.xml script reads values from this file. The file specifies the locations of build dependencies including the location of the TimesTen JDBC driver jar file and the location of the local application server or ORM installation. This file also specifies the common benchmark execution parameters.

**build-weblogic12.xml** - This build script configures an EJB 3.0 client-server application for use with Oracle WebLogic Server 12.1.

**build-jboss6.xml** - This build script configures an EJB 3.0 client-server application for use with JBoss Enterprise Application Platform 6.2.

**build-websphere8.xml** - This build script configures an EJB 3.0 client-server application for use with WebSphere Application Server 8.5.

**build-glassfish3.xml** - This build script configures an EJB 3.0 client-server application for use with Oracle GlassFish Server 3.
                             
**build-jpa-eclipselink2.xml** - This build script configures a stand-alone application that uses the JPA for persistence with EclipseLink as the persistence provider.

**build-hibernate3.xml** - This build script configures a stand-alone application that uses the native Hibernate API for persistence.
 
**build-jpa-hibernate3.xml** - This build script configures a stand-alone application that uses the JPA for persistence with Hibernate as the persistence provider.
                             
**build-hibernate4.xml** - This build script configures a stand-alone application that uses the native Hibernate API for persistence.
 
**build-jpa-hibernate4.xml** - This build script configures a stand-alone application that uses the JPA for persistence with Hibernate as the persistence provider.

**build-hibernate6.xml** - This build script configures a stand-alone application that uses the native Hibernate API for persistence.
                             
**build-jpa-openjpa2.xml** - This build script configures a stand-alone application that uses the JPA for persistence with OpenJPA 2 as the persistence provider.

Detailed instructions for building and running TptbmAS for a specific configuration are included later in this document.

The application source code for TptbmAS is located in the src/com/timesten/tptbmas directory and includes the following files:

**Tptbm.java** - This is the POJO (Plain Old Java Object) class for TptbmAS. An instance of this class corresponds to a unique row in the TPTBM table. Annotations are used to associate properties of the class with the database table.

**TptbmPKey.java** - Each instance of the Tptbm class contains an embedded instance of the TptbmPKey class. TptbmPKey represents the unique ID for a Tptbm object. The TPTBM table has a composite primary key consisting of the VPN_NB and VPN_ID table columns. TptbmPKey is mapped to these key columns through the use of annotations. 

**TptbmSession.java** - TptbmSession is the remote interface used by application server clients to manage instances of the Tptbm class.

**TptbmSessionBean.java** - TptbmSessionBean is a stateless EJB 3.0 session bean that runs in the application server's container. It implements the TptbmSession interface and manages instances of the Tptbm class via the EntityManager API.

**CommonClient.java** - All TptbmAS configurations use a client process to setup and execute the benchmark. CommonClient is the base abstract class from which all concrete client classes inherit. CommonClient implements the high level benchmark setup and execution logic. CommonClient also defines the following abstract methods which are implemented by the concrete client classes using a specific persistence API:
 
- open ()
- close ()
- txnBegin ()
- txnCommit ()
- txnRollback ()
- read ()
- insert ()
- update ()
- deleteAll ()
- populate ()

To understand how a particular TptbmAS client configuration works with             its persistence API, examine the implementation of these methods in the             concrete client classes described below.

**ASClient.java** - Implements an application server client that uses the TptbmSession interface to communicate with a stateless TptbmSessionBean class running on the server. The TptbmSessionBean class uses the EntityManager API for persistence.
                     
**JPAClient.java** - Implements a stand-alone JPA application that uses the EntityManager interface for persistence. 

**HIBClient.java** - Implements a stand-alone Hibernate application that uses the Hibernate native Session interface for persistence. 


Depending on the specific TptbmAS configuration, one or more meta-data configuration files are required to deploy and run the TptbmAS demo. These files are located in the src/META-INF directory.

For configurations that use the Hibernate persistence framework, a TimesTen specific SQL dialect class is located in the config/hibernate6 directory. This class is compiled and used by the TptbmAS benchmark. This class can also be compiled and used independently. See the config/hibernate6/README.md file for additional information.

Additional application server or ORM configuration files that are not packaged within the TptbmAS application jar files are located in the config directory. Examine the detailed instructions below for more information on these files.


## General Steps for Building and Running TptbmAS

To build and run the TptbmAS demo the following steps must be completed:

1. Install Oracle TimesTen IMDB. TimesTen version 22.1

2. Decide which specific TptbmAS configuration to run and then install the corresponding J2EE application server or ORM framework on the same machine as TimesTen. 

3. If necessary, install the Apache Ant build tool. This component is required for execution of the demo build scripts.

4. Configure a TimesTen data source name (DSN) called sampledb. Be sure to set the PermSize and TempSize data source attributes for the sampledb database to values of at least 64MB each. This will ensure that enough space is available to store the data generated by the demo. For information on creating a TimesTen DSN on a particular platform, refer to the Creating TimesTen Data Stores chapter in the Oracle TimesTen In-Memory Database Operations Guide.

   After the sampledb DSN has been created, the TPTBM table must be installed in
   the database under a user account called SCOTT with the password 'tiger'.
   A SQL script in the root directory called tptbm.sql can be used with the
   TimesTen ttIsql utility to do this. 
   
   For example:
   
       ttIsql -connStr "DSN=sampledb" -f tptbm.sql

   This script will drop the TPTBM table and the user account SCOTT, if they exist,
   and then recreate the user account SCOTT and the TPTBM table.
   

5. If necessary, configure the application server or ORM framework to connect to the TimesTen sampledb database. Detailed instructions on this step for a particular configuration are provided below.

6. Configure the relevant properties in the demo's build.properties file based on the installation locations of TimesTen and the application server or ORM framework. These property values are used by the Ant build scripts to build, deploy and run the TptbmAS demo for a particular configuration.

7. Build the TptbmAS application archives using the application server or ORM specific Ant build script. 

   The Ant target called 'package' will compile and assemble the application and
   place the resulting jar file(s) in the dist directory. 

   For example:
   ant -f build-hibernate6.xml package

   Note: To avoid Java class version conflicts build the TptbmAS packages
   using the same JVM used by the application server. This can be done by
   setting the JAVA_HOME environment variable to the location of the JDK
   used by the application server in the shell where the demo Ant build
   scripts are executed.

   To clean the build environment by removing the build and dist directories
   execute the Ant target called 'clean'.

   For example:
   
       ant -f build-hibernate6.xml clean 

8. For most application server configurations, an additional 'package' task must be executed in the Ant build script in order to compile and package the resulting jar file(s) in the dist directory.

   For example:
   
       ant -f build-hibernate6.xml package

9. Run the TptbmAS demo using the same Ant build script. All configurations use the 'run' Ant task to do this.

   For example:
   
       ant -f build-hibernate6.xml run

   The 'run' Ant task in the build script can be modified to change command line execution parameters for the demo.


Detailed instructions for each supported application server and ORM framework configuration are provided below.

## Hibernate 6 Configuration

This configuration runs the TptbmAS demo with the Hibernate 6 ORM framework.

Tested versions:  

Hibernate version 6.6.0.Final

Build script:    

build-hibernate6.xml

Required environment variables:

- TIMESTEN_HOME: Set this environment variable by sourcing the ttenv shell script (ttenv.sh or ttenv.csh). The script is located in the bin directory of the TimesTen instance.
- HIBERNATE_HOME_DIR: Environment variable pointing to the directory that contains all the required Java '*.jar' dependencies.

JDK versions supported: 17, 21
                                
Source files used by this configuration:

````
src/META-INF/Tptbm.hbm.xml
src/META-INF/hibernate.cfg.xml
src/com/timesten/tptbmas/CommonClient.java
src/com/timesten/tptbmas/HIBClient.java
src/com/timesten/tptbmas/Tptbm.java
src/com/timesten/tptbmas/TptbmPKey.java
config/hibernate6/TimesTenDialect.java
config/hibernate6/TimesTenSqlAstTranslator.java
config/hibernate6/pagination/TimesTenLimitHandler.java
config/hibernate6/sequence/TimesTenSequenceSupport.java
````

Configuration notes:

1. Create a directory for the *.jar dependencies for Hibernate 6, <hibernate_home_dir>. 
   Then, download the dependencies to <hibernate_home_dir> using 'wget'.
````
  mkdir <hibernate_home_dir>
  wget -P <hibernate_home_dir> https://repo.maven.apache.org/maven2/org/hibernate/orm/hibernate-core/6.6.0.Final/hibernate-core-6.6.0.Final.jar
  wget -P <hibernate_home_dir> https://repo1.maven.org/maven2/jakarta/persistence/jakarta.persistence-api/3.2.0/jakarta.persistence-api-3.2.0.jar
  wget -P <hibernate_home_dir> https://repo1.maven.org/maven2/org/jboss/logging/jboss-logging/3.6.1.Final/jboss-logging-3.6.1.Final.jar
  wget -P <hibernate_home_dir> https://repo1.maven.org/maven2/net/bytebuddy/byte-buddy/1.16.1/byte-buddy-1.16.1.jar
  wget -P <hibernate_home_dir> https://repo1.maven.org/maven2/org/antlr/antlr4-runtime/4.13.2/antlr4-runtime-4.13.2.jar
  wget -P <hibernate_home_dir> https://repo1.maven.org/maven2/com/fasterxml/classmate/1.7.0/classmate-1.7.0.jar
  wget -P <hibernate_home_dir> https://repo1.maven.org/maven2/jakarta/xml/bind/jakarta.xml.bind-api/4.0.2/jakarta.xml.bind-api-4.0.2.jar
  wget -P <hibernate_home_dir> https://repo1.maven.org/maven2/org/hibernate/common/hibernate-commons-annotations/7.0.3.Final/hibernate-commons-annotations-7.0.3.Final.jar
  wget -P <hibernate_home_dir> https://repo1.maven.org/maven2/jakarta/activation/jakarta.activation-api/2.1.3/jakarta.activation-api-2.1.3.jar
  wget -P <hibernate_home_dir> https://repo1.maven.org/maven2/jakarta/transaction/jakarta.transaction-api/2.0.1/jakarta.transaction-api-2.0.1.jar
  wget -P <hibernate_home_dir> https://repo1.maven.org/maven2/com/sun/xml/bind/jaxb-core/4.0.5/jaxb-core-4.0.5.jar
  wget -P <hibernate_home_dir> https://repo1.maven.org/maven2/org/glassfish/jaxb/jaxb-runtime/4.0.5/jaxb-runtime-4.0.5.jar
````

2. Set the HIBERNATE_HOME_DIR environment variable to the absolute path of <hibernate_home_dir>:
````
  export HIBERNATE_HOME_DIR=<hibernate_home_dir>
````

3. Note that this configuration uses the included Hibernate dialect class called TimesTenDialect. It has been optimized for SQL features in TimesTen 22.1. This dialect is not included in current versions of the Hibernate distribution. The TimesTenDialect class is compiled and packaged into the tptbmas-hibernate6.jar file. This is the recommended dialect for applications that use TimesTen 22.1.1 or later.
   
4. To build and run the demo, run:

````
ant -f build-hibernate6.xml clean
ant -f build-hibernate6.xml package
ant -f build-hibernate6.xml run
````


Note: The rest of the document is for historical, old releases only.

## Oracle WebLogic Server 12.1 Configuration

This configuration runs the EJB 3.0 client-server version of the TptbmAS demo under Oracle WebLogic Server 12.1.

Tested versions:

  Oracle WebLogic Server 12c (12.1.2.0)

Build script:    

  build-weblogic12.xml

Required properties in build.properties:

**weblogic.home.dir** - Set this to the root installation directory of the local WebLogic server installation.

**weblogic.domain.dir** - The location of the domain directory for the server hosting the TptbmAS application.

**weblogic.deploy.dir** - The directory where application archives can be copied and auto deployed to the server.

**weblogic.host** - The host name of the machine where the server is installed.

**weblogic.port** - The RMI port number used by external clients to communicate with the server, the default port number is 7001.

Server side configuration files:

  config/weblogic12/TptbmDS-jdbc.xml

Source files used by this configuration:

    src/META-INF/persistence-weblogic12.xml
    src/com/timesten/tptbmas/CommonClient.java
    src/com/timesten/tptbmas/ASClient.java
    src/com/timesten/tptbmas/Tptbm.java
    src/com/timesten/tptbmas/TptbmPKey.java
    src/com/timesten/tptbmas/TptbmSession.java
    src/com/timesten/tptbmas/TptbmSessionBean.java


Configuration notes:

1. Install Oracle WebLogic Server 12.1 and then set the required properties listed above in build.properties based on the installation.

2. The server will require access to the TimesTen JDBC driver jar file. This can be done by setting the CLASSPATH environment variable to the location of the TimesTen JDBC driver jar file in the environment where the server starts. Alternatively, the WebLogic domain environment configuration script located at ${weblogic.domain.dir}/bin/setDomainEnv.sh (or setDomainEnv.cmd on Windows) can be used to set the CLASSPATH for the server. 

3. The server will also need access to the TimesTen shared libraries. On Unix platforms this can be done by setting the LD_LIBRARY_PATH environment variable (or the equivalent variable for your OS) to the location of the TimesTen installation's lib directory in the environment where the server starts. On Windows platforms the PATH environment variable should be set to the location of the TimesTen installation's bin directory where TimesTen DLLs are located. Alternatively, the WebLogic domain environment configuration script located at ${weblogic.domain.dir}/bin/setDomainEnv.sh (or setDomainEnv.cmd on Windows) can be used to set these variables for the server.  

4. Before the demo can be deployed or executed, a TimesTen data source with the JNDI name of jdbc/TptbmDS must be configured on the server. A sample data source configuration file is provided by the demo at config/weblogic12/TptbmDS-jdbc.xml. This file can be copied to the \${weblogic.domain.dir}/config/jdbc directory. In order for this data source configuration to be read by the server the following section must be inserted into the <domain> section of the \${weblogic.domain.dir}/config/config.xml file:

````
<jdbc-system-resource>
 <name>TptbmDS</name>
 <target>AdminServer</target>
 <descriptor-file-name>jdbc/TptbmDS-jdbc.xml</descriptor-file-name>
</jdbc-system-resource>
````

   Note that 'AdminServer' should be replaced with the name of the target server in the configuration above.

   The server should be restarted after these changes. The server will read the new configuration when it starts up and then add the data source to its JNDI tree.

5. After the server has been configured to use the jdbc/TptbmDS data source and restarted, the demo can be built, deployed, and executed with this sequence of Ant tasks:

````
ant -f build-weblogic12.xml clean
ant -f build-weblogic12.xml package
ant -f build-weblogic12.xml deploy
ant -f build-weblogic12.xml run
````

## JBoss Enterprise Application Platform 6.2

This configuration runs the EJB 3.0 client-server version of the TptbmAS demo under JBoss Enterprise Application Platform 6.2.

Tested versions:

JBoss EAP 6.2.0.GA

Build script:

build-jboss6.xml

Required properties in build.properties:

**jboss.home.dir** - Set this to the root installation directory of the local JBoss EAP installation.

**jboss.server.dir** - The location of the JBoss EAP directory for the server hosting the Tptbm entity bean.

**jboss.deploy.dir** - The directory where application archives can be copied and auto deployed to the JBoss EAP.

**jboss.host** - The host name of the machine where the server is installed.

**jboss.port** - The RMI port number used by external clients to communicate with the server, the default port number is 4447.

Client side configuration files:

config/jbosseap6/jboss-ejb-client.properties

Source files used by this configuration:

````
src/META-INF/persistence-jboss6.xml
src/com/timesten/tptbmas/CommonClient.java
src/com/timesten/tptbmas/ASClient.java
src/com/timesten/tptbmas/Tptbm.java
src/com/timesten/tptbmas/TptbmPKey.java
src/com/timesten/tptbmas/TptbmSession.java
src/com/timesten/tptbmas/TptbmSessionBean.java
config/hibernate4/TimesTenDialect1122.java
````

Configuration notes:

1. Install JBoss EAP 6 and then set the required properties listed above in build.properties based on the installation.

2. Note that JBoss Application Server uses Hibernate as its internal persistence provider. Therefore this configuration of TptbmAS uses the included Hibernate dialect class called TimesTenDialect1122. It has been optimized for SQL features in TimesTen version 11.2.2. This dialect is not included in current versions of the Hibernate distribution. The TimesTenDialect1122 class is compiled and packaged into the EJB jar file. This is the recommended dialect for applications that use TimesTen version 11.2.2 or greater.
   
3. JBoss RMI clients require that a configuration file called jboss-ejb-client.properties exist in the client's CLASSPATH. The demo includes this configuration file in the config/jbosseap6 directory. If the demo is executed on a different machine than the JBoss server then this file will need to be manually edited to reflect the properties of the remote JBoss server.

4. The JBoss server environment will need access to the TimesTen shared libraries. This can be done by setting the LD_LIBRARY_PATH environment variable (or the equivalent variable for your OS) to the location of the TimesTen installation's lib directory. On Windows platforms the PATH environment variable should be set to the location of the TimesTen installation's bin directory where TimesTen DLLs are located. After changing these environment variables the JBoss server should be restarted.
   
5. The server will require access to the TimesTen JDBC driver jar file. In JBoss EAP 6 this can be done by deploying the TimesTen JDBC driver jar file on the application server via the JBoss management console using this sequence of steps:
   
   a. Connect to the JBoss management console. The default web address is http://localhost:9990/console.
      
   b. Select the Runtime tab near the top of the page.
   
   c. Expand the Server node on the left hand menu and select Manage Deployments.
      
   d. On the Deployments page click the Add button and then click Choose File in the resulting dialog box.
      
   e. Navigate to the location of the TimesTen JDBC drive jar file. If JBoss server is using JDK 1.7 then the ttjdbc7.jar version of the driver jar file located in the TimesTen installation's lib subdirectory should be selected. If JBoss server is using JDK 1.6 then select the ttjdbc6.jar file.
      
   f. Click the Next button in the Create Deployment dialog.
   
   g. Click the Save button in the Create Deployment dialog.
   
   h. On the Deployments page a new deployment called ttjdbc7.jar or ttjdbc6.jar should appear. Click the En/Disable button and then click Confirm to enable the TimesTen driver.


6. Before the demo can be deployed or executed, a TimesTen data source with the JNDI name of java:/TptbmDirectDS must be configured on the server. This can be done by configuring the data source on the application server via the JBoss management console using this sequence of steps:
   
   a. Connect to the JBoss management console. The default web address is http://localhost:9990/console.
      
   b. Select the Profile tab at the top of the page.  
   
   c. Expand the Subsytems and Connector nodes on the left hand menu and then select Datasources.
      
   d. Click the Add button.
   
   e. In the Datasource Attributes dialog box enter a name and a JNDI name for the new data source. The name should be set to 'TptbmDirectDS' and the JNDI name should be set to 'java:/TptbmDirectDS'.
  
   f. Click the Next button.
   
   g. In the JDBC Driver dialog select the TimesTen JDBC driver deployment called ttjdbc7.jar or ttjdbc6.jar.
      
   h. Click Next.
   
   i. In the Connection Settings dialog box, enter the TimesTen JDBC URL and TimesTen database user credentials for the connection. The URL should be set to 'jdbc:timesten:direct:SampleDb_1211'. The user should be set to 'scott' and the password 'tiger'.
      
   j. Click Done.
   
   k. At the JDBC Datasources screen click the Enable button and then click Confirm to enable the data source.
      
   l. To verify that the data source can connect to TimesTen click the Connection link and then click the Test Connection button.
   
7. After the server has been configured to use the java:/TptbmDirectDS data source and restarted, the demo can be built, deployed, and executed with this sequence of Ant tasks:

````
ant -f build-jboss6.xml clean
ant -f build-jboss6.xml package
ant -f build-jboss6.xml deploy
ant -f build-jboss6.xml run 
````

## Oracle GlassFish Server 3 Configuration

This configuration runs the EJB 3.0 client-server version of the TptbmAS demo under Oracle GlassFish Server 3.

Tested versions:

Oracle GlassFish Server 3.0.1
Oracle GlassFish Server 3.1.2.2

Build script:

build-glassfish3.xml

Required properties in build.properties:

**glassfish.home.dir** - Set this to the root installation directory of the local GlassFish server installation.

**glassfish.domain.dir** - The location of the domain directory for the server hosting the Tptbm application.

**glassfish.deploy.dir** - The directory where application archives can be copied and auto deployed to the server.
                              
**glassfish.host** - The host name of the machine where the server is installed.

**glassfish.orb.port** - The RMI port number used by external clients to communicate with the server, the default port number is 3700.

Source files used by this configuration:

````
src/META-INF/persistence-glassfish3.xml
src/com/timesten/tptbmas/CommonClient.java
src/com/timesten/tptbmas/ASClient.java
src/com/timesten/tptbmas/Tptbm.java
src/com/timesten/tptbmas/TptbmPKey.java
src/com/timesten/tptbmas/TptbmSession.java
src/com/timesten/tptbmas/TptbmSessionBean.java
````

Configuration notes:

1. Install Oracle GlassFish Server 3 and then set the required properties listed above in build.properties based on the installation.

2. The server will require access to the TimesTen JDBC driver jar file. This can be done by copying the TimesTen JDBC driver jar file from the TimesTen installation's lib directory into the ${glassfish.domain.dir}/lib directory.

3. The server will also need access to the TimesTen shared libraries. This can be done by setting the LD_LIBRARY_PATH environment variable (or the equivalent variable for your OS) to the location of the TimesTen installation's lib directory. On Windows platforms the PATH environment variable should be set to the location of the installation's bin directory where TimesTen DLLs are located.
   
4. Before the demo can be deployed and executed, a TimesTen connection pool and a JDBC resource with the JNDI name of jdbc/TptbmDS must be configured on the server. Follow these steps.

   a. Connect to the GlassFish Server administration console. By default a local server's administration console can be accessed at http://localhost:4848.

   b. Select Resources->JDBC->Connection Pools from the left hand pane. Click the 'New...' button.

   c. At the 'New JDBC Connection Pool (Step 1 of 2)' screen enter 'TptbmPool' in the 'Name' field. In the 'Resource Type' drop down list box select 'javax.sql.ConnectionPoolDataSource'. In the 'Database Vendor' field enter 'Oracle TimesTen'. Click 'Next'.

   d. At the 'New JDBC Connection Pool (Step 2 of 2)' screen enter 'com.timesten.jdbc.ObservableConnectionDS' in the 'Datasource Classname' field. Scroll down to the 'Additional Properties' section at the bottom of the screen. Click the 'Add Property' button. A new row will appear. Click the check box in the new row. Type 'url' in the 'Name' column. In the 'Value' column enter 'jdbc:timesten:direct:sampledb'. Click the check box for the 'password' property and in the 'Value' column type 'tiger'. Click the check box for the 'user' property and in the 'Value' column type 'SCOTT'. Click the 'Finish' button.

   e. Select Resources->JDBC->JDBC Resources from the left hand pane. Click the 'New...' button.

   f. At the 'New JDBC Resource' screen enter 'jdbc/TptbmDS' in the 'JNDI name' field. In the 'Pool Name' field select 'TptbmPool'. Click 'OK'. 
      
   g. The server should be restarted in order for the configuration changes to take effect.

5. After the server has been configured to use the jdbc/TptbmDS data source and restarted, the demo can be built, deployed, and executed with this sequence of Ant tasks:

````
ant -f build-glassfish3.xml clean
ant -f build-glassfish3.xml package
ant -f build-glassfish3.xml deploy
ant -f build-glassfish3.xml run
````

## WebSphere Application Server 8.5 Configuration

This configuration runs the EJB 3.0 client-server version of the TptbmAS demo under WebSphere Application Server 8.5.

Tested versions:  

IBM WebSphere Application Server 8.5.5.1

Build script:    

build-websphere8.xml

Required properties in build.properties:

**websphere.home.dir** - Set this to the home (WAS_HOME) installation directory of the local WebSphere server installation.

**websphere.profile.dir** - Set this to the profile directory for the WebSphere server that will deploy the Tptbm EJB.

**websphere.cell** - Set this to the the name of the WebSphere cell that will run the Tptbm EJB.

**websphere.host** - The name of the host machine running the  WebSphere application server.

**websphere.port** - The RMI port number used by clients to communicate with the server, the default port number is 2809.

Source files used by this configuration:

````
src/META-INF/persistence-websphere8.xml
src/com/timesten/tptbmas/CommonClient.java
src/com/timesten/tptbmas/ASClient.java
src/com/timesten/tptbmas/Tptbm.java
src/com/timesten/tptbmas/TptbmPKey.java
src/com/timesten/tptbmas/TptbmSession.java
src/com/timesten/tptbmas/TptbmSessionBean.java
````

Configuration notes:

1. Install WebSphere Application Server 8.5 and then set the required properties listed above in build.properties based on the installation. 

2. Before the demo can be deployed or executed, a TimesTen data source with the JNDI name of jdbc/TptbmDS must be configured on the server. This can be done from a web browser by configuring a JDBC Provider resource for TimesTen within the WebSphere Administrative Console. Follow these steps to configure the TimesTen data source.

   a. Login to the WebSphere Administrative Console from a web browser. The default URL for this is 'http://localhost:9060/ibm/console'.
   
   b. Select Resources->JDBC->JDBC Providers from the left hand panel.

   c. Select a scope from the drop down list box.

   d. Click the New button to create a new JDBC Provider. In the 'Database type' list box select 'User-defined'. In the 'Implementation class name' field specify 
     'com.timesten.jdbc.ObservableConnectionDS'. In the 'Name' field type 'TimesTen JDBC Provider'. Click the Next button.

   d. In the 'Class path' field specify the complete path to the TimesTen JDBC driver jar file, for example:
   
      C:\TimesTen\tt1122_64\lib\ttjdbc7.jar
      
      Click the Next button and then click the Finish button at the summary screen.

   e. Click the name of the TimesTen JDBC provider that was just created. In the 'Native library path' field specify the directory where the TimesTen shared libraries are located, for example: 'C:\TimesTen\tt1122_64\bin'. Click the Apply button. Click the Save link to save the configuration.

   f. Select Resources->JDBC->Data sources from the left hand panel.

   g. Select a scope from the drop down list box.

   h. Click the New button to create a new data source. In the 'Data source name' field type 'TptbmDS'. In the 'JNDI name' field type 'jdbc/TptbmDS'. Click Next. Click the 'Select an existing JDBC provider' check box and then select the name of the TimesTen JDBC provider. Click Next. Make sure that the 'Use this Data Source in container managed persistence' check box is checked. In the 'Data store helper class name' field 'com.ibm.websphere.rsadapter.GenericDataStoreHelper' should be specified. Click the Next button. Click the next button again on the 'Setup security aliases' step. Click the Finish button at the summary screen.

   i. Click on the 'TptbmDS' data source link. Click on the 'Custom properties' link. Click on the 'url' property link. In the 'Value' field type 'jdbc:timesten:direct:sampledb'. Click the OK button. Click on the 'user' property link. This property may be located on the next page. In the 'Value' field type 'SCOTT'. Click the OK button. Click on the 'password' property link. In the 'Value' field type 'tiger'. Click the OK button. Click on the 'webSphereDefaultIsolationLevel' property link. In the 'Value' field type '2' for READ_COMMITTED. Click the OK button.

   j. At the top of the page click on the link called 'Save' to save the server's configuration for this data source.

   k. At this point the jdbc/TptbmDS data source has been configured. 


3. After the server has been configured to use the jdbc/TptbmDS data source and restarted, the demo can be built with this sequence of Ant tasks:

````
ant -f build-websphere8.xml clean
ant -f build-websphere8.xml package
````

   These steps will result in a deployable archive file located in the dist directory called tptbmas-ejb3.jar.

   Note: To avoid Java class version conflicts, build the TptbmAS packages using the same JVM used by the application server. This can be done by setting the JAVA_HOME environment variable to the location of the JDK used by the application server in the shell where the demo ant scripts are executed.

4. To deploy the archive to the server login to the WebSphere Administrative Console and follow these steps:

   a. Select Applications->New Application from the left hand panel.
   
   b. Click on the 'New Enterprise Application' link.

   c. Click the 'Local file system' option and then click the Choose File button. Navigate to the dist directory where the tptbmas-ejb3.jar file was created in step 3 above. Select the JAR file and click Open. Click Next.

   d. At the 'Preparing for the application installation' step select the 'Fast Path' option. Click on the 'Choose to generate default bindings...' label and then check the 'Generate Default Bindings' option. Click Next.
     
   e. At the 'Select installation options' step click Next.

   f. At the 'Map modules to servers' step click Next.
   
   g. At the 'Meatadata for modules' step click Next.

   h. In the 'Summary' step click the Finish button to install the Tptbm EJB. When the installation process completes click the Save link at the bottom of the page to save the configuration.

   i. Select Applications->Application Types->WebSphere enterprise applications from the left hand panel.

   j. Click on the check box next to the 'tptbmas-ejb3_jar' application. Click the Start button.


5. To run the client execute this command:

````
   ant -f build-websphere8.xml run
````

## EclipseLink 2 Configuration

This configuration runs the TptbmAS demo with the EclipseLink 2 ORM framework.

Tested versions:  

EclipseLink 2.0.2
EclipseLink 2.1.1
EclipseLink 2.4.0
EclipseLink 2.5.1
  
Build script:    

build-jpa-eclipselink2.xml

Required properties in build.properties:

**timesten.jdbc.driver.jar** - Set this to the location of the TimesTen JDBC driver jar file.

**eclipselink.home.dir** - The location of the local EclipseLink home directory.

Source files used by this configuration:

````
src/META-INF/persistence-eclipselink2.xml
src/com/timesten/tptbmas/CommonClient.java
src/com/timesten/tptbmas/JPAClient.java
src/com/timesten/tptbmas/Tptbm.java
src/com/timesten/tptbmas/TptbmPKey.java
````

Configuration notes:

1. Install EclipseLink 2 and then set the required properties listed above in build.properties based on the installation.

2. To build and run the demo execute this sequence of Ant tasks:

````
ant -f build-jpa-eclipselink2.xml clean
ant -f build-jpa-eclipselink2.xml package
ant -f build-jpa-eclipselink2.xml run
````

## Hibernate 3 Configuration

This configuration runs the TptbmAS demo with the Hibernate 3 ORM framework.

Tested versions:  

Hibernate version 3.6.0
Hibernate version 3.6.10.Final

Build script:    

build-hibernate3.xml

Required properties in build.properties:

**timesten.jdbc.driver.jar** - Set this to the location of the TimesTen JDBC driver jar file.

**hibernate.home.dir** - The location of the Hibernate 3 home directory, this is used by the build script to place required 3rd party libraries on the CLASSPATH.
                                
**hibernate.jar** - The location of the hibernate3.jar file.

Source files used by this configuration:

````
src/META-INF/Tptbm.hbm.xml
src/META-INF/hibernate.cfg.xml
src/com/timesten/tptbmas/CommonClient.java
src/com/timesten/tptbmas/HIBClient.java
src/com/timesten/tptbmas/Tptbm.java
src/com/timesten/tptbmas/TptbmPKey.java
config/hibernate3/TimesTenDialect1122.java
````

Configuration notes:

1. Install the Hibernate 3 distribution and then set the required properties listed above in build.properties based on the installation.

2. Note that this configuration uses the included Hibernate dialect class called TimesTenDialect1122. It has been optimized for SQL features in TimesTen version 11.2.2. This dialect is not included in current versions of the Hibernate distribution. The TimesTenDialect1122 class is compiled and packaged into the tptbmas-hibernate3.jar file. This is the recommended dialect for applications that use TimesTen version 11.2.2 or greater.
   
3. Newer versions of the Hibernate distribution use the SLF4J logging facade. SLF4J requires at least one of several available log framework adapter jar files that may not be included in the Hibernate distribution. If an SLF4J log adapter jar file is not on the CLASSPATH when running this configuration then the following error may occur:
   
   SLF4J: Failed to load class "org.slf4j.impl.StaticLoggerBinder".
   
   To solve this problem download the SLF4J JDK 1.4 logging adapter jar file for the same version of SLF4J that is included in the Hibernate distribution. SLF4J distributions are available at http://www.slf4j.org/dist/. For example, if the Hibernate distribution includes the slf4j-api-1.5.8.jar file then download the corresponding SLF4J 1.5.8 distribution. Extract the slf4j-jdk14-1.5.8.jar file from the distribution and place it in the same Hibernate directory as the slf4j-api-1.5.8.jar file. Additional information on logging can be found at http://www.slf4j.org/ and in the Hibernate documentation.
   
4. To build and run the demo execute this sequence of Ant tasks:

````
ant -f build-hibernate3.xml clean
ant -f build-hibernate3.xml package
ant -f build-hibernate3.xml run
````

## JPA with Hibernate 3

This configuration runs the TptbmAS demo using the EJB 3.0 EntityManager APIv(JPA) with Hibernate as the persistence provider.

Tested versions:  

Hibernate version 3.6.0
Hibernate version 3.6.10.Final

Build script:    

build-jpa-hibernate3.xml

Required properties in build.properties:

**timesten.jdbc.driver.jar** - Set this to the location of the TimesTen JDBC driver jar file.

**hibernate.home.dir** - The location of the Hibernate 3 home directory, this is used by the build script to place required 3rd party libraries in the CLASSPATH.
                                
**hibernate.jar** - The location of the hibernate3.jar archive.

Source files used by this configuration:

````
src/META-INF/persistence-hibernate.xml
src/com/timesten/tptbmas/CommonClient.java
src/com/timesten/tptbmas/JPAClient.java
src/com/timesten/tptbmas/Tptbm.java
src/com/timesten/tptbmas/TptbmPKey.java
config/hibernate3/TimesTenDialect1122.java
````

Configuration notes:

1. Install the Hibernate 3 distribution and set the required properties listed above in build.properties based on the installation.

2. Note that this configuration uses the included Hibernate dialect called TimesTenDialect1122. This dialect is not included in current versions of the Hibernate distribution. The TimesTenDialect1122 class is compiled and packaged into the tptbmas-jpa-hibernate3.jar file. This is the recommended Hibernate dialect for TimesTen 11.2.2 applications.

3. To build and run the demo execute this sequence of Ant tasks:

````
ant -f build-jpa-hibernate3.xml clean
ant -f build-jpa-hibernate3.xml package
ant -f build-jpa-hibernate3.xml run
````

## Hibernate 4 Configuration

This configuration runs the TptbmAS demo with the Hibernate 4 ORM framework.

Tested versions:  

Hibernate version 4.1.5.SP1
Hibernate version 4.3.1.Final

Build script:    

build-hibernate4.xml

Required properties in build.properties:

**timesten.jdbc.driver.jar** - Set this to the location of the TimesTen JDBC driver jar file.

**hibernate.home.dir** - The location of the Hibernate 4 home directory, this is used by the build script to place required 3rd party libraries on the CLASSPATH.
                                
Source files used by this configuration:

````
src/META-INF/Tptbm.hbm.xml
src/META-INF/hibernate.cfg.xml
src/com/timesten/tptbmas/CommonClient.java
src/com/timesten/tptbmas/HIBClient.java
src/com/timesten/tptbmas/Tptbm.java
src/com/timesten/tptbmas/TptbmPKey.java
config/hibernate4/TimesTenDialect1122.java
````

Configuration notes:

1. Install the Hibernate 4 distribution and then set the required properties listed above in build.properties based on the installation.

2. Note that this configuration uses the included Hibernate dialect class called TimesTenDialect1122. It has been optimized for SQL features in TimesTen version 11.2.2. This dialect is not included in current versions of the Hibernate distribution. The TimesTenDialect1122 class is compiled and packaged into the tptbmas-hibernate4.jar file. This is the recommended dialect for applications that use TimesTen version 11.2.2 or greater.
   
3. To build and run the demo execute this sequence of Ant tasks:

````
ant -f build-hibernate4.xml clean
ant -f build-hibernate4.xml package
ant -f build-hibernate4.xml run
````

## JPA with Hibernate 4

This configuration runs the TptbmAS demo using the EJB 3.0 EntityManager API (JPA) with Hibernate as the persistence provider.

Tested versions:  

Hibernate version 4.1.5.SP1
Hibernate version 4.3.1.Final

Build script:    

build-jpa-hibernate4.xml

Required properties in build.properties:

**timesten.jdbc.driver.jar** - Set this to the location of the TimesTen JDBC driver jar file.

**hibernate.home.dir** - The location of the Hibernate 4 home directory, this is used by the build script to place required 3rd party libraries in the CLASSPATH.

Source files used by this configuration:

````
src/META-INF/persistence-hibernate.xml
src/com/timesten/tptbmas/CommonClient.java
src/com/timesten/tptbmas/JPAClient.java
src/com/timesten/tptbmas/Tptbm.java
src/com/timesten/tptbmas/TptbmPKey.java
config/hibernate4/TimesTenDialect1122.java
````

Configuration notes:

1. Install the Hibernate 4 distribution and set the required properties listed above in build.properties based on the installation.

2. Note that this configuration uses the included Hibernate dialect called TimesTenDialect1122. This dialect is not included in current versions of the Hibernate distribution. The TimesTenDialect1122 class is compiled and packaged into the tptbmas-jpa-hibernate4.jar file. This is the recommended Hibernate dialect for TimesTen 11.2.2 applications.

3. To build and run the demo execute this sequence of Ant tasks:

````
ant -f build-jpa-hibernate4.xml clean
ant -f build-jpa-hibernate4.xml package
ant -f build-jpa-hibernate4.xml run
````

## JPA with OpenJPA 2

This configuration runs the TptbmAS demo using the EJB 3.0 EntityManager API (JPA) with OpenJPA 2 as the persistence provider.

Tested versions:  

OpenJPA version 2.0.1
OpenJPA version 2.2.0
OpenJPA version 2.2.2

Build script:    

build-jpa-openjpa2.xml

Required properties in build.properties:

**timesten.jdbc.driver.jar** - Set this to the location of the TimesTen JDBC driver jar file.

**openjpa.home.dir** - The location of the OpenJPA 2 home directory, this is used by the build script to place required 3rd party libraries in the CLASSPATH.
                                
**openjpa.jar** - The location of the openjpa-all-2.x.x.jar archive.

Source files used by this configuration:

````
src/META-INF/persistence-openjpa2.xml
src/com/timesten/tptbmas/CommonClient.java
src/com/timesten/tptbmas/JPAClient.java
src/com/timesten/tptbmas/Tptbm.java
src/com/timesten/tptbmas/TptbmPKey.java
````

Configuration notes:

1. Install the OpenJPA 2 distribution and set the required properties listed above in build.properties based on the installation.

2. To build and run the demo execute this sequence of Ant tasks:

````
ant -f build-jpa-openjpa2.xml clean
ant -f build-jpa-openjpa2.xml package
ant -f build-jpa-openjpa2.xml run
````
