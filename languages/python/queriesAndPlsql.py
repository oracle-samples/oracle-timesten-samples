#
# Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
#    
#   DESCRIPTION
#     This sample performs the follwing steps:
#       - Creates the table 'items'
#       - Populates the table (based on 'NUM_RECORDS').
#       - Performs a SELECT statement fetching 100 rows
#       - Calls a PL/SQL anonymous block to update a row
#       - Calls a PL/SQL anonymous block to delete a row
#       - Drops the table
#
import oracledb
import AccessControl

NUM_RECORDS = 100  # Number of records to insert.

# Declare all the statements.
# Statements will use positional binding (:1,:2) -> [arg1, arg2]
createStmnt =  """
  BEGIN 
    DECLARE 
      e_table_missing EXCEPTION; 
      PRAGMA EXCEPTION_INIT(e_table_missing, -00942); 
    BEGIN 
      EXECUTE IMMEDIATE ('DROP TABLE items'); 
    EXCEPTION 
      WHEN e_table_missing 
      THEN NULL; 
    END; 
    EXECUTE IMMEDIATE(' 

      CREATE TABLE items(
        id                TT_INT            NOT NULL,
        name              VARCHAR2(100)      NOT NULL,
        descr             VARCHAR2(100) NOT NULL,
        PRIMARY KEY (id))

    '); 
  END;
"""

insertStmnt = "INSERT INTO items VALUES (:1,:2,:3)"

selectStmnt = "SELECT id, name, descr FROM items WHERE id BETWEEN 0 AND 19"

# Block to update description of a row. 
updateBlock = """
  DECLARE
    new_descr VARCHAR2(100) := 'updated description';
  BEGIN
    UPDATE items SET descr = new_descr WHERE id = :id_in ;
  END;
"""

# Block to delete a row.
deleteBlock = """
  BEGIN
    DELETE FROM items WHERE id = :id_in;
  END;
"""

dropStmnt   =         "DROP TABLE items"

# Get connection and cursor.
def connect():
  credentials = AccessControl.getCredentials("queriesAndPlsql.py")
  connection = oracledb.connect(user=credentials.user, password=credentials.password, dsn=credentials.connstr)
  connection.autocommit = True
  return connection.cursor()

# Create table function.
def createTable(cursor):
  cursor.execute(createStmnt)
  # Check if the table has been created properly.
  cursor.execute("SELECT COUNT(*) FROM items")
  count, = cursor.fetchone()
  if count == 0:
    print ("Table has been created ")

# Populate table with executeMany
def insertExecuteMany(cursor):
  print("Inserting with executeMany ...")
  # Setup all the rows to insert
  rows = [ [n, "name_"+str(n), "descr_"+str(n)] for n in range(NUM_RECORDS) ]
  # ExecuteMany will insert as many rows as elements in the array
  cursor.executemany(insertStmnt, rows)
  
  # Verify that the rows have been inserted
  cursor.execute("SELECT COUNT(*) FROM items")
  count, = cursor.fetchone()
  print ( "  " + str(count), " Registries added")

# Select and use iterators
def performSelect(cursor):
  print("Select some rows with one select ...")
  # 'rows'array will contain all the fetched rows
  rows = []
  
  cursor.execute(selectStmnt)
 
  # The cursor can be used directly as an iterator
  #   for accessing the rows in the result set
  for row in cursor:
    rows.append(row)

  print("  " + str(len(rows)), "Rows have been fetched and iterated")

# Update using a PLSQL anonymous block
def updateWithAnonBlock(cursor):
  print("Updating a row using an anonymous block ...")
  bindVars = { 
    "id_in" : 0   
  }
  # Check the previous value
  cursor.execute("SELECT descr FROM items WHERE id = :id_in", bindVars)
  res, = cursor.fetchone()
  print ("  Value before update: ", res)
  # Call the anonymous block
  cursor.execute(updateBlock, bindVars)
  # Verify that the value has changed
  cursor.execute("SELECT descr FROM items WHERE id = :id_in", bindVars)
  res, = cursor.fetchone()
  print ("  Value before update: ", res)

# Delete using a PLSQL anonymous block
def deleteWithAnonBlock(cursor):
  print("Delete a row using an anonymous block ...")
  bindVars = { 
    "id_in" : 0   
  }
  # Call the anonymous block
  cursor.execute(deleteBlock, bindVars)
  # Verify that the row has been deleted
  cursor.execute("SELECT COUNT(*) FROM items")
  count, = cursor.fetchone()
  print("  Rows after delete = ", count)


# Drop table.
def dropTable(cursor):
  cursor.execute(dropStmnt)

def releaseConnection(cursor):
  cursor.close()
  cursor.connection.close()
  print("Connection has been released")

def main():
  cursor = connect()
  createTable(cursor)
  insertExecuteMany(cursor)
  performSelect(cursor)
  updateWithAnonBlock(cursor)
  deleteWithAnonBlock(cursor)
  dropTable(cursor)
  releaseConnection(cursor)

if __name__ == "__main__":
  main()
