#include <math.h>
#include "gammafunction.h"

typedef struct {
  unsigned long max_keys;
  unsigned long *key_cdf;
  unsigned long median_index;
  unsigned long max_random;
  unsigned long uniq_key;
  double concentration;
} power_law_distribution_t;

// Create a new power-law distribution with the given parameters:
// concentration: must be > 0
//   as concentration increases, the distribution falls off more quickly
//   as concentration approaches 0.0, the distribution flattens out
// max_keys: maximum number of indices from which to draw a key
// max_random: maximum value of the random number generator

static power_law_distribution_t* 
power_law_initialize( double concentration, unsigned long max_keys, 
                     unsigned long max_random )
{
    power_law_distribution_t* dist = 
      (power_law_distribution_t*) malloc( sizeof(power_law_distribution_t) );
    dist->concentration = concentration;
    dist->max_random = max_random;
    dist->max_keys = max_keys;
    dist->key_cdf = (unsigned long*) calloc( max_keys, sizeof(unsigned long) );
    dist->uniq_key = max_keys;
    dist->median_index = 0;

    double cp1 = concentration+1.0;

    unsigned long i;
    double beta, cdf;
    for ( i = 0; i < dist->max_keys; i++ ) {
        if( i == 0 ) cdf = 0.0;
	else {
	  beta = exp( log_gamma_function( i ) + 
                      log_gamma_function( cp1 ) - 
                      log_gamma_function( i + cp1 ) );
	  cdf = 1.0 - i*beta;
	}

	// keep re-assigning the median until "cdf" is above 0.5

	if ( cdf >= 0.5 ) {
	    if( dist->median_index == 0 ) {
	        dist->median_index = i;
	    }
	}

	dist->key_cdf[i] = (unsigned long) floor( cdf*max_random );
    }

    return dist;
}

// Frees all the memory associated with the power-law distirbution
// dist: power-law distribution to free up

static void power_law_destroy( power_law_distribution_t *dist )
{
  free( dist->key_cdf );
  dist->key_cdf = NULL;
  free( dist );
}

// Draws a value from the power-law distribution and returns the index
// random_value: uniformly generated random number
// dist: power-law distribution to sample from

static unsigned long power_law_simulate( unsigned long random_value, 
                                        power_law_distribution_t *dist )
{
    // If we've got a uniform random value of zero, that's index zero

    if ( random_value == 0 ) return 0;

    unsigned long upper_index = dist->max_keys-1;
    unsigned long upper_value = dist->key_cdf[upper_index];

    if ( random_value >= upper_value ) {
      return dist->uniq_key++;
    }

    // Binary search

    unsigned long lower_index = 0;
    unsigned long lower_value = dist->key_cdf[lower_index];
    
    unsigned long guess_index = dist->median_index;
    unsigned long guess_value;
    unsigned long guess_low_value;

    int done = 0;
    while ( !done ) {
        guess_value = dist->key_cdf[guess_index];
	guess_low_value = dist->key_cdf[guess_index-1];
	if ( random_value <= guess_value ) {
	    if ( guess_low_value < random_value ) {
	      return guess_index;
	    } else if ( guess_low_value == random_value ) {
	      return guess_index-1;
	    } else {
	      // Answer is between guess and lower
              // Be leery of overflow
	      upper_index = guess_index;
	      upper_value = guess_value;
	      guess_index = lower_index + ((guess_index-lower_index) / 2); 
	    }
	}
	else {
	    // Answer is between guess and upper
	    lower_index = guess_index;
	    lower_value = guess_value;

	    // need to come up with a better way of approximating 
            //   the power law step up
	    unsigned long delta = (upper_index - guess_index) / 2 + 1;
	    guess_index = guess_index + delta; // Be leery of overflow
	}

	if ( lower_index == upper_index ) {
	    return lower_index;
	}
    }
    
    return lower_index;
}
