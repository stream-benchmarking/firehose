// Stream generator #1 = biased power-law generator

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#include "udp_throw.h"
#include "powerlaw.h"
#include "evahash.h"

// UDP ports

udp_throw_t *udpt = NULL;

// hard-coded settings

#define DEFAULT_PORT 55555    // default port to write to
#define STOPRATE 100          // per sec rate of STOP packets

double concentration = 0.5;        // slope of power-law distribution
unsigned long maxkeys = 100000;    // range of keys to generate from
unsigned int vseed = 98765;        // RN seed for generating values

// defaults for command-line switches

uint64_t npacket = 1000000;
int perpacket = 50;
int rate = 0;
uint64_t keyoffset = 1000000000;
int numgen = 1;
int whichgen = 0;
unsigned int kseed = 678912345;
uint32_t mask = 12345;

// timer, returns current time in seconds with usec precision

double myclock()
{
  double time;
  struct timeval tv;
  
  gettimeofday(&tv,NULL);
  time = 1.0 * tv.tv_sec + 1.0e-6 * tv.tv_usec;
  return time;
}

// parse command line
// regular arguments = 1 or more host/port pairs to write to
// optional arguments:
//   -n = # of packets to generate
//   -b = bundling factor = # of datums in one packet
//   -r = desired rate of generation (datums/sec)
//   -p = # of generators operating in parallel
//   -x = which of parallel generators I am (0 to N-1)
//   -o = offset value for all generated keys
//   -s = random # seed for key/value generation, time-of-day if 0
//   -m = random mask for flagging keys as anomalous

void read_cmd_options(int argc, char ** argv)
{
  register int op;

  while ((op = getopt(argc,argv,"n:b:r:p:x:o:s:m:")) != EOF) {
    switch (op) {
    case 'n':
      npacket = strtoul(optarg,NULL,0);
      break;
    case 'b':
      perpacket = atoi(optarg);
      break;
    case 'r':
      rate = atoi(optarg);
      break;
    case 'p':
      numgen = atoi(optarg);
      break;
    case 'x':
      whichgen = atoi(optarg);
      break;
    case 'o':
      keyoffset = atoi(optarg);
      break;
    case 's':
      kseed = atoi(optarg);
      break;
    case 'm':
      mask = atoi(optarg);
      break;
    }
  }

  if (optind == argc) {
    fprintf(stderr,"Syntax: powerlaw options hostname\n");
    exit(1);
  }

  int flag = 0;
  if (npacket <= 0 || perpacket <= 0 || rate < 0) flag = 1;
  if (whichgen < 0 || whichgen >= numgen) flag = 1;
  if (kseed < 0 || mask <= 0) flag = 1;
  if (flag) {
    fprintf(stderr, "ERROR: invalid command-line switch\n");
    exit(1);
  }

  // setup a UDP port for each argument

  while (optind < argc) {
    udp_throw_init_peer(udpt,argv[optind],DEFAULT_PORT);
    optind++;
  }

  if (!udpt->clientcnt) {
    fprintf(stderr,"ERROR: no clients\n");
    exit(1);
  }
}

// main program

int main(int argc, char ** argv)
{
  udpt = udp_throw_init(0);

  if (!udpt) {
    fprintf(stderr,"ERROR: unable to generate data\n");
    return -1;
  }
    
  read_cmd_options(argc,argv);

  // initialize RNG and seeds
  // use different keyoffsets in parallel, so key spaces are disjoint

  if (!kseed) kseed = (unsigned int) time(NULL);
  kseed += whichgen;
  vseed += whichgen;
  srand(kseed);
  keyoffset += whichgen*keyoffset;

  // create power-law distribution

  fprintf(stdout,"initializing power-law distribution ...\n");
  power_law_distribution_t * power = 
    power_law_initialize(concentration,maxkeys,RAND_MAX);
  
  // packet buffer length of 64 bytes per datum is ample

  int buflen = 64*perpacket;
  char *buf = (char *) malloc(buflen*sizeof(char));
  memset(buf,0,buflen);

  // generate the datums in packets

  fprintf(stdout,"starting generator ...\n");
  fflush(stdout);

  // plot
  //int *ycount = (int *) malloc(maxkeys*sizeof(int));
  //for (int i = 0; i < maxkeys; i++) ycount[i] = 0;

  double timestart = myclock();

  for (uint64_t i = 0; i < npacket; i++) {

    // packet header

    int offset = snprintf(buf,buflen,"packet %" PRIu64 "\n",i*numgen+whichgen);

    // generate one packet with perpacket datums

    uint64_t key;
    for (int j = 0; j < perpacket; j++) {
      uint64_t rn = rand_r(&kseed);
      while ((key = power_law_simulate(rn,power)) >= maxkeys)
        rn = rand_r(&kseed);

      // plot
      //ycount[key]++;

      key += keyoffset;
      uint32_t value = 0;
      uint32_t bias = 0;
      if ((evahash((uint8_t*) &key,sizeof(uint64_t),mask) & 0xFF) == 0x11) {
        bias = 1;
        value = ((rand_r(&vseed) & 0xF) == 0);
      } else value = rand_r(&vseed) & 0x1;

      offset += snprintf(buf+offset,buflen-offset,
                         "%" PRIu64 ",%u,%u\n",key,value,bias);
    }

    // write packet to UDP port(s)

    udp_throw_data(udpt,buf,offset);

    // sleep if rate is throttled 

    if (rate) {
      double n = 1.0*(i+1)*perpacket;
      double elapsed = myclock() - timestart;
      double actual_rate = n/elapsed;
      if (actual_rate > rate) {
        double delay = n/rate - elapsed;
        usleep(1.0e6*delay);
      }
    }
  }

  double timestop = myclock();

  // plot
  //FILE *fp = fopen("tmp.plot","w");
  //for (int i = 0; i < maxkeys; i++) 
  //  fprintf(fp,"%d %d\n",i,ycount[i]);
  //fclose(fp);

  uint64_t ndatum = npacket * perpacket;
  fprintf(stdout,"packets emitted = %" PRIu64 "\n",npacket);
  fprintf(stdout,"datums emitted = %" PRIu64 "\n",ndatum);
  fprintf(stdout,"elapsed time (secs) = %g\n",timestop-timestart);
  fprintf(stdout,"generation rate (datums/sec) = %g\n",
          ndatum/(timestop-timestart));
  fclose(stdout);

  power_law_destroy(power);

  // loop on sending one-integer STOP packets, STOPRATE per sec
  // allows receivers to shut down cleanly, even if behind

  int offset = snprintf(buf,buflen,"%d\n",whichgen);
  while (1) {
    udp_throw_data(udpt,buf,offset);
    usleep(1000000/STOPRATE);
  }

  udp_throw_destroy(udpt);

  return 0;
}
