// Stream generator #3 = two-level active-set generator

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

#include "active_ring.h"
#include "evahash.h"
#include "udp_throw.h"

// UDP ports

udp_throw_t *udpt = NULL;

// hard-coded settings

#define DEFAULT_PORT 55555    // default port to write to
#define STOPRATE 100          // per sec rate of STOP packets
#define NACTIVE 131072        // size of active set
#define DATUM_PER_PACKET 40
#define CNT_TARGET 24
#define CNT_THRESHOLD 4

// defaults for command-line switches

uint64_t npacket = 10000000;
int perpacket = DATUM_PER_PACKET;
int rate = 0;
int numgen = 1;
int whichgen = 0;
uint32_t seed = 678912345;

// global data

int buflen;

uint64_t positive_cnt = 0;
uint64_t negative_cnt = 0;

uint64_t inner_key_cnt = 0;
uint64_t fp = 0;
uint64_t fn = 0;
uint64_t tp = 0;
uint64_t tn = 0;

uint64_t innerkeycnt = 0;
uint64_t sessionkeycnt = 0;

int do_exit = 0;

// data structures

typedef struct bias_el_t {
  uint8_t hasbias;
  uint16_t sum;
} bias_el_t;

typedef struct session_el_t {
  uint8_t hasbias; //does inner key have bias?
  uint16_t value;  //value to emit from outer key
  uint16_t sum;    //sum of inner key bias
  uint32_t cnt;    //count of specific inner key
  uint64_t key;    //key to emit from outer key
} session_el_t;

typedef struct _fill_pkt_t {
  char *buf;
  int offset;
} fill_pkt_t;

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
//   -s = random # seed for key/value generation, time-of-day if 0

void read_cmd_options(int argc, char ** argv)
{
  register int op;

  while ( (op = getopt(argc, argv, "n:b:r:p:x:s:")) != EOF) {
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
    case 's':
      seed = atoi(optarg);
      break;
    }
  }

  if (optind == argc) {
    fprintf(stderr,"Syntax: twolevel options hostname\n");
    exit(1);
  }

  int flag = 0;
  if (npacket <= 0 || perpacket <= 0 || rate < 0) flag = 1;
  if (whichgen < 0 || whichgen >= numgen) flag = 1;
  if (seed < 0) flag = 1;
  if (flag) {
    fprintf(stderr,"ERROR: invalid command-line switch\n");
    exit(1);
  }

  // setup a UDP port for each argument

  while (optind < argc) {
    udp_throw_init_peer(udpt,argv[optind],DEFAULT_PORT);
    optind++;
  }
}

// comment

static inline void update_inner_output_stats(session_el_t * sess)
{
  if (sess->cnt == 1) inner_key_cnt++;
  if (sess->cnt == CNT_TARGET) {
    if (sess->sum <= CNT_THRESHOLD) {
      //fprintf(stderr,"Bias Key %"PRIu64"\n", sess->key);
      positive_cnt++;
      if (sess->hasbias) {
        tp++;
        //fprintf(stderr,"true anomaly = %"PRIu64"\n", sess->key);
      } else {
        fp++;
        //fprintf(stderr,"false positive = %"PRIu64"\n", sess->key);
      }
    } else {
      negative_cnt++;
      if (sess->hasbias) {
        fn++;
        //fprintf(stderr,"false negative = %"PRIu64"\n", sess->key);
      } else tn++;
    }
  }
}

static void print_session_el(active_ring_t *aring, 
                             active_el_t *el, void *vfill)
{
  fill_pkt_t * fill = (fill_pkt_t*) vfill;
  session_el_t * sess = (session_el_t*) el->data;
  
  uint16_t val;
  
  switch(el->cnt) {
  case 1:
    val = sess->key & 0xFFFF;
    break;
  case 2:
    val = (sess->key >> 16) & 0xFFFF;
    break;
  case 3:
    val = (sess->key >> 32) & 0xFFFF;
    break;
  case 4:
    val = (sess->key >> 48) & 0xFFFF;
    break;
  case 5:
    val = sess->value;
    update_inner_output_stats(sess);
    //printf("outer key %"PRIu64" inner key %"PRIu64"\n",el->id, sess->key);
    break;
  default:
    val = 9999;
  }
  
  int rtn = snprintf(fill->buf + fill->offset, buflen - fill->offset,
                     "%" PRIu64 ",%02u,%u\n", el->id, val, el->cnt);
  if (rtn > 0) fill->offset += rtn;
  else fprintf(stderr,"ERROR: printing to buffer\n");
}

// comment

static inline int print_packet(active_ring_t *aring, uint32_t packetcnt,
                               fill_pkt_t *fill, int offset)
{
  fill->offset = offset;
  while (packetcnt) {
    active_call_next_element(aring,fill,print_session_el);
    packetcnt--;
  }
  return fill->offset;
}

// comment

static void set_inner_bias(active_ring_t * aring, active_el_t * el,
                           void * vsess)
{
  session_el_t * sess = (session_el_t*)vsess;
  bias_el_t * bias = (bias_el_t*)el->data;
  
  sess->key = el->id;
  
  if (bias->hasbias) {
    sess->value = ((rand_r(aring->seed) & 0xF) == 0);
  }
  else {
    sess->value = rand_r(aring->seed) & 0x1;
  }
  bias->sum += sess->value;

  //copy value for use in tracking stats
  sess->sum = bias->sum;
  sess->cnt = el->cnt;
  sess->hasbias = bias->hasbias;
  
  sess->value += bias->hasbias * 10;
}

