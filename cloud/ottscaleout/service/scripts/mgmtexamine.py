#!/usr/bin/env python
# Copyright (c) 1999, 2019, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl

import sys
import subprocess
import fileinput
import string


teststr='ttgridadmin mgmtexamine'

def mgmtExamine(ttenv_path):
  try:
    #cmd=ttenv_path + ' ttgridadmin mgmtexamine | sed -r -e \'/Recommended commands:/{n;$p}\''
    cmd=ttenv_path + " ttgridadmin mgmtexamine | awk 'x==1 {print} /Recommended commands:/ {x=1}'"
    z=subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True)
    print ('mgmtexamine:\n{}\n{}'.format(cmd, z))
    for line in z.splitlines():
      # work around a bug where instance name isn't separated from bin/ttenv
      y = line.replace('bin/ttenv','/bin/ttenv')
      if line.lower().find(teststr) != -1:
        mgmtExamine(ttenv_path)
        continue
      print(y)
      x=subprocess.check_output(y, stderr=subprocess.STDOUT, shell=True)
  except subprocess.CalledProcessError as details:
    print 'Error running recommendation from mgmtexamine: rc={0}: {1}'.format(
           details.returncode,
           str(details.output).replace('\n',''))


if __name__ == '__main__':

  if len(sys.argv) <= 1:
    print('{}: error: expected path of management instance').format(sys.argv[0])
    sys.exit()
  ttenvpath=sys.argv[1]

  mgmtExamine(ttenvpath)

