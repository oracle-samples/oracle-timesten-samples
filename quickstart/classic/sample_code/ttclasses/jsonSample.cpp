/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 *  DESCRIPTION
 *    Demonstrates TimesTen JSON features using the TTClasses driver.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <limits.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#ifndef R_OK
#define R_OK 4
#endif
#define access _access
#else
#include <unistd.h>
#include <termios.h>
#endif

#include <ttclasses/TTInclude.h>
#include "testprog_utils.h"
#include "../common/tt_version.h"

using std::cerr;
using std::cout;
using std::endl;
using std::ifstream;

namespace
{

/// Enables or disables console echo for password entry.
/// Returns 1 on success and 0 on failure.
int
chg_echo(int echo_on)
{
#ifdef _WIN32
  HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
  if (handle == INVALID_HANDLE_VALUE)
  {
    return 0;
  }

  DWORD mode = 0;
  if (!GetConsoleMode(handle, &mode))
  {
    return 0;
  }

  if (echo_on)
  {
    mode |= ENABLE_ECHO_INPUT;
  }
  else
  {
    mode &= ~ENABLE_ECHO_INPUT;
  }

  return SetConsoleMode(handle, mode) ? 1 : 0;
#else
  struct termios settings;
  if (tcgetattr(STDIN_FILENO, &settings) == -1)
  {
    return 0;
  }

  if (echo_on)
  {
    settings.c_lflag |= ECHO;
    settings.c_lflag &= ~ECHONL;
  }
  else
  {
    settings.c_lflag &= ~ECHO;
    settings.c_lflag |= ECHONL;
  }

  return tcsetattr(STDIN_FILENO, TCSADRAIN, &settings) == -1 ? 0 : 1;
#endif
}

/// Throws a `TTStatus` configured to represent an `SQL_ERROR`.
[[noreturn]] void
ThrowSqlError()
{
  TTStatus status;
  status.rc = SQL_ERROR;
  throw status;
}

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

} // namespace

namespace
{
const char *PROGRAM_NAME = "jsonSample";
const char *TABLE_NAME    = "j_purchaseorder";
const char *INDEX_NAME    = "idx_json_user";

const char *JSON_DOC1     = "../common/jsondoc1.json";
const char *JSON_DOC1_V2  = "../common/jsondoc1-v2.json";
const char *JSON_DOC2     = "../common/jsondoc2.json";

constexpr size_t JSON_MAX_LENGTH = 256 * 1024; // 256 KB buffer for JSON documents
constexpr const char USAGE_SUFFIX[] =
  " [-keep] [-client] [-user <user>] [-password <password>]\n"
  "            [<DSN> | -connStr <connection string>]\n";

// Command-line flags
bool keepData        = false;
bool doClientConnect = false;

// Credential overrides
char userOverride[MAX_USERNAME_SIZE + 1];
char passwordOverride[MAX_PASSWORD_SIZE + 1];

// Forward declarations
void ParseCommandLine(int argc, char **argv, char *connStr, size_t connStrLen);
void PrintUsageAndExit(const char *progname, bool error = true);
void EnsureCredentials(char *username, char *password);
void ReadPassword(const char *prompt, char *target, size_t maxlen);
void RunSample(TTConnection &conn);
void DropTable(TTConnection &conn, bool reportMissing);
void CreateTable(TTConnection &conn);
void CreateJsonIndex(TTConnection &conn);
void InsertRow(TTConnection &conn, const char *id, const char *json);
void UpdateRow(TTConnection &conn, const char *id, const char *json, const char *sourcePath);
void SelectPoById(TTConnection &conn, const char *id);
void SelectPoByUser(TTConnection &conn, const char *userId);
void SelectLineItems(TTConnection &conn, const char *id);
char *ReadJsonDocument(const char *path, char *buffer, size_t bufferSize);
const char *LocateJsonFile(const char *path, char *resolved, size_t resolvedLen);
void PrintResultJson(const char *label, const char *json);

} // namespace

