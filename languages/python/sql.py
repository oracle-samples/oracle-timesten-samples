#
# Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
#
#   DESCRIPTION
#     This sample performs the follwing steps:
#       - Creates a table 'vpn_users'
#       - Populates the table (based on 'NUM_RECORDS').
#       - Performs a number of SELECTS (based on 'READ_PERCENTAGE')
#       - Updates a number of records (based on 'UPDATE_PERCENTAGE')
#       - Deletes a number of records (based on 'UPDATE_PERCENTAGE')
#       - Drops the table
#
import oracledb
import math
import AccessControl

NUM_RECORDS       = 100  # Number of records to insert. (Must have an exact square root)
READ_PERCENTAGE   = 80    # Percentage of records to perform SELECTS.
UPDATE_PERCENTAGE = 20    # Percentage of records to perform UPDATES &  DELETES.

# Declare all the statements.
# Statements will use positional binding (:1,:2) -> [arg1, arg2]
createStmnt =  """
      CREATE TABLE vpn_users(
        vpn_id             TT_INT   NOT NULL,
        vpn_nb             TT_INT   NOT NULL,
        directory_nb       CHAR(100)  NOT NULL,
        last_calling_party CHAR(100)  NOT NULL,
        descr              CHAR(100) NOT NULL,
        PRIMARY KEY (vpn_id, vpn_nb)); 
"""

insertStmnt = "INSERT INTO vpn_users VALUES (:1,:2,:3,:4,:5)"

selectStmnt = """
  SELECT directory_nb, last_calling_party, descr FROM vpn_users
  WHERE vpn_id = :1 AND vpn_nb= :2
"""

updateStmnt = """
  UPDATE vpn_users SET last_calling_party = :1 
  WHERE vpn_id = :2 AND vpn_nb = :3 
"""

deleteStmnt = "DELETE FROM vpn_users WHERE vpn_id = :1 AND vpn_nb = :2"

dropStmnt   = "DROP TABLE vpn_users"

# Get connection and cursor.
def connect():
  credentials     = AccessControl.getCredentials("sql.py")
  connection = oracledb.connect(user=credentials.user, password=credentials.password, dsn=credentials.connstr)
  # Set autocommit to true.
  connection.autocommit = True
  return connection.cursor()

# Create table
def createTable(cursor):
  cursor.execute(createStmnt)
  # Check if the table has been created properly.
  cursor.execute("SELECT COUNT(*) FROM vpn_users")
  count, = cursor.fetchone()
  if count == 0:
    print ("Table has been created ")

# Populate table
def populateTable(cursor):
  print("Populating table")
  # Keys are a combination of vpn_id and vpn_nb
  keyCnt = int(math.sqrt(NUM_RECORDS));
  # Prepare insert statement 
  cursor.prepare(insertStmnt)
  # Loop to generate the N key combinations
  for i in range(keyCnt):
    for j in range(keyCnt):
      iD = i
      nb = j
      directoryNb         = "dir_"  + str(i) + str(j)
      lastCallingParty    = "call_" + str(i) + str(j)
      desc                = "desc_" + str(i) + str(j)
      

      # As the stmnt is prepared, just call execute with None as args
      cursor.execute(None,
          [iD, nb, directoryNb, lastCallingParty, desc])
    
    print("  Inserted " + str((i + 1)*keyCnt) + " rows")
  
  # Verify that the rows have been inserted
  cursor.execute("SELECT COUNT(*) FROM vpn_users")
  count, = cursor.fetchone()
  if count != (keyCnt * keyCnt):
    print ("Error populating table")

# Perform DML operations
def performDML(cursor, operation):
  print("Performing " + operation+"s")
  # Calculate 'numOperations' depending on the proper precentage
  if operation   == "select":
    numOperations = int (100 * (float(READ_PERCENTAGE) / float(NUM_RECORDS)))
  elif operation == "update" or "delete":
    numOperations = int (100 * (float(UPDATE_PERCENTAGE) / float(NUM_RECORDS)))
 
  operationsPerformed = 0

  # Keys are a combination of vpn_id and vpn_nb
  keyCnt = int(math.sqrt(NUM_RECORDS));
  for i in range(keyCnt):
    for j in range(keyCnt):
      iD = i
      nb = j

      # Check which operation was requested and execute it
      if   operation == "select":
        cursor.execute(selectStmnt, [iD,nb])
      elif operation == "update":
        lastCallingParty  = "callU_" + str(iD) + str(nb)
        cursor.execute(updateStmnt, [lastCallingParty,iD, nb])
      elif operation == "delete":
        cursor.execute(deleteStmnt, [iD, nb])

      if cursor.rowcount != None:
        operationsPerformed += 1

      if operationsPerformed == numOperations:
        print("  " + operation+"(ed) " + str(operationsPerformed) + " rows")
        return

    print("  " + operation+"(ed) " + str((i + 1)*keyCnt) + " rows")


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
