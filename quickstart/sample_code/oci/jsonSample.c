/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * DESCRIPTION
 *   Demonstrates TimesTen JSON features using the OCI driver.
 *
 */

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <oci.h>

#include "tt_version.h"

extern int chg_echo(int echo_on);

#define TABLE_NAME          "j_purchaseorder"
#define INDEX_NAME          "idx_json_user"

static const char JSON_DOC1[]    = "../common/jsondoc1.json";
static const char JSON_DOC1_V2[] = "../common/jsondoc1-v2.json";
static const char JSON_DOC2[]    = "../common/jsondoc2.json";

#define JSON_BUFFER_SIZE    (256 * 1024)
#define SERVICE_SIZE        256

#define DEFAULT_USERNAME    UIDNAME
#define DEFAULT_SERVICE     DEMODSN
#define USERNAME_SIZE       MAX_USERNAME_SIZE
#define PASSWORD_SIZE       MAX_PASSWORD_SIZE

static const char usageStr[] =
  "Usage:\tjsonSample [-h|-help|-V]\n"
  "\tjsonSample [-keep] [-user <user>] [-password <password>] [-service <tnsServiceName>]\n\n"
  "  -h, -help            Show this help and exit\n"
  "  -V                   Print the version number and exit\n"
  "  -keep                Retain table " TABLE_NAME " after the sample completes\n"
  "  -user <user>         Specify database username (defaults to " UIDNAME ")\n"
  "  -password <password> Specify database password\n"
  "  -service <name>      Specify TNS or Easy Connect name (defaults to " DEMODSN ")\n";

static char username[USERNAME_SIZE];
static char password[PASSWORD_SIZE];
static char service[SERVICE_SIZE];
static int  keepData = 0;

static OCIEnv    *envhp  = NULL;
static OCIError  *errhp  = NULL;
static OCIServer *srvhp  = NULL;
static OCISvcCtx *svchp  = NULL;
static OCISession *authp = NULL;

static void usage(const char *progname);
static void parse_args(int argc, char **argv);
static void request_password(void);
static void cleanup(void);
static void checkerr(OCIError *errhp, sword status);
static OCIStmt *prepare_statement(const char *sql);
static void drop_table(int reportMissing);
static void create_table(void);
static void create_index(void);
static void insert_row(const char *id, const char *json);
static void update_row(const char *id, const char *json, const char *sourcePath);
static void select_po_by_id(const char *id);
static void select_po_by_user(const char *userId);
static void select_line_items(const char *id);
static char *read_json(const char *path);
static void run_demo(void);

/*
 * main
 *
 * Entry point for the JSON sample. Parses user arguments, establishes the OCI
 * connection, and runs the demonstration workflow before cleaning up.
 *
 * argc - number of command-line arguments provided by the user.
 * argv - array of argument strings.
 */
