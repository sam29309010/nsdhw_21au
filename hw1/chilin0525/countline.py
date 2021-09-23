#!/usr/bin/env python3

import sys
import os
import os.path

py_env = (os.environ['PYTHON_BIN'])
if(py_env == "python2" or py_env == "python3"):
    sys.executable = py_env
else:
    print("exec: "+py_env+": not found")
    exit(-1)

if len(sys.argv) < 2:
    sys.stdout.write('missing file name\n')
elif len(sys.argv) > 2:
    sys.stdout.write('only one argument is allowed\n')
else:
    fname = sys.argv[1]
    if os.path.exists(fname):
        with open(fname) as fobj:
            lines = fobj.readlines()
        sys.stdout.write('{} lines in {}\n'.format(len(lines), fname))
    else:
        sys.stdout.write('{} not found\n'.format(fname))