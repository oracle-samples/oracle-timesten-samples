using System;
using System.Collections.Generic;
using System.Data;
using System.Globalization;
using System.IO;
using System.Text;
using Oracle.DataAccess.Client;
using Oracle.DataAccess.Types;

namespace TimesTen.Samples.OdpNet
{
  public static class JsonSample
  {
    private const string ProgramName = "JsonSample";
    private const string TableName = "j_purchaseorder";

    private const string JsonDoc1 = "../common/jsondoc1.json";
    private const string JsonDoc1V2 = "../common/jsondoc1-v2.json";
    private const string JsonDoc2 = "../common/jsondoc2.json";

    private const string DefaultUsername = "appuser";

    private static readonly string[] UsageLines =
    {
      "Usage: JsonSample [-help|-h]",
      "       JsonSample [-keep] [-user <user>] [-password <password>] [-dsn <dsn>]",
      "",
      "  -help, -h        Show this help message",
      "  -keep            Retain demo table after completion",
      "  -user <user>     Specify database username",
      "  -password <pwd>  Specify database password",
      "  -dsn <name>      Specify TimesTen DSN (defaults to sampledb)"
    };

    private static bool keepData;
    private static string userOverride;
    private static string passwordOverride;
    private static string dsnOverride;

    /// <summary>
    /// Entry point for the sample executable; validates arguments, connects, and executes the workflow.
    /// </summary>
    /// <param name="args">Command-line arguments provided by the user.</param>
    /// <returns>Zero on success; non-zero for failures.</returns>
    public static int Main(string[] args)
    {
      if (!ParseArguments(args))
      {
        return 1;
      }

      string username = ResolveUsername();
      string password = ResolvePassword(username);
      string dataSource = ResolveDataSource();
      string connectionString = BuildConnectionString(username, password, dataSource);

      Console.WriteLine();
      Console.WriteLine("Connecting using DSN: " + dataSource);

      try
      {
        using (OracleConnection connection = new OracleConnection(connectionString))
        {
          connection.Open();
          Console.WriteLine("Connected");
          RunDemo(connection);
          Console.WriteLine("Completed JSON sample operations");
          if (!keepData)
          {
            DropTable(connection, true);
          }
          else
          {
            Console.WriteLine("Leaving table " + TableName + " in place (-keep specified)");
          }
        }
      }
      catch (OracleException ex)
      {
        PrintOracleException(ex);
        return 1;
      }
      catch (IOException ex)
      {
        Console.Error.WriteLine("Unable to read JSON document: " + ex.Message);
        return 1;
      }

      return 0;
    }

    /// <summary>
    /// Parses command-line switches and captures overrides for credentials and connectivity.
    /// </summary>
    /// <param name="args">Command-line arguments to interpret.</param>
    /// <returns>True when parsing succeeds; false otherwise.</returns>
    private static bool ParseArguments(IReadOnlyList<string> args)
    {
      var forwarded = new List<string>();

      for (int i = 0; i < args.Count; ++i)
      {
        string arg = args[i];

        switch (arg)
        {
          case "-help":
          case "-h":
            PrintUsage();
            return false;
          case "-keep":
            keepData = true;
            break;
          case "-user":
          case "-username":
            if (!TryConsumeValue(args, ref i, out userOverride, arg))
            {
              return false;
            }
            break;
          case "-password":
          case "-pwd":
            if (!TryConsumeValue(args, ref i, out passwordOverride, arg))
            {
              return false;
            }
            break;
          case "-dsn":
          case "-connstr":
            if (!TryConsumeValue(args, ref i, out dsnOverride, arg))
            {
              return false;
            }
            break;
          default:
            forwarded.Add(arg);
            break;
        }
      }

      if (forwarded.Count > 0)
      {
        Console.Error.WriteLine("Unrecognized arguments: " + string.Join(" ", forwarded));
        PrintUsage();
        return false;
      }

      return true;
    }