int
main(int argc, char **argv)
{
  sword status;

  parse_args(argc, argv);
  (void) setlocale(LC_ALL, "");

  if (password[0] == '\0') {
    request_password();
  }

  status = OCIEnvCreate(&envhp, OCI_DEFAULT, NULL, NULL, NULL, NULL, 0, NULL);
  if (status != OCI_SUCCESS) {
    fprintf(stderr, "OCIEnvCreate failed\n");
    return 1;
  }

  if (OCIHandleAlloc(envhp, (dvoid **) &errhp, OCI_HTYPE_ERROR, 0, NULL) != OCI_SUCCESS) {
    fprintf(stderr, "Failed to allocate OCI error handle\n");
    cleanup();
    return 1;
  }

  if (OCIHandleAlloc(envhp, (dvoid **) &srvhp, OCI_HTYPE_SERVER, 0, NULL) != OCI_SUCCESS) {
    fprintf(stderr, "Failed to allocate OCI server handle\n");
    cleanup();
    return 1;
  }

  if (OCIHandleAlloc(envhp, (dvoid **) &svchp, OCI_HTYPE_SVCCTX, 0, NULL) != OCI_SUCCESS) {
    fprintf(stderr, "Failed to allocate OCI service context\n");
    cleanup();
    return 1;
  }

  status = OCIServerAttach(srvhp, errhp, (text *) service, (sb4) strlen(service), OCI_DEFAULT);
  if (status != OCI_SUCCESS) {
    checkerr(errhp, status);
    cleanup();
    return 1;
  }

  checkerr(errhp, OCIAttrSet(svchp, OCI_HTYPE_SVCCTX, srvhp, 0, OCI_ATTR_SERVER, errhp));

  if (OCIHandleAlloc(envhp, (dvoid **) &authp, OCI_HTYPE_SESSION, 0, NULL) != OCI_SUCCESS) {
    fprintf(stderr, "Failed to allocate OCI session handle\n");
    cleanup();
    return 1;
  }

  checkerr(errhp, OCIAttrSet(authp, OCI_HTYPE_SESSION, (dvoid *) username,
                             (ub4) strlen(username), OCI_ATTR_USERNAME, errhp));
  checkerr(errhp, OCIAttrSet(authp, OCI_HTYPE_SESSION, (dvoid *) password,
                             (ub4) strlen(password), OCI_ATTR_PASSWORD, errhp));

  status = OCISessionBegin(svchp, errhp, authp, OCI_CRED_RDBMS, OCI_DEFAULT);
  if (status != OCI_SUCCESS && status != OCI_SUCCESS_WITH_INFO) {
    checkerr(errhp, status);
    cleanup();
    return 1;
  }

  checkerr(errhp, OCIAttrSet(svchp, OCI_HTYPE_SVCCTX, authp, 0, OCI_ATTR_SESSION, errhp));

  printf("Connected as %s@%s\n", username, service);

  run_demo();

  if (keepData) {
    status = OCITransCommit(svchp, errhp, OCI_DEFAULT);
    if (status != OCI_SUCCESS && status != OCI_SUCCESS_WITH_INFO) {
      checkerr(errhp, status);
    } else {
      printf("Committed sample data\n");
    }
  } else {
    drop_table(1);
  }

  cleanup();
  return 0;
}

/*
 * usage
 *
 * Display command usage information and terminate the program with an error
 * status. The program name is currently unused but accepted for parity with
 * other samples.
 */
static void
usage(const char *progname)
{
  printf("%s\n", usageStr);
  (void) progname;
  exit(1);
}

/*
 * parse_args
 *
 * Parse command-line arguments, populate the global credential buffers, and
 * derive defaults when values are omitted. Terminates the program if the user
 * supplies invalid input.
 */
static void
parse_args(int argc, char **argv)
{
  int i;

  memset(username, 0, sizeof(username));
  memset(password, 0, sizeof(password));
  memset(service, 0, sizeof(service));

  strncpy(username, DEFAULT_USERNAME, sizeof(username) - 1);
  strncpy(service, DEFAULT_SERVICE, sizeof(service) - 1);

  for (i = 1; i < argc; ++i) {
    if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "-help") == 0)) {
      usage(argv[0]);
    } else if (strcmp(argv[i], "-V") == 0) {
      printf("%s\n", TTVERSION_STRING);
      exit(0);
    } else if (strcmp(argv[i], "-keep") == 0) {
      keepData = 1;
    } else if ((strcmp(argv[i], "-user") == 0) || (strcmp(argv[i], "-username") == 0)) {
      size_t len;
      if (i + 1 >= argc) {
        usage(argv[0]);
      }
      len = strlen(argv[i + 1]);
      if (len >= sizeof(username)) {
        fprintf(stderr, "Username is too long\n");
        exit(1);
      }
      strcpy(username, argv[++i]);
    } else if ((strcmp(argv[i], "-password") == 0) || (strcmp(argv[i], "-pwd") == 0)) {
      size_t len;
      if (i + 1 >= argc) {
        usage(argv[0]);
      }
      len = strlen(argv[i + 1]);
      if (len >= sizeof(password)) {
        fprintf(stderr, "Password is too long\n");
        exit(1);
      }
      strcpy(password, argv[++i]);
    } else if (strcmp(argv[i], "-service") == 0) {
      size_t len;
      if (i + 1 >= argc) {
        usage(argv[0]);
      }
      len = strlen(argv[i + 1]);
      if (len >= sizeof(service)) {
        fprintf(stderr, "Service name is too long\n");
        exit(1);
      }
      strcpy(service, argv[++i]);
    } else {
      fprintf(stderr, "Unknown option: %s\n", argv[i]);
      usage(argv[0]);
    }
  }

  if (username[0] == '\0') {
      strncpy(username, DEFAULT_USERNAME, sizeof(username) - 1);
  }
  if (service[0] == '\0') {
      strncpy(service, DEFAULT_SERVICE, sizeof(service) - 1);
  }
}

