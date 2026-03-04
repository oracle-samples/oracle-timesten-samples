Copyright (c) 2010, 2021, Oracle and/or its affiliates. All rights reserved.

# Compile and Run ODP.NET Sample Program

Windows is a client only platform for TimesTen; you cannot host a TimesTen database on Windows. You can access a TimesTen database, running on another machine, from a Windows machine, using the TimesTen Windows client. 

To use this sample you will need access to a TimesTen database running on a Unix or Linux platform, as well as a Windows machine with .NET 4.0 development environment.

## IMPORTANT PRE-REQUISITES

The following steps must be performed on the Unix/Linux machine hosting the TimesTen database. You will need to have a copy of the QuickStart files available on that machine. 

1. Manually Configure the Sample DSN for the Sample Programs; refer to _quickstart/classic/html/developer/sample\_dsn\_setup.html_

 
2. Set up sample database and user accounts

    The following build_sampledb script should be run once to set up the sample database and user accounts. First set up the Instance Environment Variables e.g. If your TimesTen instance location is under /home/timesten/instance/<instance_name> directory, execute the command

    `source /home/timesten/instance/<instance_name>/bin/ttenv.sh`

    Run the quickstart/classic/sample_scripts/createdb/build_sampledb script, which creates the sample database and user accounts that are used by the sample programs. This script creates the TimesTen user accounts and prompts you for the desired user passwords.

    Unix/Linux:
    
    `cd quickstart/classic/sample_scripts/createdb`
    
    `./build_sampledb.sh`
    
The following steps should be performed on the Windows client machine.

3. [Download](https://www.oracle.com/database/technologies/timesten-downloads.html) and install the TimesTen Windows client. Be sure to select the check box to 'Register Environment Variables'. You will need to log out of Windows and then log back in (or restart the Windows machine) before proceeding.

4. Configure an ODBC client DSN named **sampledbCS** for connection to the database hosted on your Unix/Linux sytem (consult the [TimesTen documentation](https://docs.oracle.com/en/database/other-databases/timesten/) for instructions, if required).

5. Download and install [Oracle Data Provider for .NET](https://www.oracle.com/database/technologies/appdev/dotnet/odp.html) (ODP.NET).


## How to compile the sample ODP.NET program

To compile the sample program on the Windows machine:

1. Open a Visual Studio command prompt.

2. Change directory to the directory containg the sample program (DemoODP.cs).

3. Compile the program:

    `csc /out:DemoODP.exe /reference:<path\to\Oracle.DataAccess.dll> DemoODP.cs`

**NOTE:** You need to reference the **Oracle.DataAccess.dll** assembly located in your ODP.NET installation.

## How to run the sample ODP.NET program

Run the program specifying an 'easy connect' specifier that references the client DSN (sampledbCS), the database username and password:

  `DemoODP -db localhost/sampledbCS:timesten_client -user appuser -passwd <password>`

For all available program options, use:
 
  `DemoODP -help`


## JsonSample

`JsonSample.cs` demonstrates TimesTen JSON operations through ODP.NET. The program reconnects to a TimesTen database, recreates a JSON-enabled purchase order table, loads JSON documents from disk, creates a functional JSON index on the `User` attribute, updates an existing document, queries by purchase order and user, and lists line items using `JSON_TABLE`. The optional `-keep` switch preserves the table after the run; otherwise it is dropped.

Before running the sample, ensure that your TimesTen client can resolve the target DSN by configuring Oracle Net entries. Add the DSN to `tnsnames.ora`, for example:

```
sampledbcs = (DESCRIPTION =
                (CONNECT_DATA =
                   (SERVICE_NAME = sampledbcs)
                   (SERVER = timesten_client)))
```

Also configure `sqlnet.ora` so that TNSNAMES and EZCONNECT resolution are enabled:

```
# To use ezconnect syntax or tnsnames, the following entries must be
# included in the sqlnet.ora configuration.
NAMES.DIRECTORY_PATH= (TNSNAMES, EZCONNECT)
```

After configuring Oracle Net, create the TimesTen client DSN using the **ODBC Data Sources (64-bit)** administrative tool on Windows so that the sample can locate `sampledbCS`.

Example usage:

  Default credentials (appuser/sampledb) and drop the table when finished
  
  `JsonSample`

  Override credentials, DSN, and retain the table (optional `-keep`)
  
  `JsonSample -user myuser -password mypassword -dsn mydsn`

For more information on how to use ODP.NET to develop programs for
the TimesTen database, see the [ODP.NET Support for TimesTen User's Guide](https://docs.oracle.com/en/database/other-databases/timesten/22.1/odpnet/index.html).
