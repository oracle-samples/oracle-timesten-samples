#!/usr/bin/env python
# Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl

import sys
import fileinput
import string

#myvars = {}

def parsePairs(inputf):
  try:
    with open(inputf,'r') as inpfile:
      seenStart = False
      for line in inpfile:
        if seenStart == True:
          llist = line.split('=',2)
          if len(llist) > 1:
            name = str(llist[0]).strip().strip('"')
            var  = str(llist[1]).strip().strip('"')
            if name.startswith(('type','default','#')):
               continue
            print('{0:20}: \"{1}\"'.format(name, var))
        if line == "# Variables after this point written to ansible vars file\n":
          seenStart = True
  except IOError as details:
    print 'IO Error: rc={0}: {1}'.format(
           details.returncode,
           str(details.output).replace('\n',''))
  finally:
    inpfile.close()
     

if __name__ == '__main__':

  if len(sys.argv) <= 1:
    print('{}: usage inputfile').format(sys.argv[0])
    sys.exit()
  inppath=sys.argv[1]
  parsePairs(inppath)

