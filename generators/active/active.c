// Stream generator #2 = active-set generator

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

#include "evahash.h"
#include "udp_throw.h"

// UDP ports

udp_throw_t *udpt = NULL;

// hard-coded settings

#define DEFAULT_PORT 55555    // default port to write to
#define STOPRATE 100          // per sec rate of STOP packets
#define NACTIVE 131072        // size of active set
#define MAX_DISTRO 65536
#define TREND_POINTS 1024
#define CNT_TARGET 24
#define CNT_THRESHOLD 4
#define POW_EXP (-1.5)

unsigned int vseed = 98765;        // RN seed for generating values

// defaults for command-line switches

uint64_t npacket = 1000000;
int perpacket = 50;
int rate = 0;
int numgen = 1;
int whichgen = 0;
unsigned int kseed = 678912345;
uint32_t mask = 12345;

// data structures

typedef struct _active_el_t {
  uint64_t id;
  uint32_t total;
  uint32_t cnt;
  uint32_t bias;
  uint32_t acc;
  struct _active_el_t * next;
} active_el_t;

typedef struct _active_q_t {
  active_el_t * head;
  int cnt;
} active_q_t;

typedef struct _active_ring_t {
  active_q_t *qset;
  uint32_t max;
  uint32_t current;
  uint32_t offset[TREND_POINTS];
  uint32_t cnt_dist[MAX_DISTRO];
  active_el_t *el_set;
  uint64_t nextid;
  uint64_t keys_generated;

  active_el_t *active_head;
  uint32_t *kseed;
  uint32_t *vseed;
  uint32_t idseed;
  uint32_t mask;

  uint64_t tp;
  uint64_t fn;
  uint64_t fp;
  uint64_t tn;
} active_ring_t;

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
//   -m = random mask for flagging keys as anomalous

void read_cmd_options(int argc, char ** argv)
{
  register int op;

  while ( (op = getopt(argc, argv, "n:b:r:p:x:s:m:")) != EOF) {
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
      kseed = atoi(optarg);
      break;
    case 'm':
      mask = atoi(optarg);
      break;
    }
  }
  
  if (optind == argc) {
    fprintf(stderr,"Syntax: active options hostname\n");
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
}

// comment

static void generate_trend_distribution(active_ring_t * aring)
{
  double y[TREND_POINTS];
  double iter = 12/(double)TREND_POINTS;
  int i;
  double maxy = 0;
  for (i = 0; i < TREND_POINTS; i++) {
    double x = (double)i * iter;
    y[i] = 1 / (1 + exp(2-x)) - (1 / (1 + exp(5-x)));
    if (y[i] > maxy) {
      maxy = y[i];
    }
    // plot
    //printf("%f %f %f\n",x,y[i],iter);
  }
  for (i = 0; i < TREND_POINTS; i++) {
    aring->offset[i] = (uint32_t)trunc(((double)aring->max/2) * 
                                       (maxy - y[i])/maxy);
    if (aring->offset[i]== 0) {
      aring->offset[i] = 1;
    }
    // plot
    //printf("%d %u\n",i,aring->offset[i]);
  }
}

// comment

static void generate_cnt_distribution(active_ring_t * aring, uint32_t max_cnt)
{
  double max = 1;
  double min = pow((double)MAX_DISTRO, POW_EXP);
  double inv_diff = 1/(max - min);

  int i;
  for (i = 1; i <= MAX_DISTRO; i++) {
    double v =
      (double)max_cnt * (pow((double)i,POW_EXP) - min) * inv_diff;
    aring->cnt_dist[i] = (uint32_t)floor(v) + 1;
  }
}

// comment 

static inline void reset_new_el(active_ring_t * aring, active_el_t * el) {
  el->id =
    ((uint64_t)evahash((uint8_t*)&aring->nextid,
                       sizeof(uint64_t),aring->idseed) << 32)
    | (uint64_t)evahash((uint8_t*)&aring->nextid,
                        sizeof(uint64_t),aring->idseed + 1);
  
  aring->nextid++;
  
  el->cnt = 0;
  el->acc = 0;
  el->total = aring->cnt_dist[rand_r(aring->kseed)%MAX_DISTRO];
  
  el->bias = (evahash((uint8_t*)&el->id,
                      sizeof(uint64_t),aring->mask) & 0xFF) == 0x11;
  
  // select q in ring

  int q = rand_r(aring->kseed) % aring->max;
  
  // add el to ring

  el->next = aring->qset[q].head;
  aring->qset[q].head = el;
  aring->qset[q].cnt++;
}

// comment

static inline void reset_el(active_ring_t * aring, active_el_t * el)
{
  if (!el->cnt) aring->keys_generated++;
  el->cnt++;

  if (el->cnt == CNT_TARGET) {
    if (el->bias) {
      if (el->acc <= CNT_THRESHOLD) aring->tp++;
      else aring->fn++;
    }
    else {
      if (el->acc <= CNT_THRESHOLD) aring->fp++;
      else aring->tn++;
    }
  }
  if (el->cnt >= el->total) {
    reset_new_el(aring, el);
    return;
  }
  
  // select q in ring

  int pos = (el->cnt * TREND_POINTS) /el->total;
  uint32_t offset = (aring->current + aring->offset[pos]) % aring->max;
  
  // add el to ring
  el->next = aring->qset[offset].head;
  aring->qset[offset].head = el;
  aring->qset[offset].cnt++;
}

