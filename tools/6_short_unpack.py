#!/usr/bin/env python

import struct
import sys

def main(argv):
    while True:
        try:
            data = sys.stdin.read(12)
            values = list(struct.unpack('<hhhhhh', data))
            print ' '.join(['%6hd' % value for value in values])
        except struct.error:
            # couldn't unpack six values, so it's a short read
            print 'Remainder (bytes, not shorts):'
            print '%s' % ' '.join(['%x' % ord(byte) for byte in data])
            break

if __name__ == '__main__':
    main(sys.argv)
