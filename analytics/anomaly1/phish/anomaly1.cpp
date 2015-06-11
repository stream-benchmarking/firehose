// MINNOW anomaly1
// anomaly detection from fixed key range
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

void packet(int);
void stats();
void options(int, char **);

/* ---------------------------------------------------------------------- */

// hard-coded settings from generator #1

unsigned long maxkeys = 100000;

// defaults for command-line switches

int nthresh = 24;
int mthresh = 4;
int nsenders = 1;

// packet stats

uint64_t nrecv = 0;

// anomaly stats

int ptrue = 0;
int pfalse = 0;
int nfalse = 0;
int ntrue = 0;

// key-value hash table for received datums

std::tr1::unordered_map<uint64_t, std::pair<int,int> > kv(1);

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

  // pre-allocate kv to maxkeys per generator, in agreement with generator #1

  //kv.rehash(nsenders*maxkeys)
  kv.rehash(ceil(nsenders*maxkeys / kv.max_load_factor()));

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

  nrecv++;
  phish_unpack(&buf,&len);
  buf[len] = '\0';

  // scan past header line

  strtok(buf,"\n");

  // process all datums in packet

  while (1) {
    if ((datum = strtok(NULL,",\n")) == NULL) break;
    uint64_t key = strtoul(datum,NULL,0);
    uint32_t value = strtoul(strtok(NULL,",\n"),NULL,0);
    uint32_t truth = strtoul(strtok(NULL,",\n"),NULL,0);

    // store datum in hash table
    // value = (N,M)
    // N = # of times key has been seen
    // M = # of times flag = 1
    // M < 0 means already seen N times
    
    if (!kv.count(key))
      kv[key] = std::pair<int,int>(1,value);

    else {
      std::pair<int,int>& tuple = kv[key];
      if (tuple.second < 0) {
        ++tuple.first;
        continue;
      }

      ++tuple.first;
      tuple.second += value;

      if (tuple.first == nthresh) {
        if (tuple.second > mthresh) {
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
        tuple.second = -1;
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

  std::tr1::unordered_map<uint64_t, std::pair<int,int> >::iterator i;
  int kmax = 0;
  for (i = kv.begin(); i != kv.end(); i++) {
    std::pair<int,int>& tuple = i->second;
    if (tuple.first > kmax) kmax = tuple.first;
  }

  fprintf(fp,"unique keys = %d\n",(int) kv.size());
  fprintf(fp,"max occurrence of any key = %d\n",kmax);
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
//   -f = filename for output

void options(int narg, char **args)
{
  int op;
  
  while ( (op = getopt(narg,args,"t:m:p:f:")) != EOF) {
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
    case 'f':
      if (strcmp(optarg,"NULL") == 0) fname = NULL;
      else fname = optarg;
      break;
    }
  }

  if (optind != narg) phish_error("Syntax: anomaly1 options");
}
