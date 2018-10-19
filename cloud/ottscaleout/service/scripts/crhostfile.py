#!/usr/bin/env python
# Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl

import ConfigParser
import collections
import os
import sys
import re

### This is a workaround to the lack of collections.OrderedDict in 
### Python 2.6.  It should work for 2.6+

def rawConfigParserAllowNoValue(config):
    '''This is a hack to support python 2.6. ConfigParser provides the 
    option allow_no_value=True to do this, but python 2.6 doesn't have it.
    '''
    OPTCRE_NV = re.compile(
        r'(?P<option>[^:=\s][^:=]*)'    # match "option" that doesn't start with white space
        r'\s*'                          # match optional white space
        r'(?P<vi>(?:[:=]|\s*(?=$)))\s*' # match separator ("vi") (or white space if followed by end of string)
        r'(?P<value>.*)$'               # match possibly empty "value" and end of string
    )
    config.OPTCRE = OPTCRE_NV
    config._optcre = OPTCRE_NV
    return config

def fetchSection(config, s):
  try: 
    profile = s
    mylist = []
    mylist = config.options(profile)
  except ConfigParser.NoSectionError, err:
    print (str(err) + '; section added')
    mylist = []
    config.add_section(s)
  return mylist

def getFile(outputFile, ksafety):
  config = ConfigParser.RawConfigParser()
  config = rawConfigParserAllowNoValue(config)
  #config = ConfigParser.ConfigParser(None,collections.OrderedDict,True)
  config.read(str(outputFile))

  # get and validate section input
  bastions = fetchSection(config, 'bastion-hosts')
  if len(bastions) < 1 or len(bastions) > 3 :
    print ('Error: expected 1-3 bastion hosts, found ' + str(len(bastions)))
    sys.exit(1)
  #print bastions, len(bastions)
  dbinstances = fetchSection(config, 'db-addresses')
  if len(dbinstances)%int(ksafety) != 0 :
    print ('Error: mismatch of ksafety: ' + str(ksafety) + ' # of database instances: ' + str(len(dbinstances)))
    sys.exit(1)
  #print dbinstances, len(dbinstances)
  zkinstances = fetchSection(config, 'zookeeper-addresses')
  #
  if len(zkinstances) < 0 or len(zkinstances) > 3:
    # user can only input 0 or 3; 1 or 2 occur if nxk == 1x1 or 1x2
    print ('Error: expected 0 or 3 zookeeper instances, found ' + str(len(zkinstances)))
    sys.exit(1)
  #print zkinstances, len(zkinstances)
  mginstances = fetchSection(config, 'mgmt-addresses')
  if len(mginstances) != 0 and len(mginstances) != 2 :
    print ('Error: expected 0 or 2 management instances, found ' + str(len(mginstances)))
    sys.exit(1)
  #print mginstances, len(mginstances)

  needzkclients = 0
  profile = 'zookeeper-addresses'
  if len(zkinstances) == 0 :
    if len(mginstances) == 2:
      config.set(profile, mginstances[0], None)
      config.set(profile, mginstances[1], None)
      config.set(profile, dbinstances[0], None)
    elif len(dbinstances) >= 3 :
      config.set(profile, dbinstances[0], None)
      config.set(profile, dbinstances[1], None)
      config.set(profile, dbinstances[2], None)
  elif len(zkinstances) == 1 :
    config.set(profile, dbinstances[0], None)
    needzkclients = 1
  elif len(zkinstances) == 2 :
    config.set(profile, dbinstances[0], None)
    needzkclients = 2

 
  profile = 'mgmt-addresses'
  if len(mginstances) == 0:
    config.set(profile, dbinstances[0], None)
    if  len(dbinstances) > 1:
      config.set(profile, dbinstances[1], None)
    else:
      config.set(profile, zkinstances[0], None)
      needzkclients = 1

  # install clients on mgmt instances, zkservers if present
  profile = 'client-addresses'
  clinstances = fetchSection(config, 'client-addresses')
  if len(mginstances) == 2:
    config.set(profile, mginstances[0], None)
    config.set(profile, mginstances[1], None)
  if len(zkinstances) == 3:
    config.set(profile, zkinstances[0], None)
    config.set(profile, zkinstances[1], None)
    config.set(profile, zkinstances[2], None)
  for c in range(needzkclients):
    config.set(profile, zkinstances[c], None)

  profile = 'srvrhosts:children'
  config.add_section(profile)
  config.set(profile, 'db-addresses', None)
  config.set(profile, 'mgmt-addresses', None)
  config.set(profile, 'zookeeper-addresses', None)

  with open(outputFile, 'w+') as hostsFile:
    config.write(hostsFile)
  
  ### Part of the workaround for lack of collections.OrderedDict
  ### in Python 2.6
  lines = open(outputFile).readlines()
  for i, line in enumerate(lines[:]):
    lines[i]=line.split("=",1)[0]
    lines[i]=lines[i].strip("\n")+"\n";
  
  open(outputFile,'w').writelines(lines)

  return 0

if __name__ == "__main__":
  if len(sys.argv) != 3 : 
    print ('Error: expected path ksafety')
    sys.exit(1)
  getFile(sys.argv[1], sys.argv[2])
