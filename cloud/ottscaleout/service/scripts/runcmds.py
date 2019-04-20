#!/usr/bin/env python
# Copyright (c) 1999, 2019, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl

import sys
import subprocess
import fileinput

def readRunCmds():
  try:
    for line in fileinput.input():
      z=subprocess.check_output(line, stderr=subprocess.STDOUT, shell=True)
      print(z)
  except subprocess.CalledProcessError as details:
    print 'In file: {0}, line: {1}, errno: {2}, {3}'.format(
           fileinput.filename(),
           fileinput.lineno(), 
           details.returncode,
           str(details.output).replace('\n',''))
  finally:
    fileinput.close()

if __name__ == '__main__':

  if len(sys.argv) <= 1:
    print('{}: error: expected at least one file with commands to execute').format(sys.argv[0])
    sys.exit()

  readRunCmds()






