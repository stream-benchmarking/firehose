#ifndef _ACTIVE_RING_H
#define _ACTIVE_RING_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include "evahash.h"


#define MAX_DISTRO 65536

#define TREND_POINTS 1024

//define slope of power law distribution
#define POW_EXP (-1.5)

typedef struct _active_el_t {
	uint64_t id;
	uint32_t total;
	uint32_t cnt;
	struct _active_el_t * next;
     uint8_t data[8];  //can be larger - based on el_data_len
} active_el_t;

typedef struct _active_q_t {
	active_el_t * head;
     active_el_t * tail;
	int cnt;
} active_q_t;

struct _active_ring_t;
typedef struct _active_ring_t active_ring_t;

typedef void (* active_process_el_call)(active_ring_t *, active_el_t *, void *);

struct _active_ring_t {
	active_q_t * qset;
	uint32_t max;
	uint32_t current;
	uint32_t offset[TREND_POINTS];
	uint32_t cnt_dist[MAX_DISTRO];
	uint8_t * el_set;
	uint64_t nextid;
	uint64_t keys_generated;
     int el_data_len;

	active_el_t * active_head;
	uint32_t * seed;
	uint32_t idseed;
     void * callback_data;
     active_process_el_call  create_el_callback;
};

static void active_generate_trend_distribution(active_ring_t * aring) {
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
		//printf("y %f %f %f\n", y[i], x, iter);
	}
	for (i = 0; i < TREND_POINTS; i++) {
		aring->offset[i] = (uint32_t)trunc(((double)aring->max - 1) * (maxy - y[i])/maxy);
		if (aring->offset[i]== 0) {
			aring->offset[i] = 1;
		}
		//printf("trend %u\n", aring->offset[i]);
	}
}

static void active_generate_cnt_distribution(active_ring_t * aring, uint32_t max_cnt, double powexp) {

     double max = 1;
     double min = pow((double)MAX_DISTRO, powexp);
     double inv_diff = 1/(max - min);

     int i;
     for (i = 1; i <= MAX_DISTRO; i++) {
          double v =
               (double)max_cnt * (pow((double)i,powexp) - min) * inv_diff;
          aring->cnt_dist[i] = (uint32_t)floor(v) + 1;
     }
}

static inline void active_add_to_qset(active_q_t * qset, active_el_t * el) {
     el->next = NULL;
     if (qset->tail) {
          qset->tail->next = el;
          qset->tail = el;
     }
     else {
          qset->head = el;
          qset->tail = el;
     }
	qset->cnt++;
}

static inline void active_reset_new_el(active_ring_t * aring, active_el_t * el) {
     el->id =
          ((uint64_t)evahash((uint8_t*)&aring->nextid, sizeof(uint64_t), aring->idseed) << 32)
          | (uint64_t)evahash((uint8_t*)&aring->nextid, sizeof(uint64_t), aring->idseed +1);

	aring->nextid++;

	el->cnt = 0;
	el->total = aring->cnt_dist[rand_r(aring->seed)%MAX_DISTRO];
     if (aring->el_data_len) {
          memset(el->data, 0, sizeof(aring->el_data_len));
     }

     if (aring->create_el_callback) {
          aring->create_el_callback(aring, el, aring->callback_data);
     }

	//select q in ring
	int q = rand_r(aring->seed) % aring->max;

     active_add_to_qset(&aring->qset[q], el);
}

static inline void active_reset_el(active_ring_t * aring, active_el_t * el) {
	

     //do something with updating based on counts

	if (el->cnt >= el->total) {
		active_reset_new_el(aring, el);
		return;
	}

	//select q in ring
	int pos = (el->cnt * TREND_POINTS) /el->total;
	uint32_t offset = (aring->current + aring->offset[pos]) % aring->max;

	//add el to ring
     active_add_to_qset(&aring->qset[offset], el);
}

static active_ring_t * active_init(uint32_t max_rings, uint32_t max_active, uint32_t max_cnt, uint32_t seed,
                                   size_t el_data_len, void * cdata,
                                   active_process_el_call create_call,
                                   double powexp) {

	active_ring_t * aring = (active_ring_t *)calloc(1, sizeof(active_ring_t));
	if (!aring) {
		return NULL;
	}

     aring->callback_data = cdata;
     aring->create_el_callback = create_call;

	aring->max = max_rings;
	aring->qset = (active_q_t*)calloc(max_rings, sizeof(active_q_t));
	if (!aring->qset) {
		return NULL;
	}
     if (!powexp) {
          powexp = POW_EXP;
     }

	active_generate_cnt_distribution(aring, max_cnt, powexp);
	active_generate_trend_distribution(aring);

     uint32_t elrec = sizeof(active_el_t);
     aring->el_data_len = el_data_len;
     if (el_data_len > 8) {
          elrec += el_data_len - 8;
     }
     //round to the nearest 8 bytes
     elrec = (elrec + 7) & 0xFFFFFFF8;

	aring->el_set = (uint8_t*)calloc(max_active, elrec);
	if (!aring->el_set) {
		return NULL;
	}

	aring->seed = (uint32_t *)calloc(1, sizeof(uint32_t));

	*aring->seed = seed;

	aring->idseed = rand_r(aring->seed);

	int i;
	for (i = 0; i < max_active; i++) {
          active_el_t * el = (active_el_t*)(aring->el_set + i * elrec);
		active_reset_new_el(aring, el);
	}

	return aring;
}

static inline int active_call_next_element(active_ring_t * aring, void * data,
                                          active_process_el_call call) {

     while (!aring->active_head) {
          aring->current++;
          if (aring->current >= aring->max) {
               aring->current = 0;
          }
          aring->active_head = aring->qset[aring->current].head;
          aring->qset[aring->current].head = NULL;
          aring->qset[aring->current].tail = NULL;
          aring->qset[aring->current].cnt = 0;
     }

     active_el_t * el = aring->active_head;
     aring->active_head = el->next;
     if (!el->cnt) {
          aring->keys_generated++;
     }
	el->cnt++;
     call(aring, el, data);
     active_reset_el(aring, el);
     return 1;
} 

#endif //ACTIVE_RING_H
