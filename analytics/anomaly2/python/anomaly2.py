#!/usr/local/bin/python

# Stream analytic #2 = anomaly detection from unbounded key range
# Syntax: anomaly2.py options port

# parse command line
# regular argument = port to read from
# optional arguments:
#   -b = bundling factor = # of datums in one packet
#   -t = check key for anomalous when appears this many times
#   -m = key is anomalous if value=1 <= this many times
#   -p = # of generators sending to me, only used to count STOP packets
#   -c = 1 to print stats on packets received from each sender
#   -a = size of active key set

def read_cmd_options(argv):
  global perpacket,nthresh,mthresh,nsenders,countflag,maxactive,port
  iarg = 1
  iarg = 1
  while iarg < len(argv):
    if argv[iarg] == "-b":
      perpacket = int(argv[iarg+1])
      iarg += 2
    elif argv[iarg] == "-t":
      nthresh = int(argv[iarg+1])
      iarg += 2
    elif argv[iarg] == "-m":
      mthresh = int(argv[iarg+1])
      iarg += 2
    elif argv[iarg] == "-p":
      nsenders = int(argv[iarg+1])
      iarg += 2
    elif argv[iarg] == "-c":
      countflag = 1
      iarg += 1
    elif argv[iarg] == "-a":
      maxactive = int(argv[iarg+1])
      iarg += 2
    else: break
  
  if iarg != len(argv)-1:
    print "Syntax: anomaly1.py options port"
    sys.exit()
    
  flag = 0
  if perpacket <= 0: flag = 1
  if nthresh <= 0 or mthresh <= 0: flag = 1
  if nsenders <= 0 or maxactive <= 0: flag = 1
  if flag:
    print "ERROR: invalid command-line switch"
    sys.exit()

  # set UDP port
    
  port = int(argv[iarg])

# process STOP packets
# return 1 if have received STOP packet from every sender
# else return 0 if not ready to STOP

def shutdown(str):
  global nshut
  iwhich = int(str)
  if shut[iwhich]: return 0
  shut[iwhich] = 1
  nshut += 1
  if nshut == nsenders: return 1
  return 0

# stats on packets and keys received, and anomalies found

def stats():
  print
  print "packets received =",nrecv
  if countflag and nsenders > 1:
    for i,c in enumerate(count):
      print "packets received per sender %d = %d" % (i,c)
  print "datums received =",nrecv*perpacket

  print "unique keys =",len(kv)
  print "true anomalies =",ptrue
  print "false positives =",pfalse
  print "false negatives =",nfalse
  print "true negatives =",ntrue

# one active key

class Akey():
  def __init__(self):
    self.key = 0
    self.count = 0
    self.value = 0
    self.prev = None
    self.next = None

# main program
  
import sys,socket,re
from my_double_linked_list import MyDoubleLinkedList

# read command-line options

perpacket = 50
nthresh = 24
mthresh = 4
nsenders = 1
countflag = 0
maxactive = 3*131072

read_cmd_options(sys.argv)

# packet stats

nrecv = 0
nshut = 0
count = nsenders*[0]
shut = nsenders*[0]

# anomaly stats

ptrue = 0
pfalse = 0
nfalse = 0
ntrue = 0

# key-value hash table for received datums
# aset = list of active keys

aset = maxactive*[0]
for i in xrange(maxactive): aset[i] = Akey()
list = MyDoubleLinkedList()
nactive = 0
kv = {}
nunique = 0

# setup UDP port
# packet buffer length of 64 bytes per datum is ample

localhost = "127.0.0.1"
sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
sock.bind((localhost,port))
maxbuf = 64*perpacket

pattern = re.compile("packet (\d+)")

# read packets

while 1:
  str = sock.recv(maxbuf)

  # check if STOP packet
  # exit if have received STOP packet from every sender

  if len(str) < 8:
    if shutdown(str): break
    continue

  nrecv += 1

  # tally stats on packets from each sender

  if countflag:
    match = re.search(pattern,str)
    ipacket = int(match.group(1))
    count[ipacket % nsenders] += 1

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
            print "false negative =",key
          else: ntrue += 1
        else:
          if truth:
            ptrue += 1
            print "true anomaly =",key
          else:
            pfalse += 1
            print "false positive =",key
        akey.value = -1

# print stats

stats()      
