#!/usr/local/bin/python

# bench.py = invoke one or more launch.py commands

import sys,shlex
from launch import launch

date = "18Apr13"

# serial C++ runs

#cmd = "bench/score -bname 1 -gnum 2 -grate 5600000 -gtime 5 -adir c++"
#cmd += " -id %s.bench1.c++" % date
#launch(shlex.split(cmd))

#cmd = "bench/score -bname 2 -gnum 1 -grate 1900000 -gtime 30 -adir c++"
#cmd += " -id %s.bench2.c++" % date
#launch(shlex.split(cmd))

#cmd = "bench/score -bname 3 -gnum 1 -grate 1500000 -gtime 30 -adir c++"
#cmd += " -id %s.bench3.c++" % date
#launch(shlex.split(cmd))

# serial Python runs

#cmd = "bench/score -bname 1 -gnum 1 -grate 450000 -gtime 5 -adir python"
#cmd += " -id %s.bench1.python" % date
#launch(shlex.split(cmd))

#cmd = "bench/score -bname 2 -gnum 1 -grate 140000 -gtime 30 -adir python"
#cmd += " -id %s.bench2.python" % date
#launch(shlex.split(cmd))

# serial PHISH C++ runs

#cmd = "bench/score -bname 1 -gnum 2 -grate 5500000 -gtime 5 -adir phish"
#phcmd = "python /home/sjplimp/phish/bait/bait.py" + \
#    " -v p 2 -v f tmp -b mpi in.anomaly"
#cmd += ' -acmd "%s"' % phcmd
#cmd += ' -apost "python post_phish.py tmp.*"'
#cmd += " -id %s.bench1.phish.serial.c++" % date
#launch(shlex.split(cmd))

#cmd = "bench/score -bname 2 -gnum 1 -grate 1900000 -gtime 30 -adir phish"
#phcmd = "python /home/sjplimp/phish/bait/bait.py" + \
#    " -v f tmp -b mpi in.anomaly"
#cmd += ' -acmd "%s"' % phcmd
#cmd += " -id %s.bench2.phish.serial.c++" % date
#launch(shlex.split(cmd))

# parallel PHISH C++ runs

#cmd = "bench/score -bname 1 -gnum 4 -grate 10000000 -gtime 5 -adir phish"
#phcmd = "python /home/sjplimp/phish/bait/bait.py" + \
#    " -v p 4 -v f tmp -v k 4 -v a 2 -s safe 10000 -b mpi in.parallel"
#cmd += " -adropcount 100000"
#cmd += ' -acmd "%s"' % phcmd
#cmd += ' -apost "python post_phish.py tmp.*"'
#cmd += " -id %s.bench1.phish.parallel.c++" % date
#launch(shlex.split(cmd))

# Note: need to double active set size in C++ analytic to score this correctly
#       else serial run expires keys too quickly when 2 generators are used
#       i.e. cmd += ' -aswitch "-a 262144"'

#cmd = "bench/score -bname 2 -gnum 2 -grate 3400000 -gtime 30 -adir phish"
#phcmd = "python /home/sjplimp/phish/bait/bait.py" + \
#    " -v p 2 -v f tmp -v k 4 -v a 2 -b mpi in.parallel"
#cmd += ' -acmd "%s"' % phcmd
#cmd += ' -apost "python post_phish.py tmp.*"'
#cmd += " -id %s.bench2.phish.parallel.c++" % date
#launch(shlex.split(cmd))

# MP runs

cmd = "bench/score -bname 1 -gnum 5 -grate 12000000 -gtime 5 -adir mp"
mpcmd = 'metaproc "udpgen_in_buf -n 5 -U 55555 | keyadd_initial_custom"'
cmd += " -acmd '%s'" % mpcmd
cmd += ' -apost "python post_mp.py tmp"'
cmd += " -id %s.bench1.mp" % date
launch(shlex.split(cmd))

cmd = "bench/score -bname 2 -gnum 2 -grate 3400000 -gtime 30 -adir mp"
mpcmd = 'metaproc "udpgen_in_buf -n 2 -U 55555 | keyadd_initial_custom"'
cmd += " -acmd '%s'" % mpcmd
cmd += ' -apost "python post_mp.py tmp"'
cmd += " -id %s.bench2.mp" % date
launch(shlex.split(cmd))

cmd = "bench/score -bname 3 -gnum 2 -grate 2900000 -gtime 30 -adir mp"
mpcmd = 'metaproc "udpgen_in_buf -n 2 -U 55555 | keyadd_initial_custom -2"'
cmd += " -acmd '%s'" % mpcmd
cmd += ' -apost "python post_mp.py tmp"'
cmd += " -id %s.bench3.mp" % date
launch(shlex.split(cmd))

#################################################################

# debug test for why bench 2 produces different scoring in parallel PHISH
# even with one generator
# something to do with size of active set in parallel phish/anomaly2.cpp

