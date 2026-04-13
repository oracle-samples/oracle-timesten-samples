/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * module-info.java demonstrates a basic example to define module name, import 
 * TimesTen JDBC Driver module and export classes of package jdbc.demo
 */

module my.jdbc.app.module {
    requires timesten.jdbc;
    requires java.sql;
    exports jdbc.demo;
}