    /// <summary>
    /// Consumes the next argument as the value for the specified option, reporting errors when missing.
    /// </summary>
    /// <param name="args">Argument list under evaluation.</param>
    /// <param name="index">Current position within the argument list.</param>
    /// <param name="value">Receives the option value when present.</param>
    /// <param name="option">Option name used for diagnostics.</param>
    /// <returns>True when a value is successfully consumed; false otherwise.</returns>
    private static bool TryConsumeValue(IReadOnlyList<string> args, ref int index, out string value, string option)
    {
      if (index + 1 >= args.Count)
      {
        Console.Error.WriteLine("Missing value for " + option);
        PrintUsage();
        value = null;
        return false;
      }

      value = args[++index];
      return true;
    }

    /// <summary>
    /// Displays usage instructions for the sample.
    /// </summary>
    private static void PrintUsage()
    {
      foreach (string line in UsageLines)
      {
        Console.WriteLine(line);
      }
    }

    /// <summary>
    /// Determines the username to use for connectivity, applying overrides when provided.
    /// </summary>
    /// <returns>The resolved username.</returns>
    private static string ResolveUsername()
    {
      if (!string.IsNullOrEmpty(userOverride))
      {
        return userOverride;
      }

      return DefaultUsername;
    }

    /// <summary>
    /// Determines the password for connectivity, prompting interactively when absent.
    /// </summary>
    /// <param name="username">Username used when prompting for a password.</param>
    /// <returns>The password to use for the connection.</returns>
    private static string ResolvePassword(string username)
    {
      if (!string.IsNullOrEmpty(passwordOverride))
      {
        return passwordOverride;
      }

      Console.Write("Password for {0}: ", username);
      return ReadPassword();
    }

    /// <summary>
    /// Resolves the TimesTen data source to connect to, honoring overrides and environment settings.
    /// </summary>
    /// <returns>The chosen data source name.</returns>
    private static string ResolveDataSource()
    {
      if (!string.IsNullOrEmpty(dsnOverride))
      {
        return dsnOverride;
      }

      string envDsn = Environment.GetEnvironmentVariable("TTCONNECT") ?? string.Empty;
      if (!string.IsNullOrEmpty(envDsn))
      {
        return envDsn;
      }

      return "sampledb";
    }

    /// <summary>
    /// Builds an ODP.NET connection string for the supplied credentials and data source.
    /// </summary>
    /// <param name="username">Username for authentication.</param>
    /// <param name="password">Password associated with the username.</param>
    /// <param name="dataSource">TimesTen data source alias.</param>
    /// <returns>A formatted connection string.</returns>
    private static string BuildConnectionString(string username, string password, string dataSource)
    {
      OracleConnectionStringBuilder builder = new OracleConnectionStringBuilder
      {
        DataSource = dataSource,
        UserID = username,
        Password = password
      };

      return builder.ConnectionString;
    }

    /// <summary>
    /// Executes the end-to-end JSON workflow against the provided connection.
    /// </summary>
    /// <param name="connection">Open TimesTen connection.</param>
    private static void RunDemo(OracleConnection connection)
    {
      DropTable(connection, false);
      CreateTable(connection);

      string jsonDoc1 = ReadJsonFile(JsonDoc1);
      string jsonDoc1v2 = ReadJsonFile(JsonDoc1V2);
      string jsonDoc2 = ReadJsonFile(JsonDoc2);

      InsertRow(connection, "1600", jsonDoc1);
      InsertRow(connection, "1721", jsonDoc2);

      CreateJsonIndex(connection);

      UpdateRow(connection, "1600", jsonDoc1v2, JsonDoc1V2);

      SelectPoDocumentById(connection, "1600");
      SelectPoDocumentById(connection, "1721");

      SelectPoDocumentsByUser(connection, "ABULL");
      SelectPoDocumentsByUser(connection, "CGIRAFFE");
      SelectLineItemsForOrder(connection, "1600");

      if (keepData)
      {
        Commit(connection);
      }
      else
      {
        Rollback(connection);
      }
    }

