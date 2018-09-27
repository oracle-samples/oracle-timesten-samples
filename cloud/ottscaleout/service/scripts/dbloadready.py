#!/usr/bin/env python
# Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl

# used for scaleout operation
# wait for *elements* to be loaded before dbdistribute
# parse output of ttgridadmin dbstatus <dbname> -loadreadiness

import json
import subprocess
import sys
import time

# wait 3 seconds between retries
sleepTime=3
# try for 30 minutes
waitTime=900


def parseDbStatus(jsondata):
  # return ready;listo when elements loaded
  # return element name if waiting
  try:
    obj = json.loads(jsondata)
    unassigned=obj["Unassigned Elements"]
    # this is a string
    if unassigned == 'None':
       return 'none'
    for elemlist in unassigned:
      elements=elemlist["Elements"]
      for elem in elements:
        elemState=elem["State"]
        elemName=elem["Name"]
        #print('Element {} is in state: {}'.format(elemName,elemState))
        if elemState != 'Loaded':
          return elemName
        else:
          continue
    return 'ready;listo'

  except ValueError:
    print "error loading JSON"
    sys.exit(1)

def getLoadReadiness(ttenvpath, dbname):
  try:
    cmd='{} ttgridadmin -o json dbstatus {} -loadreadiness'.format(ttenvpath, dbname)
    z=subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True)
    return z
  except subprocess.CalledProcessError as details:
    print '{0} failed: errno: {1}, {2}'.format(
           cmd,
           details.returncode,
           str(details.output).replace('\n',''))


if __name__ == '__main__':

  if len(sys.argv) <= 2:
    print('error: usage: {} ttenvpath dbname'.format(sys.argv[0]))
    sys.exit()

  readiness=False
  while waitTime > 0 and readiness == False:
    jsondata=getLoadReadiness(sys.argv[1], sys.argv[2])
    ready=parseDbStatus(jsondata)
    if ready == 'ready;listo':
      readiness=True
      break
    elif ready == 'none':
      print('{}: nothing to do'.format(sys.argv[0]))
      readiness=True
      break
    else:
      waitTime = waitTime - sleepTime
      print('Waiting for element {} to be loaded ... time remaining {} seconds'.format(ready,waitTime))
      time.sleep(sleepTime)
  if readiness == False:
    print('Timed out waiting for elements to be Loaded')
    sys.exit(1)
  sys.exit(0)