/*
 * request_password
 *
 * Prompt the user for a password using a best-effort echo suppression. Reads
 * the result into the global password buffer and strips the trailing newline.
 */
static void
request_password(void)
{
  size_t len;

  printf("Enter password for %s: ", username);
  if (!chg_echo(0)) {
    fprintf(stderr, "Unable to disable console echo\n");
  }
  if (fgets(password, sizeof(password), stdin) == NULL) {
    fprintf(stderr, "Unable to read password\n");
    exit(1);
  }
  if (!chg_echo(1)) {
    fprintf(stderr, "Unable to restore console echo\n");
  }
  printf("\n");
  len = strcspn(password, "\r\n");
  password[len] = '\0';
}

/*
 * cleanup
 *
 * Release all OCI handles, end the session, and scrub sensitive information in
 * memory prior to process exit.
 */
static void
cleanup(void)
{
  if (authp != NULL) {
    OCISessionEnd(svchp, errhp, authp, OCI_DEFAULT);
    OCIHandleFree(authp, OCI_HTYPE_SESSION);
    authp = NULL;
  }
  if (svchp != NULL) {
    OCIHandleFree(svchp, OCI_HTYPE_SVCCTX);
    svchp = NULL;
  }
  if (srvhp != NULL) {
    OCIServerDetach(srvhp, errhp, OCI_DEFAULT);
    OCIHandleFree(srvhp, OCI_HTYPE_SERVER);
    srvhp = NULL;
  }
  if (errhp != NULL) {
    OCIHandleFree(errhp, OCI_HTYPE_ERROR);
    errhp = NULL;
  }
  if (envhp != NULL) {
    OCIHandleFree(envhp, OCI_HTYPE_ENV);
    envhp = NULL;
  }
  memset(password, 0, sizeof(password));
}

/*
 * checkerr
 *
 * Translate an OCI status code into a human-readable diagnostic. Errors are
 * written to stdout to align with the existing sample conventions.
 */