    /// <summary>
    /// Creates the purchase order table used by the sample.
    /// </summary>
    /// <param name="connection">Active TimesTen connection.</param>
    private static void CreateTable(OracleConnection connection)
    {
      const string sql =
        "CREATE TABLE " + TableName +
        " (id VARCHAR2(32) NOT NULL PRIMARY KEY, date_loaded TIMESTAMP, po_document JSON)";

      using (OracleCommand command = connection.CreateCommand())
      {
        command.CommandText = sql;
        command.CommandType = CommandType.Text;
        command.ExecuteNonQuery();
        Console.WriteLine("Table " + TableName + " created");
      }
    }

    /// <summary>
    /// Drops the purchase order table, optionally suppressing errors for missing objects.
    /// </summary>
    /// <param name="connection">Active TimesTen connection.</param>
    /// <param name="reportMissing">True to surface missing table errors; false to ignore them.</param>
    private static void DropTable(OracleConnection connection, bool reportMissing)
    {
      const string sql = "DROP TABLE " + TableName;

      using (OracleCommand command = connection.CreateCommand())
      {
        command.CommandText = sql;
        command.CommandType = CommandType.Text;
        try
        {
          command.ExecuteNonQuery();
          Console.WriteLine("Table " + TableName + " dropped");
        }
        catch (OracleException ex)
        {
          if (reportMissing)
          {
            Console.WriteLine("Table " + TableName + " not dropped: " + ex.Message);
          }
        }
      }
    }

    /// <summary>
    /// Builds the JSON functional index used by sample queries.
    /// </summary>
    /// <param name="connection">Active TimesTen connection.</param>
    private static void CreateJsonIndex(OracleConnection connection)
    {
      const string sql =
        "CREATE INDEX idx_json_user ON " + TableName +
        " (JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128) ERROR ON ERROR))";

      using (OracleCommand command = connection.CreateCommand())
      {
        command.CommandText = sql;
        command.CommandType = CommandType.Text;
        command.ExecuteNonQuery();
        Console.WriteLine("JSON index IDX_JSON_USER created on " + TableName);
      }
    }

    /// <summary>
    /// Inserts a purchase order row populated with JSON content.
    /// </summary>
    /// <param name="connection">Active TimesTen connection.</param>
    /// <param name="id">Identifier for the purchase order.</param>
    /// <param name="json">JSON payload to insert.</param>
    private static void InsertRow(OracleConnection connection, string id, string json)
    {
      const string sql =
        "INSERT INTO " + TableName + " (id, date_loaded, po_document) VALUES (:id, :date_loaded, :po_document)";

      using (OracleCommand command = connection.CreateCommand())
      {
        command.CommandText = sql;
        command.CommandType = CommandType.Text;
        command.Parameters.Add(":id", OracleDbType.Varchar2, id, ParameterDirection.Input);
        command.Parameters.Add(":date_loaded", OracleDbType.TimeStamp, DateTime.UtcNow, ParameterDirection.Input);
        command.Parameters.Add(":po_document", OracleDbType.Clob, json, ParameterDirection.Input);
        command.ExecuteNonQuery();
        Console.WriteLine("Inserted purchase order with id " + id);
      }
    }

    /// <summary>
    /// Updates an existing purchase order with replacement JSON content.
    /// </summary>
    /// <param name="connection">Active TimesTen connection.</param>
    /// <param name="id">Identifier of the purchase order to update.</param>
    /// <param name="json">Replacement JSON payload.</param>
    /// <param name="sourcePath">Source document path used for logging.</param>
    private static void UpdateRow(OracleConnection connection, string id, string json, string sourcePath)
    {
      const string sql =
        "UPDATE " + TableName + " SET date_loaded = :date_loaded, po_document = :po_document WHERE id = :id";

      using (OracleCommand command = connection.CreateCommand())
      {
        command.CommandText = sql;
        command.CommandType = CommandType.Text;
        command.Parameters.Add(":date_loaded", OracleDbType.TimeStamp, DateTime.UtcNow, ParameterDirection.Input);
        command.Parameters.Add(":po_document", OracleDbType.Clob, json, ParameterDirection.Input);
        command.Parameters.Add(":id", OracleDbType.Varchar2, id, ParameterDirection.Input);
        int updated = command.ExecuteNonQuery();
        if (updated > 0)
        {
          Console.WriteLine("Updated purchase order with id " + id + " using " + sourcePath);
        }
        else
        {
          Console.WriteLine("No rows updated for id " + id);
        }
      }
    }

