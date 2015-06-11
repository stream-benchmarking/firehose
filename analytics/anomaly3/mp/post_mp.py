#!/usr/local/bin/python

# process results from one or more metaproc output files
# print merged output to stdout as if one file had been output
# remove files

# Syntax: post_mp.py file1 file2 ...

import sys,types,os,string

# extract key/value pairs from lines of text
# key = one or more final alphabetic fields
# value = first numeric field preceding the key
# if multiple keys, value = list of values
# return kv dictionary

def extract_keyvalue(txt):
  kv = {}
  lines = txt.split('\n')
  for line in lines:
    words = line.split()
    if not words: continue
    flags = []
    for word in words:
      if word[0] in string.digits: flags.append(1)
      else: flags.append(0)
    ikey = len(words)-1
    while ikey >= 0 and flags[ikey] == 0: ikey -= 1
    ikey += 1
    if ikey == len(words): ikey -= 1
    ivalue = ikey-1
    while ivalue >= 0 and flags[ivalue] == 1: ivalue -= 1
    ivalue += 1
    key = " ".join(words[ikey:])
    value = words[ivalue]
    if key not in kv: kv[key] = value
    else:
      oldvalue = kv[key]
      if type(oldvalue) == types.ListType:
        oldvalue.append(value)
        kv[key] = oldvalue
      else: kv[key] = [oldvalue,value]
  return kv

# emit one line for each value of key
# if key2 is defined, print with key2 instead of key

def emit(kv,key,key2=""):
  if not key2: key2 = key
  if key not in kv: return
  values = kv[key]
  if type(values) != types.ListType: values = [values]
  for value in values:
    print "%s = %s" % (key2,value)
    
# emit one line for summed values of key
# if key2 is defined, print with key2 instead of key
    
def add(kv,key,key2=""):
  if not key2: key2 = key
  if key not in kv: return
  values = kv[key]
  if type(values) != types.ListType: values = [values]
  total = 0
  for value in values: total += int(value)
  print "%s = %s" % (key2,total)
  
# main program

files = sys.argv[1:]

txt = ""
for file in files:
  txt += open(file,'r').read()

kv = extract_keyvalue(txt)

emit(kv,"positive anomaly","true anomaly")
emit(kv,"false anomaly","false positive")
emit(kv,"missed anomaly","false negative")

add(kv,"packets/frames","packets received")
add(kv,"uniq keys","unique keys")
add(kv,"True Positives","true anomalies")
add(kv,"False Positives","false positives")
add(kv,"False Negatives","false negatives")
add(kv,"True Negatives","true negatives")

for file in files:
  os.remove(file)
