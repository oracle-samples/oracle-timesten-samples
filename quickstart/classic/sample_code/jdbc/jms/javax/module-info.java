/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * module-info.java demonstrates a basic example to define module name; import 
 * TimesTen JDBC and JMS/XLA modules; and import other modules like the one 
 * created in JDBC samples "my.jdbc.app.module" so we can use it's exported
 * classes of package jdbc.demo  
 */

module my.jms.app.module {
    requires timesten.jdbc;
    requires timesten.jmsxla;
    requires my.jdbc.app.module;
    requires java.sql;
    requires jms;
    requires java.naming;
}