    /// <summary>
    /// Queries and prints the JSON purchase order document for the specified identifier.
    /// </summary>
    /// <param name="connection">Active TimesTen connection.</param>
    /// <param name="id">Purchase order identifier to retrieve.</param>
    private static void SelectPoDocumentById(OracleConnection connection, string id)
    {
      const string sql =
        "SELECT JSON_SERIALIZE(po_document PRETTY) FROM " + TableName + " WHERE id = :id";

      using (OracleCommand command = connection.CreateCommand())
      {
        command.CommandText = sql;
        command.CommandType = CommandType.Text;
        command.Parameters.Add(":id", OracleDbType.Varchar2, id, ParameterDirection.Input);

        using (OracleDataReader reader = command.ExecuteReader())
        {
          if (reader.Read())
          {
            Console.WriteLine("Purchase order for id " + id + ":");
            Console.WriteLine(reader.GetString(0));
          }
          else
          {
            Console.WriteLine("No purchase order found for id " + id);
          }
        }
      }
    }

    /// <summary>
    /// Retrieves and prints purchase orders whose JSON documents include the specified user.
    /// </summary>
    /// <param name="connection">Active TimesTen connection.</param>
    /// <param name="user">User identifier embedded in purchase order documents.</param>
    private static void SelectPoDocumentsByUser(OracleConnection connection, string user)
    {
      const string sql =
        "SELECT JSON_SERIALIZE(po_document PRETTY) FROM " + TableName +
        " WHERE JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128)) = :user";

      using (OracleCommand command = connection.CreateCommand())
      {
        command.CommandText = sql;
        command.CommandType = CommandType.Text;
        command.Parameters.Add(":user", OracleDbType.Varchar2, user, ParameterDirection.Input);

        using (OracleDataReader reader = command.ExecuteReader())
        {
          bool printed = false;
          while (reader.Read())
          {
            if (!printed)
            {
              Console.WriteLine("Purchase orders for user " + user + ":");
              printed = true;
            }

            Console.WriteLine(reader.GetString(0));
          }

          if (!printed)
          {
            Console.WriteLine("No purchase orders found for user " + user);
          }
        }
      }
    }

    /// <summary>
    /// Projects purchase order line items using JSON_TABLE and prints formatted results.
    /// </summary>
    /// <param name="connection">Active TimesTen connection.</param>
    /// <param name="id">Purchase order identifier to inspect.</param>
    private static void SelectLineItemsForOrder(OracleConnection connection, string id)
    {
      const string sql =
        "SELECT jt.line_number, jt.sku, jt.description, jt.quantity, jt.unit_price FROM " + TableName + " po, " +
        "JSON_TABLE(po.po_document, '$.LineItems[*]' COLUMNS (" +
        "line_number FOR ORDINALITY, " +
        "sku          VARCHAR2(40) PATH '$.Part.UPCCode', " +
        "description  VARCHAR2(256) PATH '$.Part.Description', " +
        "quantity     NUMBER PATH '$.Quantity', " +
        "unit_price   NUMBER PATH '$.Part.UnitPrice'" +
        ")) jt WHERE po.id = :id";

      using (OracleCommand command = connection.CreateCommand())
      {
        command.CommandText = sql;
        command.CommandType = CommandType.Text;
        command.Parameters.Add(":id", OracleDbType.Varchar2, id, ParameterDirection.Input);

        using (OracleDataReader reader = command.ExecuteReader())
        {
          bool printed = false;

          while (reader.Read())
          {
            if (!printed)
            {
              Console.WriteLine("Line items for purchase order " + id + ":");
              Console.WriteLine("Line  SKU           Description                     Qty  Unit Price  Extended");
              printed = true;
            }

            int lineNumber = reader.GetInt32(0);
            string sku = reader.IsDBNull(1) ? string.Empty : reader.GetString(1);
            string description = reader.IsDBNull(2) ? string.Empty : reader.GetString(2);
            decimal quantity = reader.IsDBNull(3) ? 0M : reader.GetDecimal(3);
            decimal unitPrice = reader.IsDBNull(4) ? 0M : reader.GetDecimal(4);
            decimal extended = quantity * unitPrice;

            Console.WriteLine(
              string.Format(CultureInfo.InvariantCulture,
                            "{0,4}  {1,-12} {2,-30} {3,4:G}  {4,10:F2}  {5,8:F2}",
                            lineNumber, sku, description, quantity, unitPrice, extended));
          }

          if (!printed)
          {
            Console.WriteLine("No line items found for purchase order " + id);
          }
        }
      }
    }

