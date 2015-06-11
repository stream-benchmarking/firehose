# launch multiple generators via shell script

# Syntax: tcsh launch.sh

# NOTE: have to kill all generator processes after analytic completes

alias gen /home/sjplimp/firehose/generators/powerlaw/powerlaw

#gen -n 1000000 127.0.0.1

#gen -n 1000000 -p 2 -x 0 127.0.0.1 &
#gen -n 1000000 -p 2 -x 1 127.0.0.1 &

#gen -n 1000000 -p 4 -x 0 127.0.0.1 &
#gen -n 1000000 -p 4 -x 1 127.0.0.1 &
#gen -n 1000000 -p 4 -x 2 127.0.0.1 &
#gen -n 1000000 -p 4 -x 3 127.0.0.1 &

gen -n 1000000 -p 8 -x 0 127.0.0.1 &
gen -n 1000000 -p 8 -x 1 127.0.0.1 &
gen -n 1000000 -p 8 -x 2 127.0.0.1 &
gen -n 1000000 -p 8 -x 3 127.0.0.1 &
gen -n 1000000 -p 8 -x 4 127.0.0.1 &
gen -n 1000000 -p 8 -x 5 127.0.0.1 &
gen -n 1000000 -p 8 -x 6 127.0.0.1 &
gen -n 1000000 -p 8 -x 7 127.0.0.1 &

#gen -n 1500000 -r 2500000 -p 4 -x 0 127.0.0.1 &
#gen -n 1500000 -r 2500000 -p 4 -x 1 127.0.0.1 &
#gen -n 1500000 -r 2500000 -p 4 -x 2 127.0.0.1 &
#gen -n 1500000 -r 2500000 -p 4 -x 3 127.0.0.1 &
