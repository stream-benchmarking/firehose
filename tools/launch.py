#!/usr/local/bin/python

# launch.py = run generator, analytic, benchmark with both, or score results
#   manage settings, launch/kill of processes, output

# Syntax: launch.py switches action

#######################################################################
# 
# action:
# 
# gen = run the generator only
# analytic = run the analytic only
# bench = run a benchmark with generator and analytic together
# score  = score a pair of already created output files
# bench/score = perform 2 benchmark runs and score them
#   1st run with specified settings
#   2nd run with -adrop 0.0
# 
# optional switches, all have defaults:
# 
# -id string = string to append to all output file names (def = "launch")
# 
# -gname powerlaw/active/... = which generator to run (def = powerlaw)
# -gcount N = total # of packets for all generators to emit (def = 1000000)
# -grate max/N = aggregate rate (datums/sec) for all generators (def = max)
# -gnum N = # of generators to run (def = 1)
# -gtime N = minutes to run all generators (def = 0)
#   if non-zero, overrides -gcount
#   iterates over short test runs to reset -gcount
# -gtimelimit N = time limit in short iterative -gtime runs (def = 10.0)
# -gswitch N string = command-line switches for Nth generator launch
#     (def = "" for all N)
#   N = 1 to gnum
#   enclose string in quotes if needed so is a single arg
#   note: these switches are added to ones controlled by other options
#   note: -r, -p, -x are controlled by other options
#   note: can be specified multiple times for different N
# -gargs N string = arguments for Nth generator launch (def = "" for all N)
#   N = 1 to gnum
#   enclose string in quotes if needed so is a single arg
#   note: default of "" means 127.0.0.1 (localhost) will be used
#   note: can be specified multiple times for different N
# 
# -aname anomaly1/anomaly2/... = which analytic to run (def = anomaly1)
# -adir c++/python/phish/... = which flavor of analytic to run (def = c++)
# -adrop any/N = target drop rate percentage for analytic (def = any)
#   if not any, overrides -grate setting
#   iterates over short test runs to reset -grate
# -adropcount N = use for -gcount in short iterative -adrop runs (def = 1000000)
# -afrac fraction = drop -grate computed by -adrop by this fraction
#                   to insure no drops, only used if -adrop 0.0 (def = 1.0)
# -aswitch string = command-line switches for analytic launch (def = "")
#   enclose string in quotes if needed so is a single arg
#   note: these switches are added to ones controlled by other options
# -acmd string = string for launching analytic (def = "")
#   enclose string in quotes if needed so is a single arg
#   not needed for -adir c++/python
#     unless you need to change command line settings
#   required for other -adir settings
#     e.g. this script doesn't know how to launch a PHISH job
#   note: script will cd to -adir setting before cmd is invoked
# -apost string = command to invoke to post-process analytic output into
#                 expected format, e.g. to merge multiple output files,
#                 launch.py treats stdout of invoked string as file content
#                 note: will invoke command from within -adir directory
#
# -bname N = which benchmark to run (def = 0)
#   if non-zero, overrides -gname and -aname
# 
# -table no/yes = if scoring, also output table format (def = no)
# -fdir path = FireHose directory (def = ~/firehose)
# -verbose no/yes = minimal or verbose output to screen (def = no)
#    note: not yet implemented
#
#######################################################################

import sys,os,subprocess,shlex,time,types,shutil,string,commands

# hard-wired settings

version = "10 Apr 2013"          # change this when upgrade FireHose

#localhost = "134.253.242.27"          # where generators write to (port 55555)
localhost = "127.0.0.1"          # where generators write to (port 55555)
localhost2 = "127.0.0.1@55554"   # where generator test runs write to
                                 #   in case analytic is already running
portID = "55555"                 # port that analytics read from

gtime_initial = 10000            # initial -gcount when estimating for -gtime

                                 # when using action = bench or bench/score
timebetween = 1.0                # delay in secs between launching
                                 #   analytic and generators

                                 # when using -adrop >= 0.0
