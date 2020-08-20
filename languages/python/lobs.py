#
# Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
#
#   DESCRIPTION
#     Writing a CLOB from a file and     
#     reading its content.
#

import cx_Oracle
import AccessControl
import os

def lobSample(cursor):
    
  print("> Creating table with CLOB column")
  try:
    cursor.execute("DROP TABLE clobs")
  except Exception as e:
    pass
  cursor.execute("CREATE TABLE clobs (id NUMBER, text CLOB)")

  print("> Reading file")
  with open( os.path.join( os.path.dirname(__file__ ), 'text.txt'), 'r') as file:
    string = file.read()
    
    print("> Populating CLOB from file")
    cursor.execute("INSERT INTO clobs VALUES (1, :colClob)", colClob = string)

  print("> Querying CLOB column")
  cursor.execute("SELECT text FROM clobs where id=1")
  outClob, = cursor.fetchone()

  print("> Reading CLOB")
  print(outClob.read())
  print("> Finished reading CLOB")


def run():
  try:
    print("> Connecting")
    credentials = AccessControl.getCredentials("lobs.py")
    connection = cx_Oracle.connect(credentials.user, credentials.password, credentials.connstr)
    connection.autocommit = True
    cursor = connection.cursor()

    lobSample(cursor)

  except Exception as e:
    print("An error ocurred", str(e))
  finally:
    if(connection):
      try:
        cursor.close()
        connection.close()
        print("> Connection released")
      except Exception as e:
        print("An error ocurred", str(e))

# Execute sample
run()