static void
checkerr(OCIError *errorhp, sword status)
{
  text errbuf[512];
  sb4 errcode = 0;

  switch (status) {
    case OCI_SUCCESS:
      break;
    case OCI_SUCCESS_WITH_INFO:
      printf("OCI_SUCCESS_WITH_INFO\n");
      break;
    case OCI_NEED_DATA:
      printf("OCI_NEED_DATA\n");
      break;
    case OCI_NO_DATA:
      break;
    case OCI_ERROR:
      OCIErrorGet(errorhp, (ub4) 1, NULL, &errcode, errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
      printf("Error - %.*s\n", (int) sizeof(errbuf), (char *) errbuf);
      break;
    case OCI_INVALID_HANDLE:
      printf("OCI_INVALID_HANDLE\n");
      break;
    case OCI_STILL_EXECUTING:
      printf("OCI_STILL_EXECUTING\n");
      break;
    case OCI_CONTINUE:
      printf("OCI_CONTINUE\n");
      break;
    default:
      printf("Unknown OCI status %d\n", (int) status);
      break;
  }
}

/*
 * prepare_statement
 *
 * Allocate and prepare an OCI statement for the provided SQL text.
 *
 * Returns the prepared statement handle on success, or NULL on failure.
 */
static OCIStmt *
prepare_statement(const char *sql)
{
  OCIStmt *stmtp = NULL;
  sword status;

  if (sql == NULL) {
    fprintf(stderr, "SQL statement text is NULL\n");
    return NULL;
  }

  status = OCIHandleAlloc(envhp, (dvoid **) &stmtp, OCI_HTYPE_STMT, 0, NULL);
  if (status != OCI_SUCCESS) {
    checkerr(errhp, status);
    return NULL;
  }

  status = OCIStmtPrepare(stmtp, errhp, (text *) sql, (ub4) strlen(sql),
                          OCI_NTV_SYNTAX, OCI_DEFAULT);
  if (status != OCI_SUCCESS) {
    checkerr(errhp, status);
    OCIHandleFree(stmtp, OCI_HTYPE_STMT);
    stmtp = NULL;
  }

  return stmtp;
}

/*
 * drop_table
 *
 * Drop the purchase order table created by the demo. The reportMissing flag
 * controls whether failures are reported when the table is already absent.
 */
static void
drop_table(int reportMissing)
{
  OCIStmt *stmtp = NULL;
  sword status;

  stmtp = prepare_statement("DROP TABLE " TABLE_NAME);
  if (stmtp == NULL) {
    return;
  }

  status = OCIStmtExecute(svchp, stmtp, errhp, 1, 0, NULL, NULL, OCI_DEFAULT);
  if (status != OCI_SUCCESS && status != OCI_SUCCESS_WITH_INFO) {
    if (reportMissing) {
      checkerr(errhp, status);
    }
    OCIHandleFree(stmtp, OCI_HTYPE_STMT);
    return;
  }

  printf("Table %s dropped\n", TABLE_NAME);
  OCIHandleFree(stmtp, OCI_HTYPE_STMT);
}

/*
 * create_table
 *
 * Create the purchase order table used for JSON storage in the sample.
 */
static void
create_table(void)
{
  static const char createSql[] =
    "CREATE TABLE " TABLE_NAME " (id VARCHAR2(32) NOT NULL PRIMARY KEY, "
    "date_loaded TIMESTAMP, po_document JSON)";
  OCIStmt *stmtp = NULL;
  sword status;

  stmtp = prepare_statement(createSql);
  if (stmtp == NULL) {
    return;
  }

  status = OCIStmtExecute(svchp, stmtp, errhp, 1, 0, NULL, NULL, OCI_DEFAULT);
  if (status == OCI_SUCCESS || status == OCI_SUCCESS_WITH_INFO) {
    printf("Table %s created\n", TABLE_NAME);
  } else {
    checkerr(errhp, status);
  }

  OCIHandleFree(stmtp, OCI_HTYPE_STMT);
}

/*
 * create_index
 *
 * Create a functional JSON index that accelerates queries on the User field.
 */
static void
create_index(void)
{
  static const char indexSql[] =
    "CREATE INDEX " INDEX_NAME " ON " TABLE_NAME
    " (JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128) ERROR ON ERROR))";
  OCIStmt *stmtp = NULL;
  sword status;

  stmtp = prepare_statement(indexSql);
  if (stmtp == NULL) {
    return;
  }

  status = OCIStmtExecute(svchp, stmtp, errhp, 1, 0, NULL, NULL, OCI_DEFAULT);
  if (status == OCI_SUCCESS || status == OCI_SUCCESS_WITH_INFO) {
    printf("JSON index %s created\n", INDEX_NAME);
  } else {
    checkerr(errhp, status);
  }

  OCIHandleFree(stmtp, OCI_HTYPE_STMT);
}

/*
 * bind_json
 *
 * Convenience wrapper to bind a string placeholder to JSON text values.
 */
static void
bind_json(OCIStmt *stmtp, OCIBind **bindp, const char *placeholder, const char *value)
{
  sword status;
  sb4 length = (sb4) strlen(value);

  status = OCIBindByName(stmtp, bindp, errhp, (text *) placeholder, -1,
                         (dvoid *) value, length, SQLT_CHR,
                         NULL, NULL, NULL, 0, NULL, OCI_DEFAULT);
  if (status != OCI_SUCCESS) {
    checkerr(errhp, status);
  }
}

/*
 * insert_row
 *
 * Insert a purchase order JSON document into the table using the provided
 * identifier and JSON payload.
 */
static void
insert_row(const char *id, const char *json)
{
  static const char insertSql[] =
    "INSERT INTO " TABLE_NAME " (id, date_loaded, po_document) "
    "VALUES (:id, SYSDATE, :doc)";
  OCIStmt *stmtp = NULL;
  OCIBind *bndId = NULL;
  OCIBind *bndDoc = NULL;
  sword status;

  stmtp = prepare_statement(insertSql);
  if (stmtp == NULL) {
    return;
  }

  bind_json(stmtp, &bndId, ":id", id);
  bind_json(stmtp, &bndDoc, ":doc", json);

  status = OCIStmtExecute(svchp, stmtp, errhp, 1, 0, NULL, NULL, OCI_DEFAULT);
  if (status == OCI_SUCCESS || status == OCI_SUCCESS_WITH_INFO) {
    printf("Inserted purchase order %s\n", id);
  } else {
    checkerr(errhp, status);
  }

  OCIHandleFree(stmtp, OCI_HTYPE_STMT);
}

/*
 * update_row
 *
 * Update an existing purchase order entry with a new JSON document while
 * reporting the source JSON path to the user.
 */
static void
update_row(const char *id, const char *json, const char *sourcePath)
{
  static const char updateSql[] =
    "UPDATE " TABLE_NAME " SET date_loaded = SYSDATE, po_document = :doc WHERE id = :id";
  OCIStmt *stmtp = NULL;
  OCIBind *bndId = NULL;
  OCIBind *bndDoc = NULL;
  sword status;

  stmtp = prepare_statement(updateSql);
  if (stmtp == NULL) {
    return;
  }

  bind_json(stmtp, &bndDoc, ":doc", json);
  bind_json(stmtp, &bndId, ":id", id);

  status = OCIStmtExecute(svchp, stmtp, errhp, 1, 0, NULL, NULL, OCI_DEFAULT);
  if (status == OCI_SUCCESS || status == OCI_SUCCESS_WITH_INFO) {
    printf("Updated purchase order %s using %s\n", id, sourcePath);
  } else {
    checkerr(errhp, status);
  }

  OCIHandleFree(stmtp, OCI_HTYPE_STMT);
}

/*
 * select_po_by_id
 *
 * Fetch and print a pretty-formatted purchase order for the specified id.
 */
static void
select_po_by_id(const char *id)
{
  static const char selectSql[] =
    "SELECT JSON_SERIALIZE(po_document PRETTY) FROM " TABLE_NAME " WHERE id = :id";
  OCIStmt *stmtp = NULL;
  OCIBind *bndId = NULL;
  OCIDefine *defDoc = NULL;
  sword status;
  char *buffer;
  int fetched = 0;
  ub2 docLen = 0;

  buffer = (char *) malloc(JSON_BUFFER_SIZE);
  if (buffer == NULL) {
    fprintf(stderr, "Out of memory allocating JSON buffer\n");
    return;
  }

  stmtp = prepare_statement(selectSql);
  if (stmtp == NULL) {
    free(buffer);
    return;
  }

  bind_json(stmtp, &bndId, ":id", id);

  status = OCIDefineByPos(stmtp, &defDoc, errhp, 1, buffer, JSON_BUFFER_SIZE,
                          SQLT_CHR, NULL, &docLen, NULL, OCI_DEFAULT);
  if (status != OCI_SUCCESS) {
    checkerr(errhp, status);
    free(buffer);
    OCIHandleFree(stmtp, OCI_HTYPE_STMT);
    return;
  }

  status = OCIStmtExecute(svchp, stmtp, errhp, 0, 0, NULL, NULL, OCI_DEFAULT);
  if (status != OCI_SUCCESS && status != OCI_SUCCESS_WITH_INFO) {
    checkerr(errhp, status);
    free(buffer);
    OCIHandleFree(stmtp, OCI_HTYPE_STMT);
    return;
  }

  while ((status = OCIStmtFetch(stmtp, errhp, 1, OCI_FETCH_NEXT, OCI_DEFAULT)) == OCI_SUCCESS ||
         status == OCI_SUCCESS_WITH_INFO) {
    if (!fetched) {
      printf("Purchase order for id %s:\n", id);
      fetched = 1;
    }
    size_t printLen = docLen;
    if (printLen >= JSON_BUFFER_SIZE) {
      printLen = JSON_BUFFER_SIZE - 1;
    }
    buffer[printLen] = '\0';
    printf("%s\n", buffer);
  }

  if (!fetched) {
    printf("No purchase order found for id %s\n", id);
  } else if (status != OCI_NO_DATA) {
    checkerr(errhp, status);
  }

  free(buffer);
  OCIHandleFree(stmtp, OCI_HTYPE_STMT);
}

/*
 * select_po_by_user
 *
 * Retrieve purchase orders matching a user identifier extracted from JSON
 * content and print each order in a human-readable format.
 */
static void
select_po_by_user(const char *userId)
{
  static const char selectSql[] =
    "SELECT JSON_SERIALIZE(po_document PRETTY) FROM " TABLE_NAME
    " WHERE JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128)) = :user";
  OCIStmt *stmtp = NULL;
  OCIBind *bndUser = NULL;
  OCIDefine *defDoc = NULL;
  sword status;
  char *buffer;
  int printed = 0;
  ub2 docLen = 0;

  buffer = (char *) malloc(JSON_BUFFER_SIZE);
  if (buffer == NULL) {
    fprintf(stderr, "Out of memory allocating JSON buffer\n");
    return;
  }

  stmtp = prepare_statement(selectSql);
  if (stmtp == NULL) {
    free(buffer);
    return;
  }

  bind_json(stmtp, &bndUser, ":user", userId);

  status = OCIDefineByPos(stmtp, &defDoc, errhp, 1, buffer, JSON_BUFFER_SIZE,
                          SQLT_CHR, NULL, &docLen, NULL, OCI_DEFAULT);
  if (status != OCI_SUCCESS) {
    checkerr(errhp, status);
    free(buffer);
    OCIHandleFree(stmtp, OCI_HTYPE_STMT);
    return;
  }

  status = OCIStmtExecute(svchp, stmtp, errhp, 0, 0, NULL, NULL, OCI_DEFAULT);
  if (status != OCI_SUCCESS && status != OCI_SUCCESS_WITH_INFO) {
    checkerr(errhp, status);
    free(buffer);
    OCIHandleFree(stmtp, OCI_HTYPE_STMT);
    return;
  }

  while ((status = OCIStmtFetch(stmtp, errhp, 1, OCI_FETCH_NEXT, OCI_DEFAULT)) == OCI_SUCCESS ||
         status == OCI_SUCCESS_WITH_INFO) {
    if (!printed) {
      printf("Purchase orders for user %s:\n", userId);
      printed = 1;
    }
    size_t printLen = docLen;
    if (printLen >= JSON_BUFFER_SIZE) {
      printLen = JSON_BUFFER_SIZE - 1;
    }
    buffer[printLen] = '\0';
    printf("%s\n", buffer);
  }

  if (!printed) {
    printf("No purchase orders found for user %s\n", userId);
  } else if (status != OCI_NO_DATA) {
    checkerr(errhp, status);
  }

  free(buffer);
  OCIHandleFree(stmtp, OCI_HTYPE_STMT);
}

/*
 * select_line_items
 *
 * Use JSON_TABLE to extract individual line items for a purchase order and
 * render them as a tabular report.
 */
static void
select_line_items(const char *id)
{
  static const char selectSql[] =
    "SELECT jt.line_number, jt.sku, jt.description, jt.quantity, jt.unit_price "
    "FROM " TABLE_NAME " po, "
    "JSON_TABLE(po.po_document, '$.LineItems[*]' COLUMNS ("
    "line_number FOR ORDINALITY, "
    "sku VARCHAR2(40) PATH '$.Part.UPCCode', "
    "description VARCHAR2(256) PATH '$.Part.Description', "
    "quantity NUMBER PATH '$.Quantity', "
    "unit_price NUMBER PATH '$.Part.UnitPrice')) jt WHERE po.id = :id";
  OCIStmt *stmtp = NULL;
  OCIBind *bndId = NULL;
  OCIDefine *defLine = NULL;
  OCIDefine *defSku = NULL;
  OCIDefine *defDesc = NULL;
  OCIDefine *defQty = NULL;
  OCIDefine *defPrice = NULL;
  sword status;
  sb4 lineNumber = 0;
  char sku[41];
  char description[257];
  double quantity = 0.0;
  double unitPrice = 0.0;
  int printed = 0;
  ub2 skuLen = 0;
  ub2 descLen = 0;

  stmtp = prepare_statement(selectSql);
  if (stmtp == NULL) {
    return;
  }

  bind_json(stmtp, &bndId, ":id", id);

  status = OCIDefineByPos(stmtp, &defLine, errhp, 1, &lineNumber, (sb4) sizeof(lineNumber),
                          SQLT_INT, NULL, NULL, NULL, OCI_DEFAULT);
  status = (status == OCI_SUCCESS) ? OCIDefineByPos(stmtp, &defSku, errhp, 2, sku, (sb4) sizeof(sku),
                                                    SQLT_CHR, NULL, &skuLen, NULL, OCI_DEFAULT) : status;
  status = (status == OCI_SUCCESS) ? OCIDefineByPos(stmtp, &defDesc, errhp, 3, description, (sb4) sizeof(description),
                                                    SQLT_CHR, NULL, &descLen, NULL, OCI_DEFAULT) : status;
  status = (status == OCI_SUCCESS) ? OCIDefineByPos(stmtp, &defQty, errhp, 4, &quantity, (sb4) sizeof(quantity),
                                                    SQLT_FLT, NULL, NULL, NULL, OCI_DEFAULT) : status;
  status = (status == OCI_SUCCESS) ? OCIDefineByPos(stmtp, &defPrice, errhp, 5, &unitPrice, (sb4) sizeof(unitPrice),
                                                    SQLT_FLT, NULL, NULL, NULL, OCI_DEFAULT) : status;
  if (status != OCI_SUCCESS) {
    checkerr(errhp, status);
    OCIHandleFree(stmtp, OCI_HTYPE_STMT);
    return;
  }

  status = OCIStmtExecute(svchp, stmtp, errhp, 0, 0, NULL, NULL, OCI_DEFAULT);
  if (status != OCI_SUCCESS && status != OCI_SUCCESS_WITH_INFO) {
    checkerr(errhp, status);
    OCIHandleFree(stmtp, OCI_HTYPE_STMT);
    return;
  }

  while ((status = OCIStmtFetch(stmtp, errhp, 1, OCI_FETCH_NEXT, OCI_DEFAULT)) == OCI_SUCCESS ||
         status == OCI_SUCCESS_WITH_INFO) {
    if (!printed) {
      printf("Line items for purchase order %s:\n", id);
      printf("Line  SKU           Description                     Qty  Unit Price  Extended\n");
      printed = 1;
    }
    size_t safeSkuLen = skuLen;
    size_t safeDescLen = descLen;
    if (safeSkuLen >= sizeof(sku)) {
      safeSkuLen = sizeof(sku) - 1;
    }
    if (safeDescLen >= sizeof(description)) {
      safeDescLen = sizeof(description) - 1;
    }
    sku[safeSkuLen] = '\0';
    description[safeDescLen] = '\0';

    printf("%4d  %-12s %-30s %4.0f  %10.2f  %8.2f\n",
           (int) lineNumber, sku, description, quantity, unitPrice, quantity * unitPrice);
  }

  if (!printed) {
    printf("No line items found for purchase order %s\n", id);
  } else if (status != OCI_NO_DATA) {
    checkerr(errhp, status);
  }

  OCIHandleFree(stmtp, OCI_HTYPE_STMT);
}

/*
 * read_json
 *
 * Load the JSON payload from the supplied path into a heap buffer. The caller
 * is responsible for freeing the returned pointer.
 */
static char *
read_json(const char *path)
{
  FILE *fp = NULL;
  long size = 0;
  char *buffer = NULL;
  size_t bytes;

  fp = fopen(path, "rb");
  if (fp == NULL) {
    fprintf(stderr, "Unable to open %s\n", path);
    return NULL;
  }

  if (fseek(fp, 0, SEEK_END) != 0) {
    fprintf(stderr, "Unable to determine size of %s\n", path);
    fclose(fp);
    return NULL;
  }

  size = ftell(fp);
  if (size < 0) {
    fprintf(stderr, "Unable to determine size of %s\n", path);
    fclose(fp);
    return NULL;
  }

  if (size > JSON_BUFFER_SIZE) {
    fprintf(stderr, "%s exceeds max JSON buffer (%d bytes)\n", path, JSON_BUFFER_SIZE);
  }

  rewind(fp);

  buffer = (char *) malloc((size_t) size + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Out of memory reading %s\n", path);
    fclose(fp);
    return NULL;
  }

  bytes = fread(buffer, 1, (size_t) size, fp);
  if (bytes != (size_t) size) {
    fprintf(stderr, "Unable to read %s\n", path);
    free(buffer);
    fclose(fp);
    return NULL;
  }
  buffer[size] = '\0';

  fclose(fp);
  return buffer;
}

/*
 * run_demo
 *
 * Execute the JSON walkthrough: set up schema objects, load sample documents,
 * update and query data, and finally display line-item analytics.
 */
static void
run_demo(void)
{
  char *json1 = NULL;
  char *json1v2 = NULL;
  char *json2 = NULL;

  drop_table(0);
  create_table();

  json1 = read_json(JSON_DOC1);
  json1v2 = read_json(JSON_DOC1_V2);
  json2 = read_json(JSON_DOC2);

  if ((json1 == NULL) || (json1v2 == NULL) || (json2 == NULL)) {
    free(json1);
    free(json1v2);
    free(json2);
    return;
  }

  insert_row("1600", json1);
  insert_row("1721", json2);

  create_index();

  update_row("1600", json1v2, JSON_DOC1_V2);

  select_po_by_id("1600");
  select_po_by_id("1721");

  select_po_by_user("ABULL");
  select_po_by_user("CGIRAFFE");
  select_line_items("1600");

  free(json1);
  free(json1v2);
  free(json2);
}