iter_drop = 10                   # iterate this many times in binary search
fraction_score = 0.75            # downgrade -grate by this amount
                                 #   for bench/score 2nd run to insure no drops

fplog = None                     # file pointer to logfile

# error message

def error(str=None):
  if not str:
    print "Syntax: launch.py switches action"
    if fplog: print >>fplog,"Syntax: launch.py switches action"
    sys.exit()
  print "ERROR:",str
  if fplog: print >>fplog,"ERROR:",str
  sys.exit()
  
# print str to screen and logfile
# only to logfile if flag = 1
  
def out(str,flag=1):
  if flag: print str
  if fplog: print >>fplog,str

# set default command-line settings

def set_defaults():
  global action
  global id,gname,gcount,grate,gnum,gtime,gtimelimit,gswitch,gargs
  global aname,adir,adrop,adropcount,fraction_bench,aswitch,acmd,apost
  global bname,table,fdir,verbose

  id = "launch"
  action = "none"
  gname = "powerlaw"
  gcount = 1000000
  grate = 0
  gnum = 1
  gtime = 0.0
  gtimelimit = 10.0
  gswitch = [""]
  gargs = [""]
  aname = "anomaly1"
  adir = "c++"
  adrop = -1.0
  adropcount = 1000000
  fraction_bench = 1.0
  aswitch = ""
  acmd = ""
  apost = ""
  bname = 0
  table = 0
  fdir = "~/firehose"
  verbose = 0
  
# parse command-line options

def options(args):
  global action
  global id,gname,gcount,grate,gnum,gtime,gtimelimit,gswitch,gargs
  global aname,adir,adrop,adropcount,fraction_bench,aswitch,acmd,apost
  global bname,table,fdir,verbose

  set_defaults()
  
  gswitchlist = []
  gargslist = []
  
  iarg = 0
  while iarg < len(args):
    if args[iarg][0] != "-":
      if action == "none": action = args[iarg]
      else: error()
      iarg += 1
      continue

    if iarg+2 > len(args): error()
    if args[iarg] == "-id":
      id = args[iarg+1]
    elif args[iarg] == "-gname":
      gname = args[iarg+1]
    elif args[iarg] == "-gcount":
      gcount = int(args[iarg+1])
    elif args[iarg] == "-grate":
      if args[iarg+1] == "any": grate = 0
      else: grate = int(args[iarg+1])
    elif args[iarg] == "-gnum":
      gnum = int(args[iarg+1])
    elif args[iarg] == "-gtime":
      gtime = float(args[iarg+1])
    elif args[iarg] == "-gtimelimit":
      gtimelimit = float(args[iarg+1])
    elif args[iarg] == "-gswitch":
      if iarg+3 > len(args): error()
      gswitchlist.append([args[iarg+1],args[iarg+2]])
      iarg += 1
    elif args[iarg] == "-gargs":
      if iarg+3 > len(args): error()
      gargslist.append([args[iarg+1],args[iarg+2]])
      iarg += 1
    elif args[iarg] == "-aname":
      aname = args[iarg+1]
    elif args[iarg] == "-adir":
      adir = args[iarg+1]
    elif args[iarg] == "-adrop":
      if args[iarg] == "any": adrop = -1.0
      else: adrop = float(args[iarg+1])
    elif args[iarg] == "-adropcount":
      adropcount = int(args[iarg+1])
    elif args[iarg] == "-afrac":
      fraction_bench = float(args[iarg+1])
    elif args[iarg] == "-aswitch":
      aswitch = args[iarg+1]
    elif args[iarg] == "-acmd":
      acmd = args[iarg+1]
    elif args[iarg] == "-apost":
      apost = args[iarg+1]
    elif args[iarg] == "-bname":
      bname = int(args[iarg+1])
      if bname == 0:
        pass
      elif bname == 1:
        gname = "powerlaw"
        aname = "anomaly1"
      elif bname == 2:
        gname = "active"
        aname = "anomaly2"
      elif bname == 3:
        gname = "twolevel"
        aname = "anomaly3"
      else: error("Benchmark %d does not exist" % bname)
    elif args[iarg] == "-table":
      if args[iarg+1] == "no": table = 0
      elif args[iarg+1] == "yes": table = 1
      else: error()
    elif args[iarg] == "-fdir":
      fdir = args[iarg+1]
    elif args[iarg] == "-verbose":
      if args[iarg+1] == "no": verbose = 0
      elif args[iarg+1] == "yes": verbose = 1
      else: error()
    else: error()
    iarg += 2

  if action == "none": error()
  if gcount <= 0: error()
  if grate < 0: error()
  if gnum <= 0: error()
  if gtime < 0: error()
  if bname < 0: error()

  gswitch = gnum*[""]
  gargs = gnum*[""]

  for entry in gswitchlist:
    index = int(entry[0])
    if index <= 0 or index > gnum: error()
    gswitch[index-1] = entry[1]
  for entry in gargslist:
    index = int(entry[0])
    if index <= 0 or index > gnum: error()
    gargs[index-1] = entry[1]

