/*
* Copyright (c) 2019, 2026 Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 *  DESCRIPTION
 *    Provides functions to get the usernames and passwords
 *    for the samples.
 */

function parseArgs(){
  args = process.argv.slice(2);
  let opts = {};
  for(var i = 0; i < args.length; i+=2){
    opts[args[i]] = args[i+1];
  }
  return opts;
}

function usage(scriptName, passwordEnvVar){
  const passwordUsage = passwordEnvVar ? '[-p <password>]' : '-p <password>';
  const passwordDescription = passwordEnvVar
    ? `-p  <password>: database password for the user, or set ${passwordEnvVar}`
    : '-p  <password>: database password for the user';

  return `
  Usage: node ${scriptName} -u <userName> ${passwordUsage} [-c <connectionString>]

  To run the sample, pass the following parameters to the sample program:

    Required:

      -u  <username>: database user name

      ${passwordDescription}

    Optional: 

      -c  <connectionString>:  Use the specified connection string (Default: "localhost/sampledb:timesten_direct")

            <connectionString> should be in Easy-Connect format:
    
              \{<net_service_name> | <host>/<host_service_name>:\{ timesten_direct | timesten_client \}\}
    
    `;
}

function getCredentials(scriptName, passwordEnvVar){
  let args = parseArgs();
  if (typeof args['-p'] === 'undefined' && passwordEnvVar) {
    args['-p'] = process.env[passwordEnvVar];
  }
  if(typeof args['-u'] === "undefined" ||
      typeof args['-p'] === "undefined"){
    console.error(usage(scriptName, passwordEnvVar));
    throw "Error: Bad options format";
  }
  if ('-c' in args){
    if(typeof args['-c'] === "undefined")
      throw "Error: -c option requires 1 argument";
  }
  else{
    args['-c'] = "localhost/sampledb:timesten_direct";
  }
  return args;
}

module.exports = {
  parseArgs:      parseArgs,
  usage:          usage,
  getCredentials: getCredentials
}
