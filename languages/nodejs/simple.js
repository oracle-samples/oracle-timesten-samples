/*
* Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 *  DESCRIPTION
 *    Makes a connection, inserts one row, selects the inserted row, and drops trhe table
 */
'use strict';
var oracledb      = require('oracledb');
var accessControl = require('./AccessControl');

oracledb.initOracleClient();
async function run() {

  let connection;

  try {
    let credentials   = accessControl.getCredentials("simple.js");
    connection = await oracledb.getConnection({
      user: credentials['-u'], password: credentials['-p'], connectString: credentials['-c'] 
    });

    let result;

    await connection.execute("CREATE TABLE employees(first_name VARCHAR2(20), last_name VARCHAR2(20))");
    console.log("Table has been created");
    const values = [["ROBERT", "ROBERTSON"], ["ANDY", "ANDREWS"], ["MICHAEL", "MICHAELSON"]];
    await connection.executeMany("INSERT INTO employees VALUES(:1, :2)", values);
    console.log("Inserted", values.length, "employees into the table");
    result = await connection.execute("SELECT first_name, last_name FROM employees");
    result.rows.forEach(function(row){
    console.log("Selected employee:", row[0], row[1]);
    });
    await connection.execute("DROP TABLE employees");
    console.log("Table has been dropped");
  }
  catch (err) {
    console.log(err);
  }
  finally {
    if (connection) {
      try { 
        connection.close();
        console.log("Connection has been released");
      }
      catch (err) { console.log(err); }
    }
  }
}

run();

