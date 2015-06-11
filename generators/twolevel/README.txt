Streaming Benchmark 3 - Dual Active Set Generation

Goal of Benchmark:
To have a streaming benchmark that requires two levels of state tracking to test concepts of data shuffling, expiration, correlation and state tracking over time.

Produces:
  Frames of UDP packets that contain a set of key - value pairs for evaluation.  The key is a 64 bit value generated from an active set and the value is a 16 bit value.  Each packet contains 50 key-value pairs.

the output looks like

frame 123
14172362086844324614,1
13233506243690197172,51736
875521361841815481,22188
2862890229108759280,47468
2006761540380frame 43198
12478618135885153415,2283
4132194118627369652,18962


Per line, the first element is the key and the second element is the value.  This key value pair is referred to as the outer key and value.

The outer key-value pairs are generated such that if 5 or more values for a given key are observed, then the first 5 values associated with a key are set to have a specific meaning.  We can number these 5 initial values from V1 to V5.  If we collect 16 bit values V1 through V4, we can use these to generate a secondary 64bit key we'll call Xk.  The 5th value (V5) is a secondary value associated with the Xk key we'll call Xv.  The value of the V5 (Xv) element is always 0 or 1.  This new secondary key (Xk) and value (Xv), called the inner key and value, are intended to be used in a new lookup to determine if each inner key Xk is generating biased values.  The way to determine if Xk is generating biased values is to count the number of instances of each Xk keys until 24 instances are found and then sum up the values Xv for those 24 keys.   If the sum of the Xv values for the first 24 instances of an inner key Xk is less than 5, we have reached a reportable condition where the Xk key needs to be reported and counted. 

The generator currently reports which inner keys have resulted in detectable biased values. 

Please note that not every key generates enough values to perform the operations.  There is no notification whether a key (inner or outer) is still active or not.  The number of active elements can be known by the streaming receiver.  The outer keys can generate less than 5 values (can be ignored eventually) and significantly more than 5 values.  Outer values for a key that are beyond the first 5 are just random values to be ignored.



Parallel Generator:
 details on generating data in parallel have not been worked out in code.
  Ideally a parallel generator would generate overlapping keyspaces for at least the secondary keys.  This parallel generation will likely have to share state so that bias answers can be confirmed.