#cmd = "bench -bname 2 -gnum 2 -grate 1000000 -gcount 1000000"
#cmd += " -adir phish"
#phcmd = "python /home/sjplimp/phish/bait/bait.py" + \
#    " -v p 2 -v f tmp -v k 4 -v a 2 -b mpi in.parallel"
#cmd += ' -acmd "%s"' % phcmd
#cmd += ' -apost "python post_phish.py tmp.*"'
#cmd += " -id %s.bench2.phish.parallel.c++" % date
#launch(shlex.split(cmd))

#cmd = "bench -bname 2 -gnum 2 -grate 1000000 -gcount 1000000"
#cmd += " -id %s.bench2.phish.parallel.c++.nodrop" % date
#launch(shlex.split(cmd))

#cmd = "score -gnum 2"
#cmd += " -id %s.bench2.phish.parallel.c++" % date
#launch(shlex.split(cmd))

#################################################################

# scan of generation rates to check drop percentages

#gmin = 5000000
#gmax = 6000000
#gdelta = 100000
#
#grate = gmin
#while grate <= gmax:
#  cmd = "bench -bname 1 -gnum 2 -gtime 0.5 -adir phish"
#  cmd += " -grate %d -gtimelimit 2.0" % grate
#  phcmd = "python /home/sjplimp/phish/bait/bait.py" + \
#      " -v p 2 -v f tmp -b mpi in.anomaly"
#  cmd += ' -acmd "%s"' % phcmd
#  cmd += ' -apost "python post_phish.py tmp.*"'
#  cmd += " -id bench1.phish.serial.c++.%d" % grate
#  launch(shlex.split(cmd))
#  grate += gdelta

#gmin = 4000000
#gmax = 11000000
#gdelta = 1000000 
#
#grate = gmin
#while grate <= gmax:
#  cmd = "bench -bname 1 -gnum 4 -gtime 0.5 -adir phish"
#  cmd += " -grate %d -gtimelimit 2.0" % grate
#  phcmd = "python /home/sjplimp/phish/bait/bait.py" + \
#      "-v p 4 -v f tmp -v k 4 -v a 2 -s safe 10000 -b mpi in.parallel"
#  cmd += ' -acmd "%s"' % phcmd
#  cmd += ' -apost "python post_phish.py tmp.*"'
#  cmd += " -id bench1.phish.parallel.c++.%d" % grate
#  launch(shlex.split(cmd))
#  grate += gdelta

#gmin = 1600000
#gmax = 2300000
#gdelta = 100000
#
#grate = gmin
#while grate <= gmax:
#  cmd = "bench -bname 2 -gtime 1.0 -adir phish"
#  cmd += " -grate %d -gtimelimit 2.0" % grate
#  phcmd = "python /home/sjplimp/phish/bait/bait.py" + \
#    " -v f tmp in.anomaly"'
#  cmd += ' -acmd "%s"' % phcmd
#  cmd += ' -apost "python post_phish.py tmp.*"'
#  cmd += " -id bench2.phish.serial.c++.%d" % grate
#  launch(shlex.split(cmd))
#  grate += gdelta

#gmin = 3500000
#gmax = 4000000
#gdelta = 100000
#
#grate = gmin
#while grate <= gmax:
#  cmd = "bench -bname 2 -gnum 2 -gtime 1.0 -adir phish"
#  cmd += " -grate %d -gtimelimit 2.0" % grate
#  phcmd = "python /home/sjplimp/phish/bait/bait.py" + \
#      " -v p 2 -v f tmp -v k 4 -v a 2 -b mpi in.parallel"
#  cmd += ' -acmd "%s"' % phcmd
#  cmd += ' -apost "python post_phish.py tmp.*"'
#  cmd += " -id bench2.phish.parallel.c++.%d" % grate
#  launch(shlex.split(cmd))
#  grate += gdelta

#gmin = 12000000
#gmax = 12000000
#gdelta = 100000
#
#grate = gmin
#while grate <= gmax:
#  cmd = "bench -bname 1 -gnum 5 -gtime 5.0 -adir mp"
#  cmd += " -grate %d -gtimelimit 2.0" % grate
#  mpcmd = 'metaproc "udpgen_in_buf -n 5 -U 55555 | keyadd_initial_custom"'
#  cmd += " -acmd '%s'" % mpcmd
#  cmd += ' -apost "python post_mp.py tmp"'
#  cmd += " -id bench1.mp.%d" % grate
#  launch(shlex.split(cmd))
#  grate += gdelta

