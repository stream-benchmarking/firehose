// MINNOW anomaly2
// anomaly detection from unbounded key range
// print found anomalies to screen

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

#include <tr1/unordered_map>

#include "phish.h"
#include "my_double_linked_list.h"

void packet(int);
void stats();
void options(int, char **);

/* ---------------------------------------------------------------------- */

// hard-coded settings from generator #2

#define NACTIVE 131072

// defaults for command-line switches

int nthresh = 24;
int mthresh = 4;
int nsenders = 1;
int maxactive = 2*NACTIVE;

// packet stats

uint64_t nrecv = 0;
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

// active set of keys

Akey *aset;
MyDoubleLinkedList<Akey*> list;
int nactive = 0;

// key-value hash table for received datums

std::tr1::unordered_map<uint64_t,Akey*> kv(1);

// output file handle

char *fname = NULL;
FILE *fp;

/* ---------------------------------------------------------------------- */

int main(int narg, char **args)
{
  phish_init(&narg,&args);
  phish_input(0,packet,NULL,1);
  phish_check();

  options(narg,args);

  // optional file output

  if (fname) {
    char fextend[16];
    sprintf(fextend,"%s.%d",fname,phish_query("idlocal",0,0));
    fp = fopen(fextend,"w");
  } else fp = stdout;

  // aset = active key set
  // stored as doubly-linked list so can delete least recently used

  aset = new Akey[nsenders*maxactive];
  //kv.reserve(maxactive);
  kv.rehash(ceil(nsenders*maxactive / kv.max_load_factor()));

  phish_loop();

  stats();
  fclose(fp);
  phish_exit();
}

/* ---------------------------------------------------------------------- */

void packet(int nvalues)
{
  char *buf,*datum;
  int len;
  Akey *akey;

  nrecv++;
  phish_unpack(&buf,&len);
  buf[len] = '\0';

  // scan past header line

  strtok(buf,"\n");

  // process variable # of datums in packet
  // ignore perpacket setting

  while (1) {
    if ((datum = strtok(NULL,",\n")) == NULL) break;
    uint64_t key = strtoul(datum,NULL,0);
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
            fprintf(fp,"false negative = %" PRIu64 "\n",key);
          } else ntrue++;
        } else {
          if (truth) {
            ptrue++;
            fprintf(fp,"true anomaly = %" PRIu64 "\n",key);
          } else {
            pfalse++;
            fprintf(fp,"false positive = %" PRIu64 "\n",key);
          }
        }
        akey->value = -1;
      }
    }
  }
}

/* ---------------------------------------------------------------------- */

// stats on packets and keys received, and anomalies found

void stats()
{
  fprintf(fp,"\n");
  fprintf(fp,"packets received = %" PRIu64 "\n",nrecv);
  fprintf(fp,"unique keys = %" PRIu64 "\n",nunique);
  fprintf(fp,"true anomalies = %d\n",ptrue);
  fprintf(fp,"false positives = %d\n",pfalse);
  fprintf(fp,"false negatives = %d\n",nfalse);
  fprintf(fp,"true negatives = %d\n",ntrue);
}

/* ---------------------------------------------------------------------- */

// parse command line
// optional arguments:
//   -t = check key for anomalous when appears this many times
//   -m = key is anomalous if value=1 <= this many times
//   -p = # of generators sending to me, used to pre-allocate hash table
//   -a = size of active key set
//   -f = filename for output

void options(int narg, char **args)
{
  int op;
  
  while ( (op = getopt(narg,args,"t:m:p:a:f:")) != EOF) {
    switch (op) {
    case 't':
      nthresh = atoi(optarg);
      break;
    case 'm':
      mthresh = atoi(optarg);
      break;
    case 'p':
      nsenders = atoi(optarg);
      break;
    case 'a':
      maxactive = atoi(optarg);
      maxactive *= 2;
      break;
    case 'f':
      if (strcmp(optarg,"NULL") == 0) fname = NULL;
      else fname = optarg;
      break;
    }
  }

  if (optind != narg) phish_error("Syntax: anomaly2 options");
}
