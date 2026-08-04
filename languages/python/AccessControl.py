#
# Copyright (c) 2019, 2026 Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
#
#   DESCRIPTION
#     Provides functions to get the usernames and passwords
#     for the samples.
#
def parseArgs():
  from optparse import OptionParser
  parser = OptionParser()
  parser.add_option("-u", "--user", dest="user", help="username for login")
  parser.add_option("-p", "--password", dest="password", help="password for login")
  parser.add_option("-c", "--connstr", dest="connstr", help="connection string", 
      default="localhost/sampledb:timesten_direct")
  options, args = parser.parse_args()
  return options;

def usage(scriptName, password_env_var=None):
  password_usage = "-p <password>"
  password_description = "-p  <password>: database password for the user"
  if password_env_var:
    password_usage = "[-p <password>]"
    password_description = (
        f"-p  <password>: database password for the user, or set "
        f"{password_env_var}")

  return  """
  Usage: python {script} -u <userName> {password_usage} [-c <connectionString>]

  To run the sample, pass the following parameters to the sample program:

    Required:

      -u  <username>: database user name

      {password_description}

    Optional: 

      -c  <connectionString>:  Use the specified connection string (Default: "localhost/sampledb:timesten_direct")

            <connectionString> should be in Easy-Connect format:
    
              {{<net_service_name> | <host>/<host_service_name>:{{ timesten_direct | timesten_client }}}}

          """.format(
              script=scriptName,
              password_usage=password_usage,
              password_description=password_description)

def getCredentials(scriptName, password_env_var=None):
  """Return command-line credentials, with an optional password environment variable."""
  import os

  args = parseArgs()
  if args.password is None and password_env_var:
    args.password = os.getenv(password_env_var)
  if args.user == None or args.password == None:
    print(usage(scriptName, password_env_var))
    raise Exception("Error: Bad options format")
  return args
