#!/usr/bin/env python
# Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
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

hostmap  = {}
addmap   = {}
sshopts = '-o StrictHostKeyChecking=no -o ConnectTimeout=3 '

def parseDbStatus(fdbspg):
  try:
    with open(fdbspg,'r') as inpfile:
      obj = json.load(inpfile)
    ks = obj['k']
    dbs=obj['databases']
    for db in dbs:
      instances=db['instances']
      for ins in instances:
        elemlist=ins['elements']
        for elem in elemlist:
          dsgmap  = {}
          dsgmap['ds'] = ins['dataSpaceGroup']
          dsgmap['rs'] = elem['syncReplicaSet']
          dsgmap['hn'] = ins['hostName']
          dsgmap['in'] = ins['instanceName']
          cmd='ssh ' + sshopts + dsgmap['hn'] + ' hostname --domain'
          dmnm=subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True)
          dsgmap['dn']=dmnm.strip()
          hostmap[dsgmap['hn']] = copy.deepcopy(dsgmap)
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


def parseHosts(fhosts):
  try:
    dbinstances=gethosts.getInstances(fhosts, 'db-addresses')
    for host in dbinstances:
      # look up host
      if host not in hostmap:
        #print ('*** host={} ***').format(host)
        # get domainname
        cmd='ssh ' + host + ' hostname --domain'
        dnr=subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True)
        dn=dnr.strip()
        #print ('dn outer: ' + dn)
        # find matching host/domain/namespace entry
        likeHost=findMatchingDomain(dn)
        if likeHost is not None:
          # new entry
          newdsge={}
          newdsge['hn']=host
          newdsge['dn']=dn
          newdsge['ds']=likeHost['ds']
          newdsge['lk']=likeHost['hn']
          addmap[host] = copy.deepcopy(newdsge)
  except IOError as details:
    print 'IO Error: rc={0}: {1}'.format(
           details.returncode,
           str(details.output).replace('\n',''))
  except subprocess.CalledProcessError as details:
    print 'Error running ssh: rc={0}: {1}'.format(
           details.returncode,
           str(details.output).replace('\n',''))

def findMatchingDomain(dn):
    for k in hostmap:
      dsgdict = hostmap[k]
      if dsgdict['dn'] == dn:
        return dsgdict
    return None

def printMap(amap):
  for k in amap:
    print amap[k]

def printScaleoutCommands(opath, ttenv, dbname, singleAD, ksafety):
  # hostCreate
  newdsg = 0
  for hst in addmap:
    newdsg = (newdsg + 1) % (ksafety + 1)
    hstdict = addmap[hst]
    if singleAD == "true":
      dsgroup = newdsg
    else:
      dsgroup = hstdict['ds']
    #print hst, hstdict
    print('{0} ttgridadmin hostcreate {1} -internalAddress {1} -externalAddress {1} -like {2} -cascade -dataspacegroup {3}').format(ttenv, hst, hstdict['lk'], dsgroup)
  # apply
  print('{} ttgridadmin modelapply').format(ttenv)
  print('{} ttgridadmin instancelist | sort -rk 3').format(ttenv)
  # wait for elements to be loaded
  print('{0}/dbloadready.py {1} {2}'.format(opath, ttenv, dbname))
  # distribute rows
  for hst in addmap:
    hstdict = addmap[hst]
    print('{0} ttgridadmin dbdistribute {1} -add {2}').format(ttenv, dbname, hst)
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
  parseHosts(fhosts)
  #printMap(addmap)
  printScaleoutCommands(opath, mgpath, dbname, singleAD, ksafeval)

