#!/usr/local/bin/python

# MINNOW anomaly2
# anomaly detection from unbounded key range
# print found anomalies to screen

import sys
from my_double_linked_list import MyDoubleLinkedList
import phish

# command-line defaults

nthresh = 24
mthresh = 4
maxactive = 3*131072

# packet stats

nrecv = 0
nunique = 0

# anomaly stats

ptrue = 0
pfalse = 0
nfalse = 0
ntrue = 0

# key stats

nactive = 0
nunique = 0

# one active key

class Akey():
  def __init__(self):
    self.key = 0
    self.count = 0
    self.value = 0
    self.prev = None
    self.next = None

# key-value hash table for received datums

kv = {}

# parse command line
# optional arguments:
#   -t = check key for anomalous when appears this many times
#   -m = key is anomalous if value=1 <= this many times
#   -a = size of active key set

def options(args):
  global nthresh,mthresh,maxactive
  iarg = 1
  while iarg < len(args):
    if args[iarg] == "-t":
      nthresh = int(args[iarg+1])
      iarg += 2
    elif args[iarg] == "-m":
      mthresh = int(args[iarg+1])
      iarg += 2
    elif args[iarg] == "-a":
      maxactive = int(args[iarg+1])
      iarg += 2
    else: break

  if iarg != len(args):
    print "Syntax: anomaly2.py options"
    sys.exit()

  if nthresh <= 0 or mthresh <= 0 or maxactive <= 0:
    print "ERROR: invalid command-line switch"
    sys.exit()

# stats on packets and keys received, and anomalies found

def stats():
  print >>fp
  print >>fp,"packets received = %d" % nrecv
  print >>fp,"unique keys = %d" % len(kv)
  print >>fp,"true anomalies = %d" % ptrue
  print >>fp,"false positives = %d" % pfalse
  print >>fp,"false negatives = %d" % nfalse
  print >>fp,"true negatives = %d" % ntrue

# process one packet

def packet(nvalues):
  global nrecv,nactive,nunique,ptrue,pfalse,nfalse,ntrue;

  nrecv += 1
  type,str,len = phish.unpack()
  
  # process all datums in packet

  lines = str.split('\n')
  
  for line in lines[1:-1]:
    words = line.split(',')

    # store datum in hash table
    # if new key, add to active set, deleting LRU key if necessary
    # count = # of times key has been seen
    # value = # of times value = 1
    # value < 0 means already seen Nthresh times

    key = int(words[0])
    value = int(words[1])

    if key not in kv:
      nunique += 1
      if nactive < maxactive:
        akey = aset[nactive]
        nactive += 1
        kv[key] = akey
      else:
        akey = list.last
        list.remove(akey)
        del kv[akey.key]
        kv[key] = akey
      akey.key = key
      akey.count = 1
      akey.value = value
      list.prepend(akey)

    else:
      akey = kv[key]
      list.move2front(akey)
      
      if akey.value < 0:
        akey.count += 1
        continue
      
      akey.count += 1
      akey.value += value
      
      if akey.count == nthresh:
        truth = int(words[2])
        if akey.value > mthresh:
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
        akey.value = -1

# main program

args = phish.init(sys.argv)
phish.input(0,packet,None,1)
phish.check()

fp = open("tmp.%d" % phish.query("idlocal",0,0),'w');
options(args)

aset = maxactive*[0]

# key-value hash table for received datums
# aset = list of active keys

aset = maxactive*[0]
for i in xrange(maxactive): aset[i] = Akey()
list = MyDoubleLinkedList()
nactive = 0

phish.loop()

stats()
fp.close()
phish.exit()