/// Entry point for the JSON sample program.
int
main(int argc, char **argv)
{
  // ---------------------------------------------------------------
  // Parse command-line arguments and resolve connection string
  // ---------------------------------------------------------------
  char connStr[256];
  memset(connStr, 0, sizeof(connStr));
  memset(userOverride, 0, sizeof(userOverride));
  memset(passwordOverride, 0, sizeof(passwordOverride));

  ParseCommandLine(argc, argv, connStr, sizeof(connStr));

  cerr << endl << "Connecting to TimesTen <" << connStr << ">" << endl;

  // ---------------------------------------------------------------
  // Configure TTClasses logging and signal handling (common pattern)
  // ---------------------------------------------------------------
  TTGlobal::setLogStream(STDCERR);
  TTGlobal::setLogLevel(TTLog::TTLOG_ERR);

  if (HandleSignals())
  {
    cerr << "Could not set up signal handling.  Aborting." << endl;
    exit(-1);
  }

  // ---------------------------------------------------------------
  // Establish connection using TTConnection (direct or client mode)
  // ---------------------------------------------------------------
  TTConnection conn;
  try
  {
    char username[MAX_USERNAME_SIZE + 1];
    char password[MAX_PASSWORD_SIZE + 1];
    EnsureCredentials(username, password);

    if (doClientConnect)
    {
      conn.Connect(connStr, username, password);
    }
    else
    {
      conn.Connect(connStr, TTConnection::DRIVER_COMPLETE);
    }
  }
  catch (TTWarning warn)
  {
    cerr << "Warning connecting to TimesTen:" << endl << warn << endl;
  }
  catch (TTError err)
  {
    cerr << "Error connecting to TimesTen:" << endl << err << endl;
    if (err.rc != SQL_SUCCESS_WITH_INFO)
    {
      exit(-1);
    }
  }

  try
  {
    RunSample(conn);

    if (keepData)
    {
      conn.Commit();
      cout << "Committed sample data (table retained by -keep option)" << endl;
    }
    else
    {
      DropTable(conn, true);
      conn.Commit();
    }
  }
  catch (TTStatus status)
  {
    cerr << "Error executing JSON sample: " << status << endl;
    try
    {
      conn.Rollback();
    }
    catch (TTStatus rollbackStatus)
    {
      cerr << "Rollback failed: " << rollbackStatus << endl;
    }
  }

  try
  {
    conn.Disconnect();
  }
  catch (TTStatus status)
  {
    cerr << "Error disconnecting: " << status << endl;
  }

  return 0;
}