// comment 

static active_ring_t *build_rings(uint32_t max_rings, uint32_t max_active, 
                                  uint32_t max_cnt, uint32_t kseed, 
                                  uint32_t vseed, uint32_t mask)
{
  active_ring_t * aring = (active_ring_t *) calloc(1, sizeof(active_ring_t));
  if (!aring) return NULL;
  
  aring->max = max_rings;
  aring->qset = (active_q_t*)calloc(max_rings, sizeof(active_q_t));
  if (!aring->qset) return NULL;

  generate_cnt_distribution(aring, max_cnt);
  generate_trend_distribution(aring);
  
  aring->el_set = (active_el_t*)calloc(max_active, sizeof(active_el_t));
  if (!aring->el_set) return NULL;
  aring->kseed = (uint32_t *)calloc(1,sizeof(uint32_t));
  aring->vseed = (uint32_t *)calloc(1,sizeof(uint32_t));
  
  *aring->kseed = kseed;
  *aring->vseed = vseed;
  aring->idseed = rand_r(aring->kseed);
  aring->mask = mask;
  
  int i;
  for (i = 0; i < max_active; i++)
    reset_new_el(aring,&aring->el_set[i]);
  
  return aring;
}

// comment

static inline void print_el(active_ring_t *aring, active_el_t *el,
                            char * buf, int len, int *poffset)
{
  int offset = *poffset;
  int val = rand_r(aring->vseed);
  int bv;
  if (el->bias) {
    bv = ((val & 0xF) == 3);
  }
  else {
    bv = val & 0x1;
  }
  el->acc += bv;
  int rtn = snprintf(buf + offset, len - offset, "%" PRIu64 ",%u,%u\n", 
                     el->id, bv, el->bias);
  /* int rtn = snprintf(buf + offset, len - offset, "%" PRIu64 ",%u,%u,%u,%u\n",
    el->id, bv, el->bias, el->cnt, el->total);*/

  *poffset = offset + rtn;
}

// generate one packet with perpacket datums

static inline int print_packet(active_ring_t *aring, int perpacket,
                               char *buf, int buflen, int offset) 
{
  while (perpacket) {
    if (!aring->active_head) {
      aring->current++;
      if (aring->current >= aring->max) aring->current = 0;
      aring->active_head = aring->qset[aring->current].head;
      aring->qset[aring->current].head = NULL;
      aring->qset[aring->current].cnt = 0;
    }
    
    if (aring->active_head) {
      active_el_t *el = aring->active_head;
      aring->active_head = el->next;
      print_el(aring,el,buf,buflen,&offset);
      reset_el(aring,el);
      perpacket--;
    }
  }

  return offset;
}

// stats on unique keys generated

static void print_stats(active_ring_t *aring)
{
  fprintf(stdout,"unique keys generated = %" PRIu64 "\n",
          aring->keys_generated);
  fprintf(stdout,"  true anomalies  = %" PRIu64 "\n",aring->tp);
  fprintf(stdout,"  false positives = %" PRIu64 "\n",aring->fp);
  fprintf(stdout,"  false negatives = %" PRIu64 "\n",aring->fn);
  fprintf(stdout,"  true negatives = %" PRIu64 "\n",aring->tn);
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
  
  // initialize RNG and seeds
  // using different kseed in parallel will create disjoint key spaces

  if (!kseed) kseed = (unsigned int) time(NULL);
  kseed += whichgen;
  vseed += whichgen;
  srand(kseed);

  // create active set

  fprintf(stdout,"building active set ...\n");
  active_ring_t *aring = build_rings(16384,NACTIVE,65536,kseed,vseed,mask);

  // packet buffer length of 64 bytes per datum is ample

  int buflen = 64*perpacket;
  char *buf = (char *) malloc(buflen*sizeof(char));
  memset(buf,0,buflen);

  // generate the datums in packets

  fprintf(stdout,"starting generator ...\n");
  fflush(stdout);

  double timestart = myclock();

  for (uint64_t i = 0; i < npacket; i++) {

    // packet header

    int offset = snprintf(buf,buflen,"packet %" PRIu64 "\n",i*numgen+whichgen);

    // create packet containing perpacket datums

    offset = print_packet(aring,perpacket,buf,buflen,offset);

    // write the packet to UDP port(s)

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

  print_stats(aring);
  uint64_t ndatum = npacket * perpacket;
  fprintf(stdout,"packets emitted = %" PRIu64 "\n",npacket);
  fprintf(stdout,"datums emitted = %" PRIu64 "\n",ndatum);
  fprintf(stdout,"elapsed time (secs) = %g\n",timestop-timestart);
  fprintf(stdout,"generation rate (datums/sec) = %g\n",
          ndatum/(timestop-timestart));
  fclose(stdout);

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
