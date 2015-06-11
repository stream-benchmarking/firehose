#!/usr/local/bin/python

# process results from one or more PHISH output files
# print merged output to stdout as if one file had been output
# remove files

# Syntax: post_phish.py file1 file2 ...

import sys,types,os

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

# emit one line for each value of key

def emit(kv,key):
  if key not in kv: return
  values = kv[key]
  if type(values) != types.ListType: values = [values]
  for value in values:
    print "%s = %s" % (key,value)

# emit one line for summed values of key
    
def add(kv,key):
  if key not in kv: return
  values = kv[key]
  if type(values) != types.ListType: values = [values]
  total = 0
  for value in values: total += int(value)
  print "%s = %s" % (key,total)
  
# main program

files = sys.argv[1:]

txt = ""
for file in files:
  txt += open(file,'r').read()

kv = extract_keyvalue(txt)

emit(kv,"true anomaly")
emit(kv,"false positive")
emit(kv,"false negative")

add(kv,"packets received")
add(kv,"unique keys")
add(kv,"true anomalies")
add(kv,"false positives")
add(kv,"false negatives")
add(kv,"true negatives")

for file in files:
  os.remove(file)
