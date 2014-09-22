#!/usr/bin/python
import sys
import re

DEBUG_CHECK_STRING = "DEBUG\s*=\s*(\d)"

def check_debug_off(fp):
    for line in fp:
        m = re.match(DEBUG_CHECK_STRING, line, re.IGNORECASE)
        if m:
            try:
                n = int(m.group(1))
                if n == 0:
                    return True
                else:
                    return False
            except ValueError:
                return False
    return False


#makes sure DEBUG is turned off
if(len(sys.argv) > 1):
    with open(sys.argv[1], "rU") as fp:
        if not check_debug_off(fp):
            print "DEBUG Flag Must be 0 !"
            sys.exit(1)
        print "Makefile pass"
        sys.exit(0)
else:
    print "Need to pass a Makefile"
    sys.exit(1)

