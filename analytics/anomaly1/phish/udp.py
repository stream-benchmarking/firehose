#!/usr/local/bin/python

# MINNOW udp
# read packets from UDP port

import sys,socket,re
import phish

# command-line defaults

nsenders = 1;
countflag = 0;

# packet stats

nrecv = 0
nshut = 0
count = nsenders*[0]
shut = nsenders*[0]

# parse command line
# regular argument = port to read from
# optional arguments:
#   -p = # of generators sending to me, only used to count STOP packets
#   -c = 1 to print stats on packets received from each sender

def read_cmd_options(argv):
  global nsenders,countflag,port
  iarg = 1
  while iarg < len(argv):
    if argv[iarg] == "-p":
      nsenders = int(argv[iarg+1])
      iarg += 2
    elif argv[iarg] == "-c":
      countflag = 1
      iarg += 1
    else: break
    
  if iarg != len(argv)-1:
    print "Syntax: udp.py options port"
    sys.exit()
    
  if nsenders <= 0:
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

# main program
    
args = phish.init(sys.argv)
phish.output(0)
phish.check()

read_cmd_options(args)

# setup UDP port
# packet buffer length of 1 Mbyte is ample

localhost = "127.0.0.1"
sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
sock.bind((localhost,port))
maxbuf = 1024*1024

pattern = re.compile("packet (\d+)")

# read packets

while 1:
  str = sock.recv(maxbuf)

  # check if STOP packet
  # exit if have received STOP packet from every sender

  if len(str) < 8:
    if shutdown(str): break
    continue

  # send packet downstream

  phish.pack_string(str);
  phish.send(0);

  nrecv += 1

  # tally stats on packets from each sender

  if countflag:
    match = re.search(pattern,str)
    ipacket = int(match.group(1))
    count[ipacket % nsenders] += 1

if countflag and nsenders > 1:
  for c in count: print c,
  print "packets received per sender"

phish.exit()
