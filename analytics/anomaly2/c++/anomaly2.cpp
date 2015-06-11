// Stream analytic #2 = anomaly detection from unbounded key range
// Syntax: anomaly2 options port

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>

#include <stdexcept>
#include <signal.h>
#include <netinet/in.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <vector>
#include <tr1/unordered_map>

#include "my_double_linked_list.h"

// hard-coded settings from generator #2

#define NACTIVE 131072

// UDP port

int port;

// defaults for command-line switches

int perpacket = 50;
int nthresh = 24;
int mthresh = 4;
int nsenders = 1;
int countflag = 0;
int maxactive = 2*NACTIVE;

// packet stats

uint64_t nrecv = 0;
int nshut = 0;
int *shut;
uint64_t *count;
uint64_t nunique = 0;

// anomaly stats

int ptrue = 0;
int pfalse = 0;
int nfalse = 0;
int ntrue = 0;

// one active key

struct Akey {
  uint64_t key;
  int count,value;
  Akey *prev;
  Akey *next;
};

// key-value hash table for received datums

std::tr1::unordered_map<uint64_t,Akey*> kv(1);

// parse command line
// regular argument = port to read from
// optional arguments:
//   -b = bundling factor = # of datums in one packet
//   -t = check key for anomalous when appears this many times
//   -m = key is anomalous if value=1 <= this many times
//   -p = # of generators sending to me, used to pre-allocate hash table
//   -c = 1 to print stats on packets received from each sender
//   -a = size of active key set

void read_cmd_options(int argc, char **argv)
{
  int op;

  while ( (op = getopt(argc, argv, "b:t:m:p:c:a:")) != EOF) {
    switch (op) {
    case 'b':
      perpacket = atoi(optarg);
      break;
    case 't':
      nthresh = atoi(optarg);
      break;
    case 'm':
      mthresh = atoi(optarg);
      break;
    case 'p':
      nsenders = atoi(optarg);
      break;
    case 'c':
      countflag = 1;
      break;
    case 'a':
      maxactive = atoi(optarg);
      maxactive *= 2;
      break;
    }
  }

  if (optind != argc-1) {
    fprintf(stderr,"Syntax: anomaly2 options port\n");
    exit(1);
  }
  
  int flag = 0;
  if (perpacket <= 0) flag = 1;
  if (nthresh <= 0 || mthresh <= 0) flag = 1;
  if (nsenders <= 0 || maxactive <= 0) flag = 1;
  if (flag) {
    fprintf(stderr, "ERROR: invalid command-line switch\n");
    exit(1);
  }

  // set UDP port

  port = atoi(argv[optind]);
}

// process STOP packets
// return 1 if have received STOP packet from every sender
// else return 0 if not ready to STOP

int shutdown(char *buf)
{
  int iwhich = atoi(buf);
  if (shut[iwhich]) return 0;
  shut[iwhich] = 1;
  nshut++;
  if (nshut == nsenders) return 1;
  return 0;
}

// stats on packets and keys received, and anomalies found

void stats()
{
  printf("\n");
  printf("packets received = %" PRIu64 "\n",nrecv);
  if (countflag && nsenders > 1)
    for (int i = 0; i < nsenders; i++)
      printf("packets received from sender %d = %" PRIu64 "\n",i,count[i]);

  uint64_t ndatum = nrecv * perpacket;
  fprintf(stdout,"datums received = %" PRIu64 "\n",ndatum);

  printf("unique keys = %" PRIu64 "\n",nunique);
  printf("true anomalies = %d\n",ptrue);
  printf("false positives = %d\n",pfalse);
  printf("false negatives = %d\n",nfalse);
  printf("true negatives = %d\n",ntrue);
}

// main program

int main(int argc, char **argv)
{
  read_cmd_options(argc,argv);

  // sender stats and stop flags

  count = new uint64_t[nsenders];
  shut = new int[nsenders];
  for (int i = 0; i < nsenders; i++) count[i] = shut[i] = 0;

  // setup UDP port

  const int socket = ::socket(PF_INET6, SOCK_DGRAM, 0);
  if (socket == -1)
    throw std::runtime_error(std::string("socket(): ") + ::strerror(errno));

  struct sockaddr_in6 address;
  ::memset(&address, 0, sizeof(address));
  address.sin6_family = AF_INET6;
  address.sin6_addr = in6addr_any;
  address.sin6_port = htons(port);
  if (-1 == ::bind(socket, reinterpret_cast<sockaddr*>(&address),
                   sizeof(address)))
    throw std::runtime_error(std::string("bind(): ") + ::strerror(errno));

  // scale maxactive by number of generators
  // aset = active key set
  // stored as doubly-linked list so can delete least recently used
  // nactive = # of keys in active set

  maxactive *= nsenders;
  Akey *aset = new Akey[maxactive];
  MyDoubleLinkedList<Akey*> list;
  int nactive = 0;

  // pre-allocate to maxactive per generator, in agreement with generator #2
  
  //kv.rehash(maxactive)
  kv.rehash(ceil(maxactive / kv.max_load_factor()));

  // packet buffer length of 64 bytes per datum is ample

  int maxbuf = 64*perpacket;
  std::vector<char> buffer(maxbuf);
  int ipacket;

  // loop on reading packets

  Akey *akey;

  while (true) {

    // read a packet with Nbytes

    const int nbytes = ::recv(socket,&buffer[0],buffer.size()-1,0);
    buffer[nbytes] = '\0';

    // check if STOP packet
    // exit if have received STOP packet from every sender
    
    if (nbytes < 8) {
      if (shutdown(&buffer[0])) break;
      continue;
    }

    nrecv++;

    // tally stats on packets from each sender

    if (countflag) {
      sscanf(&buffer[0],"packet %d",&ipacket);
      count[ipacket % nsenders]++;
    }

    // scan past header line

    strtok(&buffer[0],"\n");

    // process perpacket datums in packet

    for (int i = 0; i < perpacket; i++) {
      uint64_t key = strtoul(strtok(NULL,",\n"),NULL,0);
      uint32_t value = strtoul(strtok(NULL,",\n"),NULL,0);
      uint32_t truth = strtoul(strtok(NULL,",\n"),NULL,0);

      // store datum in hash table
      // if new key, add to active set, deleting LRU key if necessary
      // count = # of times key has been seen
      // value = # of times value = 1
      // value < 0 means already seen Nthresh times

      if (!kv.count(key)) {
        nunique++;
        if (nactive < maxactive) {
          akey = &aset[nactive++];
          kv[key] = akey;
        } else {
          akey = list.last;
          list.remove(akey);
          kv.erase(akey->key);
          kv[key] = akey;
        }
        akey->key = key;
        akey->count = 1;
        akey->value = value;
        list.prepend(akey);

      } else {
        akey = kv[key];
        list.move2front(akey);

        if (akey->value < 0) {
          akey->count++;
          continue;
        }
        
        akey->count++;
        akey->value += value;

        if (akey->count == nthresh) {
          if (akey->value > mthresh) {
            if (truth) {
              nfalse++;
              printf("false negative = %" PRIu64 "\n",key);
            } else ntrue++;
          } else {
            if (truth) {
              ptrue++;
              printf("true anomaly = %" PRIu64 "\n",key);
            } else {
              pfalse++;
              printf("false positive = %" PRIu64 "\n",key);
            }
          }
          akey->value = -1;
        }
      }
    }
  }
  
  // close UDP port and print stats

  ::close(socket);
  stats();
}