# generate command lines for launching each of N generators
# command line is a function of gcount,grate,gnum,gswitch,gargs
# flag = 1/2 for localhost/localhost2
# return cmds = list of command lines
    
def generator_commands(exe,flag=1):
  switches = []
  for i in xrange(gnum):
    count = gcount/gnum
    if i < gcount % gnum: count += 1
    switch = "-n %d " % count
    if grate: switch += "-r %d " % (grate/gnum)
    if gnum > 1: switch += "-p %d -x %d " % (gnum,i)
    if gswitch[i]: switch += gswitch[i]
    switches.append(switch.strip())

  args = []
  for i in xrange(gnum):
    if flag == 1: arg = localhost
    elif flag == 2: arg = localhost2
    if gargs[i]: arg = gargs[i]
    args.append(arg)

  cmds = []
  for i in xrange(gnum):
    cmd = "%s %s %s" % (exe,switches[i],args[i])
    cmds.append(cmd)

  return cmds

# generate command line for launching analytic
# command line is a function of acmd,gcount,gnum
# return command line
    
def analytic_command(exe):
  if acmd: return acmd
  if adir != "c++" and adir != "python": error("Need an -acmd switch")
  if gnum > 1: switch = "-p %d" % gnum
  else: switch = ""
  if aswitch: switch += " %s" % aswitch
  arg = portID
  if switch: cmd = "%s %s %s" % (exe,switch.strip(),arg)
  else: cmd = "%s %s" % (exe,arg)
  return cmd

# launch the generators, return list of processes

def generator_invoke(cmds):
  p = gnum*[None]
  for i in xrange(gnum):
    args = shlex.split(cmds[i])
    p[i] = subprocess.Popen(args,stdout=subprocess.PIPE)
  return p

# monitor when generators are done, kill them
# if killflag = 1, kill each generator here, after output has been shut down
# else just return output and let caller (action_gen) kill after user prompt

def generator_complete(p,killflag):
  goutputs = []
  for i in xrange(gnum):
    gout = p[i].stdout.read()
    if killflag: p[i].kill()
    goutputs.append(gout)
  return goutputs

# launch analytic, return process

