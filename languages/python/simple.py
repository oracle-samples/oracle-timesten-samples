#
# Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
#
#   DESCRIPTION
#     Makes a connection, inserts one row, selects the inserted row, and drops the table
#

from __future__ import print_function
import cx_Oracle
import AccessControl

def run():
  try:
    credentials = AccessControl.getCredentials("simple.py")
    connection = cx_Oracle.connect(credentials.user, credentials.password, credentials.connstr)
      
    cursor = connection.cursor()
    cursor.execute("""
      CREATE TABLE employees(first_name VARCHAR2(20), last_name VARCHAR2(20))""")
    print("Table has been created")
    values = [["ROBERT", "ROBERTSON"], ["ANDY", "ANDREWS"], ["MICHAEL", "MICHAELSON"]]
    cursor.executemany("INSERT INTO employees VALUES (:1, :2)", values)
    print("Inserted ", len(values), "employees into the table")
    cursor.execute("""
      SELECT first_name, last_name FROM employees""")
    for fname, lname in cursor:
      print("Selected employee:", fname, lname)
    cursor.execute("DROP TABLE employees")
    print("Table has been dropped")
    cursor.close()
    connection.close()
    print("Connection has been released")

  except Exception as e:
    # Something went wrong
    print("An error ocurred", str(e))

run()

