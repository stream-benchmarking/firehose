#!/usr/local/bin/python

# MINNOW keyhash
# disperse datums via key hashing to downstream minnows

import sys
import phish

# command-line defaults

peroutput = 50

# parse command line
# optional arguments:
#   -b 50 = output bundling factor = # of datums in one output packet

def options(args):
  global peroutput
  iarg = 1
  while iarg < len(args):
    if args[iarg] == "-b":
      peroutput = int(args[iarg+1])
      iarg += 2
    else: break

  if iarg != len(args):
    print "Syntax: keyhash.py options"
    sys.exit()

# process one packet

def packet(nvalues):
  global ptrue,pfalse,nfalse,ntrue

  type,str,len = phish.unpack()
  
  # process all datums in packet

  lines = str.split('\n')

  for line in lines[1:-1]:

    # copy datum to an output buffer, based on hash of key

    key,tmp = line.split(',',1)
    m = int(key) % nout
    obuf[m] += line + '\n'
    dcount[m] += 1

    # send buffer downstream when output buffer is full
    # reset buffer with new packet ID

    if dcount[m] == peroutput:
      phish.pack_string(obuf[m])
      phish.send_direct(0,m)
      pcount[m] += 1
      dcount[m] = 0
      obuf[m] = "packet %d\n" % pcount[m]
  
# flush output buffers when no more packets will be received

def flush():
  for m in xrange(nout):
    if dcount[m]:
      phish.pack_string(obuf[m])
      phish.send_direct(0,m)
      pcount[m] += 1

# main program

args = phish.init(sys.argv)
phish.input(0,packet,flush,1)
phish.output(0)
phish.check()

options(args)

nout = phish.query("outport/direct",0,0);
pcount = nout*[0]
dcount = nout*[0]
obuf = nout*['packet 0\n']

phish.loop()
phish.exit()
