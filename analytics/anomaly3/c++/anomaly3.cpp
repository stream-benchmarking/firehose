// Stream analytic #3 = anomaly detection from unbounded key range
//                      with two-level key lookup and assembly
// Syntax: anomaly3 options port

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

// hard-coded settings from generator #3

#define NACTIVE 131072

// UDP port

int port;

// defaults for command-line switches

int perpacket = 40;
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
uint64_t munique = 0;

// anomaly stats

int ptrue = 0;
int pfalse = 0;
int nfalse = 0;
int ntrue = 0;

// one active outer and inner key

struct Kouter {
  uint64_t key,inner;
  int count;
  Kouter *prev;
  Kouter *next;
};

struct Kinner {
  uint64_t key;
  int count,value;
  Kinner *prev;
  Kinner *next;
};

// key-value hash tables for received datums

std::tr1::unordered_map<uint64_t,Kouter*> okv(1);
std::tr1::unordered_map<uint64_t,Kinner*> ikv(1);

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

  while ( (op = getopt(argc, argv, "n:b:t:m:p:c:a:")) != EOF) {
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
    fprintf(stderr,"Syntax: anomaly3 options port\n");
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

  printf("unique outer keys = %" PRIu64 "\n",nunique);
  printf("unique inner keys = %" PRIu64 "\n",munique);
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
  // oset/iset = active key sets for outer/inner keys
  // olist/ilist = doubly-linked lists so can delete least recently used
  // mactive = # of keys in active set for inner keys
  // okv/ikv = outer/inner key-value hash tables
  // ofree = index of 1st free element in linked list of free oset elements

  maxactive *= nsenders;
  Kouter *oset = new Kouter[maxactive];
  Kinner *iset = new Kinner[maxactive];
  MyDoubleLinkedList<Kouter*> olist;
  MyDoubleLinkedList<Kinner*> ilist;
  int mactive = 0;

  Kouter *ofree = oset;
  for (int i = 0; i < maxactive; i++)
    oset[i].next = &oset[i+1];
  oset[maxactive-1].next = NULL;

  // pre-allocate to maxactive per generator, in agreement with generator #3
  
  okv.rehash(ceil(maxactive / okv.max_load_factor()));
  ikv.rehash(ceil(maxactive / ikv.max_load_factor()));

  // packet buffer length of 64 bytes per datum is ample

  int maxbuf = 64*perpacket;
  std::vector<char> buffer(maxbuf);
  int ipacket;

  // loop on reading packets

  Kouter *okey;
  Kinner *ikey;

  int max = 0;

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
      char *value = strtok(NULL,",\n");
      int count = atoi(strtok(NULL,",\n"));

      // store outer key in okv hash table
      // if new key, add to active set, deleting LRU key if necessary
      // count = # of times key has been seen
      // discard key if its count is not consistent with okv count
      // build up inner key 16-bits at a time

      if (!okv.count(key)) {
        if (count > 1) continue;
        nunique++;
        if (ofree) {
          okey = ofree;
          ofree = ofree->next;
          okv[key] = okey;
        } else {
          okey = olist.last;
          olist.remove(okey);
          okv.erase(okey->key);
          okv[key] = okey;
        }
        okey->key = key;
        okey->inner = atoi(value);
        okey->count = 1;
        olist.prepend(okey);

      } else {
        okey = okv[key];
        if (okey->count != count-1) {
          okv.erase(okey->key);
          olist.remove(okey);
          okey->next = ofree;
          ofree = okey;
          continue;
        }
        if (count <= 4) {
          olist.move2front(okey);
          uint64_t ivalue = atoi(value);
          ivalue = ivalue << (16*okey->count);
          okey->inner |= ivalue;
          okey->count++;
          continue;
        }

        // 5th occurrence of outer key, discard it
        // value of inner key = low-order digit of value
        // truth of inner key = hi-order digit of value

        okv.erase(okey->key);
        olist.remove(okey);
        okey->next = ofree;
        ofree = okey;

        key = okey->inner;
        uint32_t innervalue,truth;
        if (value[0] == '0') truth = 0;
        else truth = 1;
        if (value[1] == '0') innervalue = 0;
        else innervalue = 1;

        if (!ikv.count(key)) {
          munique++;
          if (mactive < maxactive) {
            ikey = &iset[mactive++];
            ikv[key] = ikey;
          } else {
            ikey = ilist.last;
            ilist.remove(ikey);
            ikv.erase(ikey->key);
            ikv[key] = ikey;
          }
          ikey->key = key;
          ikey->count = 1;
          ikey->value = innervalue;
          ilist.prepend(ikey);
          
        } else {
          ikey = ikv[key];
          ilist.move2front(ikey);

          if (ikey->value < 0) {
            ikey->count++;
            continue;
          }
        
          ikey->count++;
          ikey->value += innervalue;

          if (ikey->count > max) max = ikey->count;

          if (ikey->count == nthresh) {
            if (ikey->value > mthresh) {
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
            ikey->value = -1;
          }
        }
      }
    }
  }

  //printf("IFLAG %d %d\n",olist.check(),ilist.check());

  // close UDP port and print stats

  ::close(socket);
  stats();
}
