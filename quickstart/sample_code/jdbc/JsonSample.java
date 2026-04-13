/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * DESCRIPTION
 *   Demonstrates TimesTen JSON features using the JDBC driver.
 */

package jdbc.demo;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.PrintWriter;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Timestamp;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
/**
 * Provides a complete JDBC-based demonstration of TimesTen JSON support,
 * including table creation, document loading, indexing, querying, and cleanup.
 */
public class JsonSample
{
  private static final String PROGRAM_NAME = "JsonSample";
  private static final String DEFAULT_USERNAME = "appuser";

  private static final String TIMESTEN_DIRECT_DRIVER = "com.timesten.jdbc.TimesTenDriver";
  private static final String TIMESTEN_CLIENT_DRIVER = "com.timesten.jdbc.TimesTenClientDriver";

  private static final String DIRECT_URL_PREFIX = "jdbc:timesten:direct:";
  private static final String CLIENT_URL_PREFIX = "jdbc:timesten:client:";

  private static final String TABLE_NAME = "j_purchaseorder";

  private static final String JSON_DOC1    = "../common/jsondoc1.json";
  private static final String JSON_DOC1_V2 = "../common/jsondoc1-v2.json";
  private static final String JSON_DOC2    = "../common/jsondoc2.json";

  private boolean keepData = false;
  private IOLibrary ioLibrary;
  private String userOverride;
  private String passwordOverride;

  /**
   * Entry point for the JSON sample application.
   *
   * @param args command-line arguments supplied by the user
   */
  public static void main(String[] args)
  {
    int exitCode = new JsonSample().run(args);
    if (exitCode != 0)
    {
      System.exit(exitCode);
    }
  }

  /**
   * Coordinates option parsing, connectivity, and demo execution.
   *
   * @param args command-line arguments supplied by the user
   * @return 0 on success; non-zero otherwise
   */
  private int run(String[] args)
  {
    ioLibrary = new IOLibrary(System.err);

    String usage = buildUsage();
    if (!parseOptions(args, usage))
    {
      return 1;
    }

    if (ioLibrary.opt_doTrace)
    {
      DriverManager.setLogWriter(new PrintWriter(System.out, true));
    }

    if (!loadDrivers())
    {
      return 1;
    }

    AccessControl accessControl = new AccessControl();
    String username = (userOverride != null) ? userOverride : resolveUsername(accessControl);
    String password = (passwordOverride != null)
                      ? passwordOverride
                      : accessControl.getPassword(username);

    String url = buildJdbcUrl();

    System.out.println();
    System.out.println("Connecting using URL: " + url);

    try (Connection connection = DriverManager.getConnection(url, username, password))
    {
      System.out.println("Connected");
      runDemo(connection);
      System.out.println("Completed JSON sample operations");
      if (!keepData)
      {
        dropTable(connection, true);
      }
      else
      {
        System.out.println("Leaving table " + TABLE_NAME + " in place (-keep specified)");
      }
    }
    catch (SQLException e)
    {
      printSQLException(e);
      return 1;
    }
    catch (IOException e)
    {
      System.err.println("Unable to read JSON document: " + e.getMessage());
      return 1;
    }

    return 0;
  }

  /**
   * Executes the sample workflow: create table, load JSON, index, update, and query.
   *
   * @param connection open JDBC connection to TimesTen
   * @throws SQLException if any database operation fails
   * @throws FileNotFoundException if a JSON source document is missing
   */
  private void runDemo(Connection connection) throws SQLException, IOException
  {
    dropTable(connection, false);
    createTable(connection);

    String jsonDoc1   = readJsonFile(JSON_DOC1);
    String jsonDoc1v2 = readJsonFile(JSON_DOC1_V2);
    String jsonDoc2   = readJsonFile(JSON_DOC2);

    insertRow(connection, "1600", jsonDoc1);
    insertRow(connection, "1721", jsonDoc2);

    createJsonIndex(connection);

    updateRow(connection, "1600", jsonDoc1v2, JSON_DOC1_V2);

    selectPoDocumentById(connection, "1600");
    selectPoDocumentById(connection, "1721");

    selectPoDocumentsByUser(connection, "ABULL");
    selectPoDocumentsByUser(connection, "CGIRAFFE");
    selectLineItemsForOrder(connection, "1600");
  }

