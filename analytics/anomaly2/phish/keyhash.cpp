// MINNOW keyhash
// disperse datums via key hashing to downstream minnows

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdexcept>

#include "phish.hpp"
#include "hashlittle.h"

void packet(int);
void flush();
void options(int, char **);

/* ---------------------------------------------------------------------- */

// defaults for command-line switches

int peroutput = 50;        // # of datums per packet for downstream sends

int nout;                  // # of downstream minnows
char **obuf;               // output buffers, one per downstream receiver
int *pcount;               // packet count for each receiver
int *dcount;               // datum count for each receiver
int *offset;               // current offest into each buffer
int bufmax;                // size of output buffers

/* ---------------------------------------------------------------------- */

int main(int narg, char **args)
{
  phish_init(&narg,&args);
  phish_input(0,packet,flush,1);
  phish_output(0);
  phish_check();

  options(narg,args);

  nout = phish_query("outport/direct",0,0);
  pcount = new int[nout];
  dcount = new int[nout];
  offset = new int[nout];
  obuf = new char*[nout];

  // assume 1 Mbyte is adequate for incoming and outgoing packets

  bufmax = 1024*1024;

  for (int i = 0; i < nout; i++) {
    pcount[i] = dcount[i] = 0;
    obuf[i] = new char[bufmax];
    offset[i] = snprintf(obuf[i],bufmax,"packet %d\n",pcount[i]);
  }

  phish_loop();
  phish_exit();
}

/* ---------------------------------------------------------------------- */

void packet(int nvalues)
{
  char *buf,*datum;
  int len;
  phish_unpack(&buf,&len);
  buf[len] = '\0';

  // scan past header line

  strtok(buf,"\n");

  // process all datums in packet

  while (1) {

    // copy datum to an output buffer, based on hash of key

    if ((datum = strtok(NULL,"\n")) == NULL) break;
    uint64_t key = strtoul(datum,NULL,0);
    int m = hashlittle((char *)&key,sizeof(uint64_t),12345) % nout;
    offset[m] += snprintf(obuf[m]+offset[m],bufmax-offset[m],"%s\n",datum);
    dcount[m]++;

    // send buffer downstream when output buffer is full
    // reset buffer with new packet ID

    if (dcount[m] == peroutput) {
      phish_pack_raw(obuf[m],offset[m]);
      phish_send_direct(0,m);
      obuf[m][offset[m]] = '\0';
      dcount[m] = 0;
      pcount[m]++;
      offset[m] = snprintf(obuf[m],bufmax,"packet %d\n",pcount[m]);
    }
  }
}

/* ---------------------------------------------------------------------- */

// flush output buffers when no more packets will be received

void flush()
{
  for (int m = 0; m < nout; m++) {
    obuf[m][offset[m]] = '\0';
    if (dcount[m]) {
      phish_pack_raw(obuf[m],offset[m]);
      phish_send_direct(0,m);
      pcount[m]++;
    }
  }
}

/* ---------------------------------------------------------------------- */

// parse command line
// optional arguments:
//   -b 50 = output bundling factor = # of datums in one output packet

void options(int narg, char **args)
{
  int op;
  
  while ( (op = getopt(narg,args,"b:")) != EOF) {
    switch (op) {
    case 'b':
      peroutput = atoi(optarg);
      break;
    }
  }

  if (optind != narg) phish_error("Syntax: keyhash options");
}