// comment

static void create_bias_el(active_ring_t * aring, active_el_t * el,
                           void * vignore)
{
  bias_el_t * bias = (bias_el_t*)el->data;
  bias->hasbias =
    (evahash((uint8_t*)&el->id,
             sizeof(uint64_t), 0x123456) & 0xFF) == 0x11; 
  innerkeycnt++;
}

// comment

static void create_session_el(active_ring_t * aring, active_el_t * el, 
                              void * vinner)
{
  active_ring_t * inner_ring = (active_ring_t*)vinner;
  session_el_t * sess = (session_el_t*)el->data;
  
  if (el->total >= 5) {
    active_call_next_element(inner_ring, sess, set_inner_bias);
    el->total = 5;
  }
  else {
    sess->key = ((uint64_t)rand_r(aring->seed) << 32) + 
      (uint64_t)rand_r(aring->seed);
  }
  sessionkeycnt++;
}

// stats on unique keys generated

static void print_stats(active_ring_t *aring, active_ring_t *inner_ring)
{
  fprintf(stdout,"unique outer keys generated = %" PRIu64 "\n",
          aring->keys_generated);
  fprintf(stdout,"unique inner keys generated = %" PRIu64 "\n",
          inner_key_cnt);
  fprintf(stdout,"  positive inner keys = %" PRIu64 "\n",positive_cnt);
  fprintf(stdout,"  negative inner keys  = %" PRIu64 "\n",negative_cnt);
  fprintf(stdout,"  true anomalies  = %" PRIu64 "\n",tp);
  fprintf(stdout,"  false positives = %" PRIu64 "\n",fp);
  fprintf(stdout,"  false negatives = %" PRIu64 "\n",fn);
  fprintf(stdout,"  true negatives  = %" PRIu64 "\n",tn);
}

// handle signal interrupts to exit code

void clean_exit(int sig)
{
  fprintf(stderr, "exit %d called\n", sig);
  do_exit++;
  if (do_exit > 3) {
    _exit(0);
  }
}

// main program

int main(int argc, char **argv)
{
  udpt = udp_throw_init(0);
  
  if (!udpt) {
    fprintf(stderr,"ERROR: unable to generate data\n");
    return -1;
  }

  read_cmd_options(argc,argv);

  // initialize RNG and seed
  // using different seed in parallel will create disjoint key spaces

  if (!seed) seed = (unsigned int) time(NULL);
  seed += whichgen;
  srand(seed);

  // create active sets

  fprintf(stdout,"building active set ...\n");
  active_ring_t *inner_ring = active_init(16384,NACTIVE,65536,seed,
                                          sizeof(bias_el_t),NULL,
                                          create_bias_el,-0.9);
  
  active_ring_t * aring = active_init(16384,NACTIVE,65536,seed,
                                      sizeof(session_el_t),inner_ring,
                                      create_session_el,-1.1);
  
  // packet buffer length of 64 bytes per datum is ample

  fill_pkt_t fill;
  buflen = 64*perpacket;
  fill.buf = (char *) malloc(buflen*sizeof(char));
  memset(fill.buf,0,buflen);

  //catch signals to report output on interrupt exit

  signal(SIGTERM, clean_exit);
  signal(SIGINT,  clean_exit);
  signal(SIGQUIT, clean_exit);
  signal(SIGABRT, clean_exit);

  // generate the datums in packets

  fprintf(stdout,"starting generator ...\n");
  fflush(stdout);

  double timestart = myclock();
 
  uint64_t i = 0; 
  for (i = 0; i < npacket; i++) {

    // packet header

    int offset = snprintf(fill.buf,buflen,"packet %" PRIu64 "\n",
                          i*numgen+whichgen);

    // create packet containing perpacket datums

    offset = print_packet(aring,perpacket,&fill,offset);
    
    // write the packet to UDP port(s)

    udp_throw_data(udpt,fill.buf,offset);

    if (rate) {
      double n = 1.0*(i+1)*perpacket;
      double elapsed = myclock() - timestart;
      double actual_rate = n/elapsed;
      if (actual_rate > rate) {
        double delay = n/rate - elapsed;
        usleep(1.0e6*delay);
      }
    }
    if (do_exit) {
         break;
    }
  }
  
  double timestop = myclock();

  print_stats(aring,inner_ring);
  uint64_t ndatum = i * perpacket;
  fprintf(stdout,"packets emitted = %" PRIu64 "\n",i);
  fprintf(stdout,"datums emitted = %" PRIu64 "\n",ndatum);
  fprintf(stdout,"elapsed time (secs) = %g\n",timestop-timestart);
  fprintf(stdout,"generation rate (datums/sec) = %g\n",
          ndatum/(timestop-timestart));
  fclose(stdout);

  // loop on sending one-integer STOP packets, STOPRATE per sec
  // allows receivers to shut down cleanly, even if behind

  int offset = snprintf(fill.buf,buflen,"%d\n",whichgen);
  do_exit = 0;
  while (1) {
    udp_throw_data(udpt,fill.buf,offset);
    usleep(1000000/STOPRATE);
    if (do_exit) break;
  }

  udp_throw_destroy(udpt);
  
  return 0;
}
