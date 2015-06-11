#!/usr/local/bin/python

# MINNOW anomaly1
# anomaly detection from fixed key range
# print found anomalies to screen

import sys
import phish

# command-line defaults

nthresh = 24
mthresh = 4

# packet stats

nrecv = 0

# anomaly stats

ptrue = 0
pfalse = 0
nfalse = 0
ntrue = 0

# key-value hash table for received datums

kv = {}

# parse command line
# optional arguments:
#   -t = check key for anomalous when appears this many times
#   -m = key is anomalous if value=1 <= this many times

def options(args):
  global nthresh,mthresh
  iarg = 1
  while iarg < len(args):
    if args[iarg] == "-t":
      nthresh = int(args[iarg+1])
      iarg += 2
    elif args[iarg] == "-m":
      mthresh = int(args[iarg+1])
      iarg += 2
    else: break

  if iarg != len(args):
    print "Syntax: anomaly1.py options"
    sys.exit()

  if nthresh <= 0 or mthresh <= 0:
    print "ERROR: invalid command-line switch"
    sys.exit()

# stats on packets and keys received, and anomalies found

def stats():
  print >>fp
  print >>fp,"packets received = %d" % nrecv
  print >>fp,"unique keys = %d" % len(kv)
  print >>fp,"max occurrence of any key = %d" % max([v[0] for v in kv.values()])
  print >>fp,"true anomalies = %d" % ptrue
  print >>fp,"false positives = %d" % pfalse
  print >>fp,"false negatives = %d" % nfalse
  print >>fp,"true negatives = %d" % ntrue

# process one packet

def packet(nvalues):
  global nrecv,ptrue,pfalse,nfalse,ntrue

  nrecv += 1
  type,str,len = phish.unpack()

  # process all datums in packet

  lines = str.split('\n')
  
  for line in lines[1:-1]:
    words = line.split(',')

    # store datum in hash table
    # value = (N,M)
    # N = # of times key has been seen
    # M = # of times flag = 1
    # M < 0 means already seen N times

    key = int(words[0])
    value = int(words[1])

    if key not in kv: kv[key] = (1,value)
    else:
      n,m = kv[key]
      if m < 0:
        kv[key] = (n+1,m)
        continue
      n += 1
      m += value
      if n >= nthresh:
        truth = int(words[2])
        if m > mthresh:
          if truth:
            nfalse += 1
            print >>fp,"false negative =",key
          else: ntrue += 1
        else:
          if truth:
            ptrue += 1
            print >>fp,"true anomaly =",key
          else:
            pfalse += 1
            print >>fp,"false positive =",key
        m = -1
      kv[key] = (n,m)

# main program

args = phish.init(sys.argv)
phish.input(0,packet,None,1)
phish.check()

fp = open("tmp.%d" % phish.query("idlocal",0,0),'w');
options(args)

phish.loop()

stats()
fp.close()
phish.exit()
