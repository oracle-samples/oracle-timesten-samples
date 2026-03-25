 Copyright (c) 2025, Oracle and/or its affiliates.

# Hibernate 6 SQL Dialect for TimesTen

This directory contains the source code and build script for a Hibernate SQL dialect class for TimesTen. The class name is TimesTenDialect. This dialect class has been tested with Hibernate 6.6.0 and TimesTen 22.1.

The use of this Hibernate dialect class will help to avoid SQL syntax incompatibilites for Hibernate applications that use TimesTen. 

New features introduced with this dialect:
- Support for Hibernate 6.
- Added support for uid, SESSION_USER, SYSTEM_USER, and CURRENT_USER SQL functions.

Supported JDK versions: 17, 21

To compile and configure the TimesTenDialect classes, follow these steps:

1. Create a directory for the Hibernate 6 Core Jar, <hibernate_home_dir>. Then, download Hibernate 6 Core Jar to <hibernate_home_dir> using 'wget':
````
  mkdir <hibernate_home_dir>
  wget -P <hibernate_home_dir> https://repo.maven.apache.org/maven2/org/hibernate/orm/hibernate-core/6.6.0.Final/hibernate-core-6.6.0.Final.jar
````

2. Set the HIBERNATE_HOME_DIR environment variable to the absolute path of <hibernate_home_dir>:
````
  export HIBERNATE_HOME_DIR=<hibernate_home_dir>
````

3. Compile the TimesTenDialect class with the Apache Ant build tool. Ensure that the directory containing the ant executable is included in the PATH environment variable. 
````
  ant clean
  ant
````

4. Include the 'build/org/hibernate/dialect/' directory into the Hibernate application's CLASSPATH environment variable. This directory is created during the dialect classes compilation.

5. To configure a Hibernate application to use the TimesTenDialect class, set the hibernate.dialect session property to 'org.hibernate.dialect.TimesTenDialect'. The example hibernate.cfg.xml configuration file, shown below, illustrates the configuration of a Hibernate session that uses the TimesTenDialect class.

```text
<?xml version='1.0' encoding='utf-8'?>
<!DOCTYPE hibernate-configuration PUBLIC
    "-//Hibernate/Hibernate Configuration DTD//EN"
    "http://hibernate.sourceforge.net/hibernate-configuration-3.0.dtd">

<hibernate-configuration>

  <!-- a SessionFactory instance -->
  <session-factory name="Tptbm">

    <!-- properties -->
    <property name="hibernate.connection.url">jdbc:timesten:direct:sampledb</property>
    <property name="hibernate.connection.username"></property>
    <property name="hibernate.connection.password"></property>                
    <property name="hibernate.connection.driver_class">com.timesten.jdbc.TimesTenDriver</property> 
   
    <property name="hibernate.dialect">org.hibernate.dialect.TimesTenDialect</property>        
    <property name="hibernate.connection.isolation">2</property>
    <property name="hibernate.temp.use_jdbc_metadata_defaults">false</property>
    
    <property name="hibernate.jdbc.fetch_size">32</property>
    <property name="hibernate.jdbc.batch_size">256</property>
    <property name="hibernate.jdbc.batch_versioned_data">true</property>

    <property name="hibernate.jdbc.use_streams_for_binary">false</property>
    <property name="hibernate.jdbc.use_get_generated_keys">false</property>
    <property name="hibernate.jdbc.use_scrollable_resultset">false</property>
    
    <property name="hibernate.cache.use_query_cache">false</property>
    <property name="hibernate.cache.use_second_level_cache">false</property>
    
    <property name="hibernate.show_sql">false</property>        
    <property name="hibernate.connection.pool_size">4</property>

    <!-- mapping file -->
    <mapping resource="META-INF/Tptbm.hbm.xml"/>

  </session-factory>

</hibernate-configuration>
```
