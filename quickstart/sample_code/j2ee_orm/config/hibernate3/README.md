Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

# Hibernate SQL Dialect for TimesTen 11.2.2

This directory contains the source code and build script for a Hibernate SQL dialect class for TimesTen IMDB 11.2.2. The class name is TimesTenDialect1122. This dialect class has been tested with various versions of the Hibernate 3.x.x series. The latest version to be tested is Hibernate 3.6.0 with TimesTen IMDB 11.2.2.

Use of this Hibernate dialect class will help to avoid SQL syntax incompatibilites for Hibernate applications that use TimesTen. 

To compile and configure the TimesTenDialect1122 class follow these steps:

1. Edit the build.properties file in this directory. Set the hibernate.home.dir and hibernate.jar properties to reference the location of the local Hibernate 3 distribution.

2. The Apache Ant build tool is used to compile the class. Make sure that the directory containing the ant executable is included on the PATH environment variable. Execute these commands from this directory to compile the class.

    ant clean
    ant

3. If the dialect class compiles successfully it will be located in a subdirectory as 'build/org/hibernate/dialect/TimesTenDialect1122.class'. This class must be included in the Hibernate application's CLASSPATH environment variable in order for it to be used by the application.

4. To configure a Hibernate application to use the TimesTenDialect1122 class the hibernate.dialect session property must be set to 'org.hibernate.dialect.TimesTenDialect1122'. The example hibernate.cfg.xml configuration file below demonstrates a TimesTen Hibernate session configuration that uses the TimesTenDialect1122 class.

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
   
    <property name="hibernate.dialect">org.hibernate.dialect.TimesTenDialect1122</property>        
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