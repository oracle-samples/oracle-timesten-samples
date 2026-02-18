Copyright (c) 2010, 2026, Oracle and/or its affiliates. All rights reserved.

# How to Run the SQL Sample Code

The SQL samples demonstrate features of Oracle TimesTen that can be exercised directly from `ttIsql` scripts. The `jsonSample.sql` program showcases TimesTen native JSON functionality, including table creation, JSON_VALUE indexing, document updates, JSON_TABLE projections, and cleanup logic executed entirely through SQL statements.

## IMPORTANT PRE-REQUISITES

1. Manually configure the sample DSN for the sample programs; refer to `quickstart/classic/html/developer/sample_dsn_setup.html`.

2. Set up the environment to run the sample application.

   The following scripts must be run in each terminal session:

   Set up the TimesTen instance environment variables (for example, if your TimesTen instance is under `/home/timesten/instance/tt261`):

   `source /home/timesten/instance/tt261/bin/ttenv.sh`

   Set up Quick Start environment variables:

   Unix/Linux:

   `. quickstart/classic/ttquickstartenv.sh`

   or

   `source quickstart/classic/ttquickstartenv.csh`

## How to run the sample SQL code

Run the SQL sample code using `ttIsql`. You can either source the script interactively or run it in batch mode.

Interactive example:

`ttIsql "dsn=sampledb;uid=appuser"
Enter the password for appuser
Command> @jsonSample.sql;`

Batch example:

`ttIsql -f jsonSample.sql "dsn=sampledb;uid=appuser"`

_Sample Description_

**jsonSample.sql**

- Drops and recreates the `j_purchaseorder` table with a JSON column
- Loads JSON purchase orders from the shared Quick Start JSON files
- Builds a JSON_VALUE-based functional index for user lookups
- Demonstrates JSON_VALUE query behavior with matching and mismatched RETURNING types
- Updates a purchase order with revised JSON and validates the stored document via JSON_EQUAL
- Pretty-prints purchase orders, filters by JSON attributes, and flattens arrays with `JSON_TABLE`
- Performs analytic queries such as aggregating line item quantities and pattern matching on JSON content
- Cleans up by dropping the purchase-order table (unless you modify the script to skip teardown)

This sample should be executed from `ttIsql` so that parameter bindings (such as `:JSON`) and JSON file loads operate correctly.

For more information on SQL programming with Oracle TimesTen, refer to the [Oracle TimesTen In-Memory Database SQL Reference](https://docs.oracle.com/en/database/other-databases/timesten/22.1/sql-reference/index.html).