  /**
   * Parses command-line options, handling sample-specific switches and delegating remainder to IOLibrary.
   *
   * @param args  command-line arguments to parse
   * @param usage usage string to display upon parse failure
   * @return {@code true} when parsing succeeds; {@code false} otherwise
   */
  private boolean parseOptions(String[] args, String usage)
  {
    List<String> forwardedArgs = new ArrayList<String>();

    for (int i = 0; i < args.length; i++)
    {
      String arg = args[i];

      if ("-keep".equals(arg))
      {
        keepData = true;
      }
      else if ("-user".equals(arg) || "-username".equals(arg))
      {
        if (i + 1 >= args.length)
        {
          System.err.println("Missing value for " + arg);
          System.err.println(usage);
          return false;
        }
        userOverride = args[++i];
      }
      else if ("-password".equals(arg) || "-pwd".equals(arg))
      {
        if (i + 1 >= args.length)
        {
          System.err.println("Missing value for " + arg);
          System.err.println(usage);
          return false;
        }
        passwordOverride = args[++i];
      }
      else
      {
        forwardedArgs.add(arg);
      }
    }

    String[] ioArgs = forwardedArgs.toArray(new String[forwardedArgs.size()]);

    if (!ioLibrary.parseOpts(ioArgs, usage))
    {
      return false;
    }

    return true;
  }

  /**
   * Builds the help/usage string presented to the user when option parsing fails or -help is provided.
   *
   * @return formatted usage text including the sample-specific -keep option description
   */
  private String buildUsage()
  {
    String baseUsage = ioLibrary.getUsageString(PROGRAM_NAME);
    String keepOption = "\n  -keep         retain " + TABLE_NAME + " after the sample completes";
    String userOption = "\n  -user <name>  supply username non-interactively";
    String pwdOption  = "\n  -password <pw> supply password non-interactively";
    return baseUsage + keepOption + userOption + pwdOption;
  }

  /**
   * Ensures the necessary TimesTen JDBC drivers are available to the JVM.
   *
   * @return {@code true} if all required drivers load successfully; {@code false} otherwise
   */
  private boolean loadDrivers()
  {
    try
    {
      Class.forName(TIMESTEN_DIRECT_DRIVER);
      if (ioLibrary.opt_doClient)
      {
        Class.forName(TIMESTEN_CLIENT_DRIVER);
      }
      return true;
    }
    catch (ClassNotFoundException e)
    {
      String drivers = ioLibrary.opt_doClient
                       ? TIMESTEN_DIRECT_DRIVER + ", " + TIMESTEN_CLIENT_DRIVER
                       : TIMESTEN_DIRECT_DRIVER;
      System.err.println("Unable to load TimesTen JDBC driver(s): " + drivers + ". " + e.getMessage());
      return false;
    }
  }

  /**
   * Determines the JDBC username, falling back to a sensible default when AccessControl supplies none.
   *
   * @param accessControl AccessControl helper providing credential information
   * @return chosen username for the current session
   */
  private String resolveUsername(AccessControl accessControl)
  {
    String username = accessControl.getUsername();
    if (username == null || username.isEmpty())
    {
      return DEFAULT_USERNAME;
    }
    return username;
  }

  /**
   * Builds the JDBC URL using connection mode and connection string supplied by the common IOLibrary.
   *
   * @return fully-qualified JDBC URL
   */
  private String buildJdbcUrl()
  {
    String prefix = ioLibrary.opt_doClient ? CLIENT_URL_PREFIX : DIRECT_URL_PREFIX;
    return prefix + ioLibrary.opt_connstr;
  }

  /**
   * Creates the purchase order table used throughout the sample run.
   *
   * @param connection active JDBC connection
   * @throws SQLException if the CREATE TABLE statement fails
   */
  private void createTable(Connection connection) throws SQLException
  {
    String sql = "CREATE TABLE " + TABLE_NAME +
                 " (id VARCHAR2(32) NOT NULL PRIMARY KEY, date_loaded TIMESTAMP, po_document JSON)";

    try (PreparedStatement statement = connection.prepareStatement(sql))
    {
      statement.execute();
      System.out.println("Table " + TABLE_NAME + " created");
    }
  }

