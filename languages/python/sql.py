#
# Copyright (c) 2019, 2026 Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
#
#   DESCRIPTION
#     This sample simulates a small application service that keeps active API
#     session state in TimesTen. Applications often need fast access to this
#     type of short-lived operational data to route requests, update activity
#     counters, and remove sessions that are no longer active.
#
#     The sample uses an 'api_sessions' table with session identifiers, user
#     names, service names, regions, request counters, session status, and a
#     last-seen timestamp. It demonstrates basic SQL operations that are common
#     in low-latency application state management:
#       - Creates the 'api_sessions' table
#       - Populates the table (based on 'NUM_RECORDS')
#       - Performs a number of SELECTS (based on 'READ_PERCENTAGE')
#       - Updates request counters and last-seen timestamps for active sessions
#         (based on 'UPDATE_PERCENTAGE')
#       - Deletes a number of session records (based on 'UPDATE_PERCENTAGE')
#       - Drops the table
#
import datetime
import math

import oracledb

import AccessControl

NUM_RECORDS       = 100  # Number of records to insert. (Must have an exact square root)
READ_PERCENTAGE   = 80   # Percentage of records to perform SELECTS.
UPDATE_PERCENTAGE = 20   # Percentage of records to perform UPDATES & DELETES.

# Declare all the statements.
# Statements will use positional binding (:1,:2) -> [arg1, arg2]
createStmnt = """
      CREATE TABLE api_sessions(
        session_id     TT_INT        NOT NULL PRIMARY KEY,
        user_name      VARCHAR2(30)  NOT NULL,
        service_name   VARCHAR2(40)  NOT NULL,
        region         VARCHAR2(20)  NOT NULL,
        request_count  TT_INT        NOT NULL,
        session_status VARCHAR2(12)  NOT NULL,
        last_seen      TIMESTAMP     NOT NULL);
"""

insertStmnt = "INSERT INTO api_sessions VALUES (:1,:2,:3,:4,:5,:6,:7)"

selectStmnt = """
  SELECT user_name, service_name, region, request_count, session_status
  FROM api_sessions
  WHERE session_id = :1
"""

updateStmnt = """
  UPDATE api_sessions
  SET request_count = request_count + 1,
      session_status = :1,
      last_seen = :2
  WHERE session_id = :3
"""

deleteStmnt = "DELETE FROM api_sessions WHERE session_id = :1"

dropStmnt   = "DROP TABLE api_sessions"

# Get connection and cursor.
def connect():
  credentials = AccessControl.getCredentials("sql.py")
  connection = oracledb.connect(user=credentials.user, password=credentials.password, dsn=credentials.connstr)
  # Set autocommit to true.
  connection.autocommit = True
  return connection.cursor()

# Create table
def createTable(cursor):
  cursor.execute(createStmnt)
  # Check if the table has been created properly.
  cursor.execute("SELECT COUNT(*) FROM api_sessions")
  count, = cursor.fetchone()
  if count == 0:
    print("Table has been created")

# Populate table
def populateTable(cursor):
  print("Populating table")
  keyCnt = int(math.sqrt(NUM_RECORDS))
  services = ["orders", "payments", "search", "support"]
  regions = ["us-east", "us-west", "eu-central", "ap-south"]

  # Prepare insert statement
  cursor.prepare(insertStmnt)
  # Loop to generate the N key combinations
  for i in range(keyCnt):
    for j in range(keyCnt):
      sessionId = (i * keyCnt) + j
      userName = "user_" + str(i) + str(j)
      serviceName = services[sessionId % len(services)]
      region = regions[sessionId % len(regions)]
      requestCount = sessionId % 25
      sessionStatus = "ACTIVE"
      lastSeen = datetime.datetime.now()

      # As the stmnt is prepared, just call execute with None as args
      cursor.execute(None,
          [sessionId, userName, serviceName, region, requestCount, sessionStatus, lastSeen])

    print("  Inserted " + str((i + 1) * keyCnt) + " rows")

  # Verify that the rows have been inserted
  cursor.execute("SELECT COUNT(*) FROM api_sessions")
  count, = cursor.fetchone()
  if count != (keyCnt * keyCnt):
    print("Error populating table")

# Perform DML operations
def performDML(cursor, operation):
  print("Performing " + operation + "s")
  # Calculate 'numOperations' depending on the proper percentage
  if operation == "select":
    numOperations = int(NUM_RECORDS * (float(READ_PERCENTAGE) / 100))
  elif operation == "update" or operation == "delete":
    numOperations = int(NUM_RECORDS * (float(UPDATE_PERCENTAGE) / 100))
  else:
    print("Unsupported operation: " + operation)
    return

  operationsPerformed = 0

  keyCnt = int(math.sqrt(NUM_RECORDS))
  for i in range(keyCnt):
    for j in range(keyCnt):
      sessionId = (i * keyCnt) + j

      # Check which operation was requested and execute it
      if operation == "select":
        cursor.execute(selectStmnt, [sessionId])
      elif operation == "update":
        cursor.execute(updateStmnt, ["ACTIVE", datetime.datetime.now(), sessionId])
      elif operation == "delete":
        cursor.execute(deleteStmnt, [sessionId])

      if cursor.rowcount is not None:
        operationsPerformed += 1

      if operationsPerformed == numOperations:
        print("  " + operation + "(ed) " + str(operationsPerformed) + " rows")
        return

    print("  " + operation + "(ed) " + str((i + 1) * keyCnt) + " rows")


# Drop table.
def dropTable(cursor):
  cursor.execute(dropStmnt)

# Release connection
def releaseConnection(cursor):
  cursor.close()
  cursor.connection.close()
  print("Connection has been released")

def main():
  cursor = connect()
  createTable(cursor)
  populateTable(cursor)
  performDML(cursor, "select")
  performDML(cursor, "update")
  performDML(cursor, "delete")
  dropTable(cursor)
  releaseConnection(cursor)

# Execute script
if __name__ == "__main__":
  main()
