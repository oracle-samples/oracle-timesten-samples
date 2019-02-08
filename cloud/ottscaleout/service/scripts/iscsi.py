#!/usr/bin/env python
# Copyright (c) 1999, 2019, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl

import sys
import subprocess
import fileinput

def iscsiOperation(operation, dpath):
  try:
    line = 'curl -sL http://169.254.169.254/opc/v1/instance/id'
    z=subprocess.check_output(line, stderr=subprocess.STDOUT, shell=True)
    #print(z)
    line = '{0}/runcmds.py {1}/iscsi/iscsi-{2}.{3}'.format(dpath, dpath, operation, z)
    y=subprocess.check_output(line, stderr=subprocess.STDOUT, shell=True)
  except subprocess.CalledProcessError as details:
    print ('Error getting instance id or executing iscsi ops: {0} id: {1}\nrc={2} details: {3}'.format(
        operation, z, details.returncode, str(details.output).replace('\n','')))

if __name__ == '__main__':

  if len(sys.argv) <= 2:
    print('{}: error: expected "{attach|detach <path>"').format(sys.argv[0])
    sys.exit()
  operation = sys.argv[1]
  cmdsfilepath = sys.argv[2]

  iscsiOperation(operation, cmdsfilepath)