#gmin = 2000000
#gmax = 10000000
#gdelta = 1000000
#
#grate = gmin
#while grate <= gmax:
#  cmd = "bench -bname 2 -gnum 4 -gtime 0.5 -adir mp"
#  cmd += " -grate %d -gtimelimit 2.0" % grate
#  mpcmd = 'metaproc "udpgen_in_buf -n 4 -U 55555 | keyadd_initial_custom"'
#  cmd += " -acmd '%s'" % mpcmd
#  cmd += ' -apost "python post_mp.py tmp"'
#  cmd += " -id bench2.mp.%d" % grate
#  launch(shlex.split(cmd))
#  grate += gdelta

#gmin = 2000000
#gmax = 10000000
#gdelta = 1000000
#
#grate = gmin
#while grate <= gmax:
#  cmd = "bench -bname 3 -gnum 4 -gtime 0.5 -adir mp"
#  cmd += " -grate %d -gtimelimit 2.0" % grate
#  mpcmd = 'metaproc "udpgen_in_buf -n 4 -U 55555 | keyadd_initial_custom -2"'
#  cmd += " -acmd '%s'" % mpcmd
#  cmd += ' -apost "python post_mp.py tmp"'
#  cmd += " -id bench3.mp.%d" % grate
#  launch(shlex.split(cmd))
#  grate += gdelta

#################################################################

# scan of generator number to check drop percentages

#gmin = 3
#gmax = 8
#
#gnum = gmin
#while gnum <= gmax:
#  cmd = "bench -bname 1 -gnum %d -gtime 0.5 -adir mp" % gnum
#  cmd += " -gtimelimit 2.0"
#  mpcmd = 'metaproc "udpgen_in_buf -n %d -U 55555 | keyadd_initial_custom"' % gnum
#  cmd += " -acmd '%s'" % mpcmd
#  cmd += ' -apost "python post_mp.py tmp"'
#  cmd += " -id bench1.mp.%d" % gnum
#  launch(shlex.split(cmd))
#  gnum += 1

#################################################################

# run with -adrop = 0.0 to estimate max allowed rate

#cmd = "bench -bname 1 -gnum 5 -gtime 0.5 -adir mp -adrop 0.0"
#cmd += " -gtimelimit 2.0"
#mpcmd = 'metaproc "udpgen_in_buf -n 5 -U 55555 | keyadd_initial_custom"'
#cmd += " -acmd '%s'" % mpcmd
#cmd += ' -apost "python post_mp.py tmp"'
#cmd += " -id bench1.mp.estimate"
#launch(shlex.split(cmd))

#cmd = "bench -bname 2 -gnum 2 -gtime 0.5 -adir mp -adrop 0.0"
#cmd += " -gtimelimit 2.0"
#mpcmd = 'metaproc "udpgen_in_buf -n 2 -U 55555 | keyadd_initial_custom"'
#cmd += " -acmd '%s'" % mpcmd
#cmd += ' -apost "python post_mp.py tmp"'
#cmd += " -id bench2.mp.estimate"
#launch(shlex.split(cmd))

#cmd = "bench -bname 3 -gnum 2 -gtime 3.0 -adir mp -adrop 0.0"
#cmd += " -gtimelimit 2.0"
#mpcmd = 'metaproc "udpgen_in_buf -n 2 -U 55555 | keyadd_initial_custom -2"'
#cmd += " -acmd '%s'" % mpcmd
#cmd += ' -apost "python post_mp.py tmp"'
#cmd += " -id bench3.mp.estimate"
#launch(shlex.split(cmd))

#################################################################

# ramp up run times at fixed or max rate to check if drop percentage changes

#times = [0.25,0.5,1.0,2.0,5.0,10.0]
#
#for time in times:
#  cmd = "bench -bname 3 -gtime %g -adir c++" % time
#  cmd += " -grate 1500000 -gtimelimit 2.0"
#  cmd += " -id bench3.c++.%d" % int(time*60)
#  launch(shlex.split(cmd))

#################################################################

# generation rates produced by multiple generators

#for ngen in range(16):
#  cmd = "gen -gname powerlaw -gnum %d -gtime 0.5 -gtimelimit 2.0" % (ngen+1)
#  cmd += " -id gen1.%d" % ngen
#  launch(shlex.split(cmd))

#################################################################

# generation rate as function of bundle size

#for bundle in [1,10,25,40,50,100,200,500,1000]:
#  cmd = "gen -gname twolevel -gtime 0.5 -gtimelimit 2.0"
#  cmd += ' -gswitch 1 "-b %d"' % bundle
#  cmd += " -id gen3.%d" % bundle
#  launch(shlex.split(cmd))

#################################################################

# produce reference C++ results for varying datum counts

#for count in [10000,100000,1000000,10000000]:
#  for bench in [1,2,3]:
#    cmd = "bench -bname %d -gcount %d" % (bench,count)
#    if bench == 2: cmd += " -grate 1900000"
#    if bench == 3: cmd += " -grate 1500000"
#    cmd += " -id bench%d.%d" % (bench,count)
#    launch(shlex.split(cmd))
