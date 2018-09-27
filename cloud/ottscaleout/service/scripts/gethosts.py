#!/usr/bin/env python
# Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl

import collections
import ConfigParser
#import os
import sys
#import re

def fetchSection(config, s):
  try: 
    profile = s
    mylist = []
    mylist = config.options(profile)
  except ConfigParser.NoSectionError, err:
    print str(err) + '; section mising'
    sys.exit(1)
  return mylist

def getInstances(hostFile, section):
  config = ConfigParser.ConfigParser(None,collections.OrderedDict,True)
  config.read(str(hostFile))

  # get and validate section input
  instances = fetchSection(config, section)
  return instances


if __name__ == '__main__':

  if len(sys.argv) <= 2:
    print('error: usage {} hostfile serverlist').format(sys.argv[0])
    sys.exit()
  serverList=[]
  fhosts=sys.argv[1]
  for arg in sys.argv[2:]:
    serverList = serverList + getInstances(fhosts,arg) 
  print serverList

  


