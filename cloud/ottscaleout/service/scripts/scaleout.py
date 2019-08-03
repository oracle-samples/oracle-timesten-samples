#!/usr/bin/env python
# Copyright (c) 1999, 2019, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl

import gethosts
import copy
import json
import os
import string
import subprocess
import sys

hostlist = []
likemap = {}

# 
# Using dbstatus -dataspacegroup output capture
# ksafety, list of hosts
# In each dataspacegroup, get one tuple (dsg, hostAddr, hostName) for 'like' clause
# "hostcreate -address hostAddr -like hostName cascade -dataspacegroup dsg"
#
def parseDbStatus(fdbspg):
  try:
    with open(fdbspg,'r') as inpfile:
      obj = json.load(inpfile)
    ks = obj['k']
    dbs=obj['databases']
    for db in dbs:
      # assumes 1 db
      dsglist=[]
      instances=db['instances']
      for ins in instances:
        dsg=ins['dataSpaceGroup']
        hostname=ins['hostName']
        if hostname not in hostlist:
          hostlist.append(hostname)
        if dsg not in dsglist:
          dsglist.append(dsg)
          likemap[dsg] = []
          likemap[dsg].append(dsg)
          likemap[dsg].append(ins['hostAddr'])
          likemap[dsg].append(hostname)
          #print('hostaddress {} hostname {} data space group {}'.format(ins['hostAddr'],ins['hostName'],dsg))
    return ks
  except ValueError as details:
    print 'Error loading JSON from file'.format(
           details.returncode,
           str(details.output).replace('\n',''))
    sys.exit(1)
  except IOError as details:
    print 'IO Error: rc={0}: {1}'.format(
           details.returncode,
           str(details.output).replace('\n',''))
    sys.exit(1)
  except subprocess.CalledProcessError as details:
    print 'Error running: ' + cmd + '\nrc={0}: {1}'.format(
           details.returncode,
           str(details.output).replace('\n',''))
    sys.exit(1)


def printMap(amap):
  for k in amap:
    print amap[k]

def printScaleoutCommands(opath, ttenv, dbname, singleAD, ksafety):

  # get list of all hosts including ones to add
  dbinstances=gethosts.getInstances(fhosts, 'db-addresses')
  newdsg = 0
  if not likemap:
    print('oops: no likemap')
    #else:
    #print('{}'.format(likemap))
  for host in dbinstances:
    if host not in hostlist:
      # hostCreate
      newdsg = (newdsg % (int(max(likemap.keys())))) + 1
      (dsgroup, addr, name) = likemap[str(newdsg)]
      #print (dsgroup, addr, name)
      print('{0} ttgridadmin hostcreate {1} -internalAddress {1} -externalAddress {1} -like {2} -cascade -dataspacegroup {3}').format(ttenv, host, name, dsgroup)
  # apply
  print('{} ttgridadmin modelapply').format(ttenv)
  print('{} ttgridadmin instancelist | sort -rk 3').format(ttenv)
  # wait for elements to be loaded
  print('{0}/dbloadready.py {1} {2}'.format(opath, ttenv, dbname))
  # distribute rows
  for host in dbinstances:
    if host not in hostlist:
      print('{0} ttgridadmin dbdistribute {1} -add {2}').format(ttenv, dbname, host)
  print('{0} ttgridadmin dbdistribute {1} -apply').format(ttenv, dbname)
  print('{0} ttgridadmin dbstatus {1} --replicaset').format(ttenv, dbname)

if __name__ == '__main__':

  if len(sys.argv) <= 4:
    print('{}: usage mgmt-path dbname hostfile dataspacegroupfile').format(sys.argv[0])
    sys.exit()
  opath=os.path.dirname(sys.argv[0])
  mgpath=sys.argv[1]
  dbname=sys.argv[2]
  fhosts=sys.argv[3]
  fdbspg=sys.argv[4]
  singleAD=sys.argv[5]
  ksafeval = parseDbStatus(fdbspg)
  #printMap(hostmap)
  #printMap(likemap)
  printScaleoutCommands(opath, mgpath, dbname, singleAD, ksafeval)

