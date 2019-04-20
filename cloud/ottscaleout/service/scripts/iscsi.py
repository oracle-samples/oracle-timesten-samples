#!/usr/bin/env python
# Copyright (c) 1999, 2019, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl

import sys
import subprocess
import fileinput
import re
import socket
print(socket.gethostname())

def iscsiOperation(operation, dpath):
  try:
    hname=socket.gethostname()
    bvname = re.sub(r'-di-','-bvat-',hname)
    line = '{0}/runcmds.py {1}/iscsi/iscsi-{2}.{3}'.format(dpath, dpath, operation, bvname)
    y=subprocess.check_output(line, stderr=subprocess.STDOUT, shell=True)
    if re.search('errno', y):
      print('ERROR: {0}'.format(y))
      return -1
    return 0
  except subprocess.CalledProcessError as details:
    print ('Error getting instance id or executing iscsi operations:\n\t{0}\n\trc={2} details: {3}'.format(
        line, details.returncode, str(details.output).replace('\n','')))
    return -1

if __name__ == '__main__':

  if len(sys.argv) <= 2:
    print('{}: error: expected "{attach|detach <path>"').format(sys.argv[0])
    sys.exit()
  operation = sys.argv[1]
  cmdsfilepath = sys.argv[2]

  sys.exit(iscsiOperation(operation, cmdsfilepath))