namespace
{

/// Parses command-line arguments and initializes global runtime flags.
void
ParseCommandLine(int argc, char **argv, char *connStr, size_t connStrLen)
{
  std::string usage_string = std::string(" ") + PROGRAM_NAME + USAGE_SUFFIX;

  ParamParser parser(usage_string.c_str());
  parser.setArg("-keep", false, 0);
  parser.setArg("-client", false, 0);
  parser.setArg("-user", false, MAX_USERNAME_SIZE);
  parser.setArg("-username", false, MAX_USERNAME_SIZE);
  parser.setArg("-password", false, MAX_PASSWORD_SIZE);
  parser.setArg("-pwd", false, MAX_PASSWORD_SIZE);

  parser.processArgs(argc, argv, connStr);

  if (parser.argUsed("-keep"))
  {
    keepData = true;
  }
  if (parser.argUsed("-client"))
  {
    doClientConnect = true;
  }

  if (parser.argUsed("-user"))
  {
    strncpy(userOverride, parser.getArgValue("-user"), sizeof(userOverride) - 1);
  }
  else if (parser.argUsed("-username"))
  {
    strncpy(userOverride, parser.getArgValue("-username"), sizeof(userOverride) - 1);
  }

  if (parser.argUsed("-password"))
  {
    strncpy(passwordOverride, parser.getArgValue("-password"), sizeof(passwordOverride) - 1);
  }
  else if (parser.argUsed("-pwd"))
  {
    strncpy(passwordOverride, parser.getArgValue("-pwd"), sizeof(passwordOverride) - 1);
  }

  if (!*connStr)
  {
    snprintf(connStr, connStrLen, "dsn=%s;%s", doClientConnect ? "sampledbCS" : DEMODSN, UID);
  }
}

/// Displays usage information and terminates the process with the appropriate status code.
void
PrintUsageAndExit(const char *progname, bool error)
{
  const char *program = (progname != NULL && *progname != '\0') ? progname : PROGRAM_NAME;
  std::ostream &stream = error ? cerr : cout;

  stream << "Usage: " << program << USAGE_SUFFIX;
  stream.flush();

  exit(error ? EXIT_FAILURE : EXIT_SUCCESS);
}

/// Resolves the credentials used for the TimesTen connection.
void
EnsureCredentials(char *username, char *password)
{
  memset(username, 0, MAX_USERNAME_SIZE + 1);
  memset(password, 0, MAX_PASSWORD_SIZE + 1);

  if (*userOverride)
  {
    strncpy(username, userOverride, MAX_USERNAME_SIZE);
  }
  else
  {
    strncpy(username, UIDNAME, MAX_USERNAME_SIZE);
  }

  if (*passwordOverride)
  {
    strncpy(password, passwordOverride, MAX_PASSWORD_SIZE);
    return;
  }

  if (doClientConnect)
  {
    ReadPassword("Enter password: ", password, MAX_PASSWORD_SIZE + 1);
  }
  else
  {
    strncpy(password, UIDNAME, MAX_PASSWORD_SIZE);
  }
}

/// Prompts for a password while temporarily disabling console echo.
void
ReadPassword(const char *prompt, char *target, size_t maxlen)
{
  cout << prompt;
  cout.flush();

  if (!chg_echo(0))
  {
    cerr << "Unable to disable console echo" << endl;
  }

  if (!fgets(target, static_cast<int>(maxlen), stdin))
  {
    cerr << "Unable to read password" << endl;
    exit(1);
  }

  if (!chg_echo(1))
  {
    cerr << "Unable to restore console echo" << endl;
  }

  cout << endl;

  size_t len = strcspn(target, "\r\n");
  target[len] = '\0';
}

/// Executes the end-to-end JSON sample workflow using the provided connection.
void
RunSample(TTConnection &conn)
{
  cout << endl;
  DropTable(conn, false);
  CreateTable(conn);

  char jsonBuffer[JSON_MAX_LENGTH];
  char resolvedPath[PATH_MAX];

  const char *doc1Path = LocateJsonFile(JSON_DOC1, resolvedPath, sizeof(resolvedPath));
  char *doc1Buffer = ReadJsonDocument(doc1Path, jsonBuffer, sizeof(jsonBuffer));
  std::string doc1(doc1Buffer ? doc1Buffer : "");
  const char *doc2Path = LocateJsonFile(JSON_DOC2, resolvedPath, sizeof(resolvedPath));
  char *doc2Buffer = ReadJsonDocument(doc2Path, jsonBuffer, sizeof(jsonBuffer));
  std::string doc2(doc2Buffer ? doc2Buffer : "");
  const char *doc1v2Path = LocateJsonFile(JSON_DOC1_V2, resolvedPath, sizeof(resolvedPath));
  char *doc1v2Buffer = ReadJsonDocument(doc1v2Path, jsonBuffer, sizeof(jsonBuffer));
  std::string doc1v2(doc1v2Buffer ? doc1v2Buffer : "");

  InsertRow(conn, "1600", doc1.c_str());
  InsertRow(conn, "1721", doc2.c_str());

  CreateJsonIndex(conn);

  UpdateRow(conn, "1600", doc1v2.c_str(), doc1v2Path);

  SelectPoById(conn, "1600");
  SelectPoById(conn, "1721");

  SelectPoByUser(conn, "ABULL");
  SelectPoByUser(conn, "CGIRAFFE");

  SelectLineItems(conn, "1600");
}

/// Drops the sample table if present, optionally reporting when it is missing.
void
DropTable(TTConnection &conn, bool reportMissing)
{
  TTCmd cmd;
  try
  {
    cmd.ExecuteImmediate(&conn, "DROP TABLE j_purchaseorder");
    cout << "Table " << TABLE_NAME << " dropped" << endl;
  }
  catch (TTStatus status)
  {
    if (reportMissing)
    {
      cerr << "Table " << TABLE_NAME << " not dropped: " << status << endl;
    }
  }
  cmd.Drop();
}

/// Creates the JSON-backed purchase order table used by the sample.
void
CreateTable(TTConnection &conn)
{
  TTCmd cmd;
  try
  {
    cmd.ExecuteImmediate(&conn,
                         "CREATE TABLE j_purchaseorder ("
                         "id VARCHAR2(32) NOT NULL PRIMARY KEY,"
                         " date_loaded TIMESTAMP,"
                         " po_document JSON)");
    cout << "Table " << TABLE_NAME << " created" << endl;
  }
  catch (TTStatus status)
  {
    cerr << "Unable to create table " << TABLE_NAME << ": " << status << endl;
    throw;
  }
  cmd.Drop();
}

/// Builds an index on the JSON `User` field to accelerate lookups.
void
CreateJsonIndex(TTConnection &conn)
{
  TTCmd cmd;
  try
  {
    char statement[512];
    snprintf(statement, sizeof(statement),
             "CREATE INDEX %s ON j_purchaseorder (JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128) ERROR ON ERROR))",
             INDEX_NAME);
    cmd.ExecuteImmediate(&conn, statement);
    cout << "JSON index " << INDEX_NAME << " created" << endl;
  }
  catch (TTStatus status)
  {
    cerr << "Unable to create JSON index: " << status << endl;
    throw;
  }
  cmd.Drop();
}

/// Inserts a JSON purchase order row identified by the supplied ID.
void
InsertRow(TTConnection &conn, const char *id, const char *json)
{
  TTCmd stmt;
  try
  {
    stmt.Prepare(&conn,
                 "INSERT INTO j_purchaseorder (id, date_loaded, po_document)"
                 " VALUES(?, SYSDATE, ?)");
    stmt.setParam(1, id);
    stmt.setParam(2, json);
    stmt.Execute();
    cout << "Inserted purchase order " << id << endl;
    stmt.Close();
  }
  catch (TTStatus status)
  {
    cerr << "Unable to insert purchase order " << id << ": " << status << endl;
    throw;
  }
  stmt.Drop();
}

/// Replaces the JSON document for an existing purchase order ID.
void
UpdateRow(TTConnection &conn, const char *id, const char *json, const char *sourcePath)
{
  TTCmd stmt;
  try
  {
    stmt.Prepare(&conn,
                 "UPDATE j_purchaseorder SET date_loaded = SYSDATE, po_document = ? WHERE id = ?");
    stmt.setParam(1, json);
    stmt.setParam(2, id);
    stmt.Execute();
    stmt.Close();
    cout << "Updated purchase order " << id << " using " << sourcePath << endl;
  }
  catch (TTStatus status)
  {
    cerr << "Unable to update purchase order " << id << ": " << status << endl;
    throw;
  }
  stmt.Drop();
}

/// Fetches and prints the purchase order JSON for a specific ID.
void
SelectPoById(TTConnection &conn, const char *id)
{
  TTCmd stmt;
  try
  {
    stmt.Prepare(&conn,
                 "SELECT JSON_SERIALIZE(po_document PRETTY) FROM j_purchaseorder WHERE id = ?");
    stmt.setParam(1, id);
    stmt.Execute();

    bool printed = false;
    while (!stmt.FetchNext())
    {
      char *jsonDoc = NULL;
      stmt.getColumn(1, &jsonDoc);
      if (!printed)
      {
        cout << "Purchase order for id " << id << ":" << endl;
        printed = true;
      }
      PrintResultJson(NULL, jsonDoc);
    }

    if (!printed)
    {
      cout << "No purchase order found for id " << id << endl;
    }

    stmt.Close();
  }
  catch (TTStatus status)
  {
    cerr << "Error selecting purchase order " << id << ": " << status << endl;
    throw;
  }
  stmt.Drop();
}

/// Fetches and prints purchase orders whose JSON documents match the user ID.
void
SelectPoByUser(TTConnection &conn, const char *userId)
{
  TTCmd stmt;
  try
  {
    stmt.Prepare(&conn,
                 "SELECT JSON_SERIALIZE(po_document PRETTY) FROM j_purchaseorder "
                 "WHERE JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128)) = ?");
    stmt.setParam(1, userId);
    stmt.Execute();

    bool printed = false;
    while (!stmt.FetchNext())
    {
      char *jsonDoc = NULL;
      stmt.getColumn(1, &jsonDoc);
      if (!printed)
      {
        cout << "Purchase orders for user " << userId << ":" << endl;
        printed = true;
      }
      PrintResultJson(NULL, jsonDoc);
    }

    if (!printed)
    {
      cout << "No purchase orders found for user " << userId << endl;
    }

    stmt.Close();
  }
  catch (TTStatus status)
  {
    cerr << "Error selecting purchase orders for user " << userId << ": " << status << endl;
    throw;
  }
  stmt.Drop();
}

/// Unnests JSON line items for the purchase order and prints a formatted table.
void
SelectLineItems(TTConnection &conn, const char *id)
{
  TTCmd stmt;
  try
  {
    stmt.Prepare(&conn,
                 "SELECT jt.line_number, jt.sku, jt.description, jt.quantity, jt.unit_price "
                 "FROM j_purchaseorder po, "
                 "JSON_TABLE(po.po_document, '$.LineItems[*]' COLUMNS ("
                 " line_number FOR ORDINALITY,"
                 " sku VARCHAR2(40) PATH '$.Part.UPCCode',"
                 " description VARCHAR2(256) PATH '$.Part.Description',"
                 " quantity NUMBER PATH '$.Quantity',"
                 " unit_price NUMBER PATH '$.Part.UnitPrice')) jt"
                 " WHERE po.id = ?");
    stmt.setParam(1, id);
    stmt.Execute();

    bool printed = false;
    while (!stmt.FetchNext())
    {
      double lineNumber = 0.0;
      char *sku = NULL;
      char *description = NULL;
      double quantity = 0.0;
      double unitPrice = 0.0;

      stmt.getColumn(1, &lineNumber);
      stmt.getColumn(2, &sku);
      stmt.getColumn(3, &description);
      stmt.getColumn(4, &quantity);
      stmt.getColumn(5, &unitPrice);

      if (!printed)
      {
        cout << "Line items for purchase order " << id << ":" << endl;
        cout << "Line  SKU           Description                     Qty  Unit Price  Extended" << endl;
        printed = true;
      }

      double extended = quantity * unitPrice;
      int lineNumberDisplay = static_cast<int>(lineNumber);
      cout << std::setw(4) << lineNumberDisplay << "  "
           << std::left << std::setw(12) << (sku ? sku : "")
           << " " << std::setw(30) << (description ? description : "")
           << std::right << std::setw(4) << static_cast<int>(quantity)
           << "  " << std::setw(10) << std::fixed << std::setprecision(2) << unitPrice
           << "  " << std::setw(8) << std::fixed << std::setprecision(2) << extended
           << endl;
    }

    if (!printed)
    {
      cout << "No line items found for purchase order " << id << endl;
    }

    stmt.Close();
  }
  catch (TTStatus status)
  {
    cerr << "Error selecting line items for " << id << ": " << status << endl;
    throw;
  }
  stmt.Drop();
}

/// Loads a JSON document from disk into the provided buffer.

/// Prints the supplied JSON document with an optional label prefix.
void
PrintResultJson(const char *label, const char *json)
{
  if (label != NULL)
  {
    cout << label << endl;
  }

  if (json != NULL)
  {
    cout << json << endl;
  }
  else
  {
    cout << "<null JSON document>" << endl;
  }
}

char *
ReadJsonDocument(const char *path, char *buffer, size_t bufferSize)
{
  ifstream input(path, std::ios::in | std::ios::binary);
  if (!input)
  {
    cerr << "Unable to open JSON document: " << path << endl;
    ThrowSqlError();
  }

  input.read(buffer, static_cast<std::streamsize>(bufferSize - 1));
  std::streamsize bytesRead = input.gcount();
  if (!input && !input.eof())
  {
    cerr << "Error reading JSON document: " << path << endl;
    ThrowSqlError();
  }

  buffer[bytesRead] = '\0';
  return buffer;
}

/// Resolves relative JSON file paths and validates read access.
const char *
LocateJsonFile(const char *path, char *resolved, size_t resolvedLen)
{
  if (path == NULL || resolved == NULL)
  {
    ThrowSqlError();
  }

  strncpy(resolved, path, resolvedLen - 1);
  resolved[resolvedLen - 1] = '\0';

  if (access(resolved, R_OK) == 0)
  {
    return resolved;
  }

  const char *basename = strrchr(path, '/');
  if (basename != NULL)
  {
    ++basename;
  }
  else
  {
    basename = path;
  }

  strncpy(resolved, basename, resolvedLen - 1);
  resolved[resolvedLen - 1] = '\0';

  if (access(resolved, R_OK) == 0)
  {
    return resolved;
  }

  cerr << "JSON document not found: " << path << endl;
  ThrowSqlError();
}

} // namespace
