#ifndef _GAMMAFUNCTION_H_
#define _GAMMAFUNCTION_H_

#include <math.h>

/**
 * Computes the natural logarithm of the Gamma function.
 */

static inline double log_gamma_function( double input )
{

#ifdef lgamma
     return lgamma( input );
#else
     
     if( input <= 0.0 )
     {
	  // the input must be strictly greater than 0.0
	  return -1.0;
     }
     
     double xx = input;
     double tmp, ser;
     tmp = xx + 4.5;
     tmp -= (xx - 0.5) * log( tmp );
     
     ser = 1.000000000190015
       + (76.18009172947146 / xx)
       - (86.50532032941677 / (xx+1.0))
       + (24.01409824083091 / (xx+2.0))
       - (1.231739572450155 / (xx+3.0))
       + (0.1208650973866179e-2 / (xx+4.0))
       - (0.5395239384953e-5 / (xx+5.0));

     return (log( 2.5066282746310005 * ser ) - tmp);
#endif

}

static inline double log_factorial( int n )
{
     
     if( n < 0 )
     {
	  return -1;
     }

     // Less than 1 is defined to be 1, taking log(1) == 0
     else if( n <= 1 )
     {
	  return 0;
     }

     else
     {
       return log_gamma_function( n+1.0 );
     }


}

static inline double log_multinomial_beta_function( double *values, int dim )
{

     double logsum = 0.0;
     double inputsum = 0.0;
     int i;
     for( i = 0; i < dim; i++ )
     {
	  double ai = values[i];
	  inputsum += ai;
	  logsum += log_gamma_function( ai );
     }
     logsum -= log_gamma_function( inputsum );
     return logsum;

}

static inline double log_binomial_coefficient( int N, int k )
{
     return log_factorial( N ) - log_factorial( k ) - log_factorial( N - k );
}


#endif
