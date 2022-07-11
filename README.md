## FireHose suite of stream processing benchmarks.

FireHose is meant to invoke the imagery of drinking from a firehose of
streaming data.

Copyright (2013) Sandia Corporation.  Under the terms of Contract
DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
certain rights in this software.  This software is distributed under
the Berkeley Software Distribution (BSD) License.

----------------------------------------------------------------------

The main purpose of the FireHose Streaming Benchmarks is to enable
comparison of streaming software and hardware, both *quantitatively*
vis-a-vis the rate at which they can process data, and *qualitatively*
by judging the effort involved to implement and run the benchmarks.

The benchmarks themsleves each have a generator and analytic.  Code
for the generators is provided and is meant to be run as-is.
Reference implementations for the analytics are also provided; users
are free to re-implement them for other streaming frameworks or to
optimize them for their hardware.

The originators of FireHose are Karl Anderson and Steve Plimpton, who
can be contacted at sjplimp@sandia.gov.  The FireHose WWW Site at
https://stream-benchmarking.github.io/firehose/ 
has more information as well as tabulated performance results.

This FireHose distribution includes the following files and
directories:

```
README      this file
LICENSE     the BSD License
analytics   code for the analytics
doc         HTML documentation
generators  code for the generators
tools       auxiliary scripts useful in running the benchmarks
results     output files of correct benchmark answers
```

Point your browser at `doc/README.html` to get started.