def analytic_invoke(cmd):
  args = shlex.split(cmd)
  return subprocess.Popen(args,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
  
# capture output from analytic, no need to kill it

def analytic_complete(a):
  (aout,aerr) = a.communicate()
  return aout,aerr

# extract key/value pairs from lines of text
# if multiple keys, value = list of values
# return kv dictionary

def extract_keyvalue(txt):
  kv = {}
  lines = txt.split('\n')
  for line in lines:
    index = line.find('=')
    if index < 0: continue
    key = line[:index].strip()
    value = line[index+1:].strip()
    if key not in kv: kv[key] = value
    else:
      oldvalue = kv[key]
      if type(oldvalue) == types.ListType:
        oldvalue.append(value)
        kv[key] = oldvalue
      else: kv[key] = [oldvalue,value]
  return kv

# time the generators
# increase gcount from timecount in factors of 2 until exceed timetest
# reset gcount based on last timing

def generator_time(exe):
  global gcount
  gcount = gtime_initial
  while 1:
    cmds = generator_commands(exe,2)
    processes = generator_invoke(cmds)
    goutputs = generator_complete(processes,1)
    times = []
    for i in xrange(gnum):
      kv = extract_keyvalue(goutputs[i])
      times.append(float(kv["elapsed time (secs)"]))
    maxtime = max(times)
    out("  %d packets = %g secs" % (gcount,maxtime))
    if maxtime > gtimelimit: break
    gcount *= 2
      
  scale = gtime*60 / maxtime
  gcount *= scale
  gcount = int(gcount)
  out("  final packet count = %d" % gcount)

# adjust generator rate to achieve desired drop rate
# iter_drop iterations of binary search between rate = 0 and flat-out
# reset grate to max rate that gives better than desired drop rate

def generator_rate(gexe,aexe):
  global grate,gcount
  gcount_hold = gcount
  gcount = gnum*adropcount

  grate = 0
  gratemin = 0
  gratebest = 0
  iteration = 0
  
  while iteration < iter_drop:
    gcmds = generator_commands(gexe)
    acmd = analytic_command(aexe)
    aprocess = analytic_invoke(acmd)
    time.sleep(timebetween)
    gprocesses = generator_invoke(gcmds)
    aout,aerr = analytic_complete(aprocess)
    aout += aerr
    if adir != "c++" and adir != "python":
      open("tmp",'w').write(aout)
    goutputs = generator_complete(gprocesses,1)
    if apost: aout = commands.getoutput(apost)
    
    if grate == 0:
      for i in xrange(gnum):
        kv = extract_keyvalue(goutputs[i])
        grate += int(float(kv["generation rate (datums/sec)"]))
      gratemax = grate

    drop = drop_compute(goutputs,aout)  
    out("  %d generation rate = %g drop percentage" % (grate,drop))

    if drop <= adrop and grate > gratebest:
      if not iteration:
        gratebest = 0
        break
      gratebest = grate
    
    if drop <= adrop:
      gratemin = grate
      grate = (gratemin+gratemax) / 2
    else:
      gratemax = grate
      grate = (gratemin+gratemax) / 2
    iteration += 1

  grate = gratebest
  gcount = gcount_hold

# compute drop percentage from generator and analytics outputs
  
def drop_compute(goutputs,aout):
  packets = []
  for i in xrange(gnum):
    kv = extract_keyvalue(goutputs[i])
    packets.append(int(kv["packets emitted"]))
  npackets = sum(packets)
  kv = extract_keyvalue(aout)
  nrecv = int(kv["packets received"])
  
  # parallel PHISH can overcount packets by 1 per minnow due to re-bundling
  if nrecv > npackets: nrecv = npackets
  
  drop = 100.0 - 100.0*nrecv/npackets
  return drop

# action = gen

def action_gen():
  gexe = "%s/generators/%s/%s" % (fdir,gname,gname)
  if not os.path.isfile(gexe):
    error("Generator executable does not exist: %s" % gexe)

  if gtime:
    out("Adjusting the generator packet count ...")
    generator_time(gexe)

  gcmds = generator_commands(gexe)

  # perform final run

  out("Generating %d packets ..." % gcount)
  if gnum == 1: out("generator command = %s" % gcmds[0])
  else:
    for i in xrange(gnum):
      out("generator command %d = %s" % (i,gcmds[i]))
  processes = generator_invoke(gcmds)
  goutputs = generator_complete(processes,0)
  
  # extract summary info

  packets = []
  times = []
  rates = []
  for i in xrange(gnum):
    kv = extract_keyvalue(goutputs[i])
    packets.append(int(kv["packets emitted"]))
    times.append(float(kv["elapsed time (secs)"]))
    rates.append(int(float(kv["generation rate (datums/sec)"])))

  # print summary
  
  out("Packets emitted = %d" % sum(packets))
  out("Generation rate = %d" % sum(rates))
  out("Elapsed time = %g" % max(times))

  # write final outputs to files

  for i in xrange(gnum):
    file = "generator.%s" % id
    if gnum > 1: file += ".%d" % i
    open(file,'w').write(goutputs[i])

  # prompt user to kill generators

  raw_input("Hit return to kill generator(s) ...")
  for i in xrange(gnum):
    processes[i].kill()
    
# action = analytic

def action_analytic():
  aexe = "%s/analytics/%s/%s/%s" % (fdir,aname,adir,aname)
  if adir == "python": aexe += ".py"
  if not os.path.isfile(aexe):
    error("Analytic executable does not exist: %s" % aexe)
  if adir == "python": aexe = "python " + aexe
  acmd = analytic_command(aexe)

  currentdir = os.getcwd()
  if adir != "c++" and adir != "python":
    os.chdir("%s/analytics/%s/%s" % (fdir,aname,adir))
    
  out("Invoking analytic ...")
  out("analytic command = %s" % acmd)
  process = analytic_invoke(acmd)
  aout,aerr = analytic_complete(process)
  aout += aerr
  if adir != "c++" and adir != "python":
    open("tmp",'w').write(aout)
  if apost: aout = commands.getoutput(apost)
  
  os.chdir(currentdir)

  # write output to file

  file = "analytic.%s" % id
  open(file,'w').write(aout)

# action = bench
# return drop rate
  
def action_bench(flag):
  global grate
    
  gexe = "%s/generators/%s/%s" % (fdir,gname,gname)
  if not os.path.isfile(gexe):
    error("Generator executable does not exist: %s" % gexe)
  aexe = "%s/analytics/%s/%s/%s" % (fdir,aname,adir,aname)
  if adir == "python": aexe += ".py"
  if adir == "c++" or adir == "python":
    if not os.path.isfile(aexe):
      error("Analytic executable does not exist: %s" % aexe)
  if adir == "python": aexe = "python " + aexe

  currentdir = os.getcwd()
  if adir != "c++" and adir != "python":
    os.chdir("%s/analytics/%s/%s" % (fdir,aname,adir))

  if adrop >= 0.0:
    out("Adjusting the generator rate ...")
    generator_rate(gexe,aexe)
    if flag == 1: grate *= fraction_bench
    elif flag == 2: grate *= fraction_score
    out("  final generation rate = %d" % grate)

  if gtime and flag == 1:
    out("Adjusting the generator packet count ...")
    generator_time(gexe)

  gcmds = generator_commands(gexe)
  acmd = analytic_command(aexe)

  out("Launching analytic ...")
  out("analytic command = %s" % acmd)
  aprocess = analytic_invoke(acmd)
  time.sleep(timebetween)

  out("Generating %d packets ..." % gcount)
  if gnum == 1: out("generator command = %s" % gcmds[0])
  else:
    for i in xrange(gnum):
      out("generator command %d = %s" % (i,gcmds[i]))
  gprocesses = generator_invoke(gcmds)

  aout,aerr = analytic_complete(aprocess)
  aout += aerr
  if adir != "c++" and adir != "python":
    open("tmp",'w').write(aout)
  goutputs = generator_complete(gprocesses,1)
  if apost:
    aout_original = aout
    aout = commands.getoutput(apost)

  os.chdir(currentdir)
  
  # extract summary info
  
  packets = []
  times = []
  rates = []
  for i in xrange(gnum):
    kv = extract_keyvalue(goutputs[i])
    packets.append(int(kv["packets emitted"]))
    times.append(float(kv["elapsed time (secs)"]))
    rates.append(int(float(kv["generation rate (datums/sec)"])))

  kv = extract_keyvalue(aout)
  try:
    nrecv = int(kv["packets received"])
    ta = int(kv["true anomalies"])
    fp = int(kv["false positives"])
    fn = int(kv["false negatives"])
    tn = int(kv["true negatives"])
  except:
    if apost: str = "Failed analytic output:\n" + aout_original + \
          "Failed post-process output:\n" + aout
    else: str = "Failed analytic output:\n" + aout
    error(str)
    
  # print summary
  
  out("Packets emitted = %d" % sum(packets))
  out("Packets received = %d" % nrecv)
  out("Generation rate = %d" % sum(rates))
  out("Percent received = %g" % (100.0*nrecv/sum(packets)))
  out("True anomalies = %d" % ta)
  out("False positives = %d" % fp)
  out("False negatives = %d" % fn)
  out("True negatives = %d" % tn)
  out("Elapsed time = %g" % (max(times)))
  
  # write outputs of generators and analytic to files

  for i in xrange(gnum):
    file = "generator.%s" % id
    if gnum > 1: file += ".%d" % i
    open(file,'w').write(goutputs[i])

  file = "analytic.%s" % id
  open(file,'w').write(aerr + aout)

  return 1.0 - 1.0*nrecv/sum(packets)

# action = score

def action_score():
  out("Scoring benchmark results against no-drop results ...")
  
  file = "analytic.%s" % id
  if not os.path.isfile(file):
    error("File to score does not exist: %s" % file)
  out("file to score = %s" % file)
  txtscore = open(file,'r').read()
  kvscore = extract_keyvalue(txtscore)

  file = "analytic.%s.nodrop" % id
  if not os.path.isfile(file):
    error("File to score against does not exist: %s" % file)
  out("file to score against = %s" % file)
  txtgold = open(file,'r').read()
  kvgold = extract_keyvalue(txtgold)

  # warn if no-drop results not actually zero drop rate
  # goutputs = contents of generator files for no-drop run

  goutputs = []
  for i in xrange(gnum):
    filegold = "generator.%s.nodrop" % id
    if gnum > 1: filegold += ".%d" % i
    goutputs.append(open(filegold,'r').read())
  
  drop = drop_compute(goutputs,txtgold)
  if drop != 0.0: out("WARNING: no-drop results had drop rate = %g" % drop)
  
  # compute score

  if "true anomaly" in kvscore: ta_score = kvscore["true anomaly"]
  else: ta_score = []
  if "false positive" in kvscore: fp_score = kvscore["false positive"]
  else: fp_score = []
  if "false negative" in kvscore: fn_score = kvscore["false negative"]
  else: fn_score = []
  if "true anomaly" in kvgold: ta_gold = kvgold["true anomaly"]
  else: ta_gold = []
  if "false positive" in kvgold: fp_gold = kvgold["false positive"]
  else: fp_gold = []
  if "false negative" in kvgold: fn_gold = kvgold["false negative"]
  else: fn_gold = []

  if type(ta_score) != types.ListType: ta_score = [ta_score]
  if type(fp_score) != types.ListType: fp_score = [fp_score]
  if type(fn_score) != types.ListType: fn_score = [fn_score]
  if type(ta_gold) != types.ListType: ta_gold = [ta_gold]
  if type(fp_gold) != types.ListType: fp_gold = [fp_gold]
  if type(fn_gold) != types.ListType: fn_gold = [fn_gold]
  
  ta_count = 0
  for ta in ta_score:
    if ta not in ta_gold: ta_count += 1
  for ta in ta_gold:
    if ta not in ta_score: ta_count += 1

  fp_count = 0
  for fp in fp_score:
    if fp not in fp_gold: fp_count += 1
  for fp in fp_gold:
    if fp not in fp_score: fp_count += 1
    
  fn_count = 0
  for fn in fn_score:
    if fn not in fn_gold: fn_count += 1
  for fn in fn_gold:
    if fn not in fn_score: fn_count += 1

  ta_score = fp_score = fn_score = tn_score = 0
  if "true anomalies" in kvscore: ta_score = int(kvscore["true anomalies"])
  if "false positives" in kvscore: fp_score = int(kvscore["false positives"])
  if "false negatives" in kvscore: fn_score = int(kvscore["false negatives"])
  if "true negatives" in kvscore: tn_score = int(kvscore["true negatives"])
  ta_gold = fp_gold = fn_gold = tn_gold = 0
  if "true anomalies" in kvgold: ta_gold = int(kvgold["true anomalies"])
  if "false positives" in kvgold: fp_gold = int(kvgold["false positives"])
  if "false negatives" in kvgold: fn_gold = int(kvgold["false negatives"])
  if "true negatives" in kvgold: tn_gold = int(kvgold["true negatives"])

  tn_count = tn_score - tn_gold
  if tn_count < 0: tn_count = -tn_count
    
  score = ta_count + fp_count + fn_count + tn_count
  total = ta_gold + fp_gold + fn_gold + tn_gold
  
  # print summary

  out("Score:")
  out("  event count in correct result = %d" % total)
  out("  true anomalies from both files = %d %d" % (ta_score,ta_gold))
  out("  false positives from both files = %d %d" % (fp_score,fp_gold))
  out("  false negatives from both files = %d %d" % (fn_score,fn_gold))
  out("  true negatives from both files = %d %d" % (tn_score,tn_gold))
  out("  true anomalies not in both files = %d" % ta_count)
  out("  false positives not in both files = %d" % fp_count)
  out("  false negatives not in both files = %d" % fn_count)
  out("  total score = %d" % score)

  # output score in benchmark table format

  if table:
    goutputs = []
    for i in xrange(gnum):
      filescore = "generator.%s" % gname
      if gnum > 1: filescore += ".%d" % i
      if id: filescore += ".%s" % id
      goutputs.append(open(filescore,'r').read())

    packets = []
    rates = []
    times = []
    for i in xrange(gnum):
      kv = extract_keyvalue(goutputs[i])
      packets.append(int(kv["packets emitted"]))
      rates.append(int(float(kv["generation rate (datums/sec)"])))
      times.append(float(kv["elapsed time (secs)"]))

    kv = extract_keyvalue(txtscore)
    nrecv = int(kv["packets received"])
    percent = 100.0*nrecv/sum(packets)
    
    str = "%s : machine : %d : %d : %d : %g : " + \
        "cores : %d : %g : %d : %d : yes : loc : none"
    str = str % (adir,gnum,sum(packets),sum(rates),max(times),
                 nrecv,percent,total,score)
    out("Table format = %s" % str)
  
# action = bench/score

def action_bench_score():
  global id,adrop,adir,acmd,apost
  
  out("Initial benchmark run ...")
  drop = action_bench(1)

  # if no drops and ran serial C++
  # then no need for 2nd run, just copy files to create no-drop results
  
  if drop == 0.0 and adir == "c++":
    out("Second run is copy of first, since no drops in first ...")
    for i in xrange(gnum):
      filescore = "generator.%s" % id
      if gnum > 1: filescore += ".%d" % i
      filegold = "generator.%s.nodrop" % id
      if gnum > 1: filegold += ".%d" % i
      shutil.copyfile(filescore,filegold)
    filescore = "analytic.%s" % id
    filegold = "analytic.%s.nodrop" % id
    shutil.copyfile(filescore,filegold)
  
  # perform a 2nd run with serial C++ version at no-drop rate
  # to create no-drop output files
    
  else:
    out("Second run with c++ version with no drops ...")
    id_hold = id
    adir_hold = adir
    
    id += ".nodrop"
    adir = "c++"
    acmd = ""
    apost = ""
    adrop = 0.0
    action_bench(2)

    id = id_hold
    adir = adir_hold

  # score the original vs no-drop output
    
  action_score()

# parse all args and perform one action

def launch(args):
  global fplog,fdir

  options(args)
  
  fplog = open("log." + id,'w')
  out("FireHose (%s)" % version)
  out("launch command = %s" % (string.join(args)),0)

  fdir = os.path.expanduser(fdir)

  if action == "gen": action_gen()
  elif action == "analytic": action_analytic()
  elif action == "bench": action_bench(1)
  elif action == "score": action_score()
  elif action == "bench/score": action_bench_score()
  else: error()

  fplog.close()
  fplog = None
  
# main program
# invoke launch with args

if __name__ == "__main__":
  launch(sys.argv[1:])