  /**
   * Drops the purchase order table, optionally reporting errors when the table is absent.
   *
   * @param connection     active JDBC connection
   * @param reportMissing  whether to notify the user when the table does not exist
   */
  private void dropTable(Connection connection, boolean reportMissing)
  {
    String sql = "DROP TABLE " + TABLE_NAME;

    try (PreparedStatement statement = connection.prepareStatement(sql))
    {
      statement.execute();
      System.out.println("Table " + TABLE_NAME + " dropped");
    }
    catch (SQLException e)
    {
      if (reportMissing)
      {
        System.out.println("Table " + TABLE_NAME + " not dropped: " + e.getMessage());
      }
    }
  }

  /**
   * Inserts a new purchase order row into the sample table.
   *
   * @param connection active JDBC connection
   * @param id         identifier for the purchase order
   * @param jsonString JSON document representing the purchase order
   * @throws SQLException if the INSERT statement fails
   */
  private void insertRow(Connection connection, String id, String jsonString) throws SQLException
  {
    String sql = "INSERT INTO " + TABLE_NAME + " (id, date_loaded, po_document) VALUES (?, ?, ?)";

    try (PreparedStatement statement = connection.prepareStatement(sql))
    {
      statement.setString(1, id);
      statement.setTimestamp(2, currentTimestamp());
      statement.setString(3, jsonString);
      statement.executeUpdate();
      System.out.println("Inserted purchase order with id " + id);
    }
  }

  /**
   * Updates an existing purchase order with a new JSON document.
   *
   * @param connection active JDBC connection
   * @param id         identifier for the purchase order
   * @param jsonString replacement JSON document contents
   * @param sourcePath path of the JSON file supplying replacement content
   * @throws SQLException if the UPDATE statement fails
   */
  private void updateRow(Connection connection, String id, String jsonString, String sourcePath) throws SQLException
  {
    String sql = "UPDATE " + TABLE_NAME + " SET date_loaded = ?, po_document = ? WHERE id = ?";

    try (PreparedStatement statement = connection.prepareStatement(sql))
    {
      statement.setTimestamp(1, currentTimestamp());
      statement.setString(2, jsonString);
      statement.setString(3, id);
      int updated = statement.executeUpdate();
      if (updated > 0)
      {
        System.out.println("Updated purchase order with id " + id + " using " + sourcePath);
      }
      else
      {
        System.out.println("No rows updated for id " + id);
      }
    }
  }

  /**
   * Creates a JSON index on the User attribute to accelerate sample queries.
   *
   * @param connection active JDBC connection
   * @throws SQLException if the CREATE INDEX statement fails
   */
  private void createJsonIndex(Connection connection) throws SQLException
  {
    String sql = "CREATE INDEX idx_json_user ON " + TABLE_NAME +
                 " (JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128) ERROR ON ERROR))";

    try (PreparedStatement statement = connection.prepareStatement(sql))
    {
      statement.execute();
      System.out.println("JSON index IDX_JSON_USER created on " + TABLE_NAME);
    }
  }

  /**
   * Retrieves and prints the JSON purchase order for the specified identifier.
   *
   * @param connection active JDBC connection
   * @param id         identifier for the purchase order to retrieve
   * @throws SQLException if the SELECT statement fails
   */
  private void selectPoDocumentById(Connection connection, String id) throws SQLException
  {
    String sql = "SELECT JSON_SERIALIZE(po_document PRETTY) FROM " + TABLE_NAME + " WHERE id = ?";

    try (PreparedStatement statement = connection.prepareStatement(sql))
    {
      statement.setString(1, id);

      try (ResultSet resultSet = statement.executeQuery())
      {
        if (resultSet.next())
        {
          System.out.println("Purchase order for id " + id + ":");
          System.out.println(resultSet.getString(1));
        }
        else
        {
          System.out.println("No purchase order found for id " + id);
        }
      }
    }
  }