    /// <summary>
    /// Reads a JSON document from disk into a string.
    /// </summary>
    /// <param name="path">Relative path to the JSON file.</param>
    /// <returns>JSON contents as a string.</returns>
    private static string ReadJsonFile(string path)
    {
      string jsonPath = LocateJsonFile(path);
      return File.ReadAllText(jsonPath, Encoding.UTF8);
    }

    /// <summary>
    /// Resolves the JSON file location using relative and current directory lookups.
    /// </summary>
    /// <param name="path">Requested JSON path.</param>
    /// <returns>Resolved path when the file exists.</returns>
    private static string LocateJsonFile(string path)
    {
      if (File.Exists(path))
      {
        return path;
      }

      string local = Path.GetFileName(path);
      if (!string.IsNullOrEmpty(local) && File.Exists(local))
      {
        return local;
      }

      throw new FileNotFoundException(path);
    }

    /// <summary>
    /// Reads a password from the console while suppressing echo for interactive entry.
    /// </summary>
    /// <returns>The password captured from the console.</returns>
    private static string ReadPassword()
    {
      StringBuilder password = new StringBuilder();
      ConsoleKeyInfo keyInfo;

      while (true)
      {
        keyInfo = Console.ReadKey(true);

        if (keyInfo.Key == ConsoleKey.Enter)
        {
          Console.WriteLine();
          break;
        }

        if (keyInfo.Key == ConsoleKey.Backspace)
        {
          if (password.Length > 0)
          {
            password.Length--;
          }

          continue;
        }

        if (!char.IsControl(keyInfo.KeyChar))
        {
          password.Append(keyInfo.KeyChar);
        }
      }

      return password.ToString();
    }

    /// <summary>
    /// Commits the current transaction, retaining sample data.
    /// </summary>
    /// <param name="connection">Active TimesTen connection.</param>
    private static void Commit(OracleConnection connection)
    {
      using (OracleTransaction transaction = connection.BeginTransaction())
      {
        transaction.Commit();
        Console.WriteLine("Committed sample data");
      }
    }

    /// <summary>
    /// Rolls back pending changes to remove sample data.
    /// </summary>
    /// <param name="connection">Active TimesTen connection.</param>
    private static void Rollback(OracleConnection connection)
    {
      using (OracleTransaction transaction = connection.BeginTransaction())
      {
        transaction.Rollback();
      }
    }

    /// <summary>
    /// Prints diagnostic details for each error contained in an Oracle exception.
    /// </summary>
    /// <param name="exception">Oracle exception returned by the provider.</param>
    private static void PrintOracleException(OracleException exception)
    {
      foreach (OracleError error in exception.Errors)
      {
        Console.Error.WriteLine("OracleError: " + error.Message);
      }
    }
  }
}