  /**
   * Retrieves and prints all JSON purchase orders associated with the given user.
   *
   * @param connection active JDBC connection
   * @param user       user identifier embedded within the JSON documents
   * @throws SQLException if the SELECT statement fails
   */
  private void selectPoDocumentsByUser(Connection connection, String user) throws SQLException
  {
    String sql = "SELECT JSON_SERIALIZE(po_document PRETTY) FROM " + TABLE_NAME +
                 " WHERE JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128)) = ?";

    try (PreparedStatement statement = connection.prepareStatement(sql))
    {
      statement.setString(1, user);

      try (ResultSet resultSet = statement.executeQuery())
      {
        boolean printed = false;
        while (resultSet.next())
        {
          if (!printed)
          {
            System.out.println("Purchase orders for user " + user + ":");
            printed = true;
          }
          System.out.println(resultSet.getString(1));
        }

        if (!printed)
        {
          System.out.println("No purchase orders found for user " + user);
        }
      }
    }
  }

  /**
   * Uses JSON_TABLE to project purchase order line items into relational columns for the specified order id.
   *
   * @param connection active JDBC connection
   * @param id         identifier for the purchase order whose line items should be listed
   * @throws SQLException if the SELECT statement fails
   */
  private void selectLineItemsForOrder(Connection connection, String id) throws SQLException
  {
    String sql =
      "SELECT jt.line_number, jt.sku, jt.description, jt.quantity, jt.unit_price FROM " + TABLE_NAME + " po, " +
      "JSON_TABLE(po.po_document, '$.LineItems[*]' COLUMNS (" +
      "line_number FOR ORDINALITY, " +
      "sku          VARCHAR2(40) PATH '$.Part.UPCCode', " +
      "description  VARCHAR2(256) PATH '$.Part.Description', " +
      "quantity     NUMBER PATH '$.Quantity', " +
      "unit_price   NUMBER PATH '$.Part.UnitPrice'" +
      ")) jt WHERE po.id = ?";

    try (PreparedStatement statement = connection.prepareStatement(sql))
    {
      statement.setString(1, id);

      try (ResultSet resultSet = statement.executeQuery())
      {
        boolean printed = false;
        while (resultSet.next())
        {
          if (!printed)
          {
            System.out.println("Line items for purchase order " + id + ":");
            System.out.println("Line  SKU           Description                     Qty  Unit Price  Extended");
            printed = true;
          }

          int lineNumber = resultSet.getInt(1);
          String sku = resultSet.getString(2);
          String description = resultSet.getString(3);
          double quantity = resultSet.getDouble(4);
          double unitPrice = resultSet.getDouble(5);
          double extended = quantity * unitPrice;

          System.out.printf("%4d  %-12s %-30s %4.0f  %10.2f  %8.2f%n",
                            lineNumber, sku, description, quantity, unitPrice, extended);
        }

        if (!printed)
        {
          System.out.println("No line items found for purchase order " + id);
        }
      }
    }
  }

  /**
   * Reads a JSON document from disk, searching both relative and local paths.
   *
   * @param path relative path to the JSON source file
   * @return JSON content as a string
   * @throws FileNotFoundException when the specified file cannot be located
   */
  private String readJsonFile(String path) throws IOException
  {
    Path file = locateJsonFile(path);
    return Files.readString(file, StandardCharsets.UTF_8);
  }

  /**
   * Resolves the JSON file location, supporting both relative paths and execution-directory lookups.
   *
   * @param path relative path to the JSON source file
   * @return a {@link File} pointing to the located JSON document
   * @throws FileNotFoundException when the file cannot be located in either search location
   */
  private Path locateJsonFile(String path) throws FileNotFoundException
  {
    Path relative = Path.of(path);
    if (Files.exists(relative))
    {
      return relative;
    }

    Path local = Path.of(relative.getFileName().toString());
    if (Files.exists(local))
    {
      return local;
    }

    throw new FileNotFoundException(path);
  }

  /**
   * Provides a timestamp for data insertion and updates.
   *
   * @return current timestamp
   */
  private Timestamp currentTimestamp()
  {
    return new Timestamp(System.currentTimeMillis());
  }

  /**
   * Prints TimesTen-specific details for the provided {@link SQLException} chain.
   *
   * @param exception root {@link SQLException} thrown during processing
   */
  private void printSQLException(SQLException exception)
  {
    SQLException current = exception;
    while (current != null)
    {
      System.err.println("SQLState: " + current.getSQLState());
      System.err.println("ErrorCode: " + current.getErrorCode());
      System.err.println("Message: " + current.getMessage());
      current = current.getNextException();
      if (current != null)
      {
        System.err.println();
      }
    }
  }
}
