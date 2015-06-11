# doubly linked list
# datums passed in must be a class instance with prev/next fields

class MyDoubleLinkedList():
  def __init__(self):
    self.reset()

  # reset the list to be empty

  def reset(self):
    self.first = self.last = None
    self.nlist = 0

  # append new datum to end of list

  def append(self,datum):
    if self.last: last.next = datum
    datum.prev = self.last
    datum.next = None
    if not self.first: self.first = datum
    self.last = datum
    self.nlist += 1

  # prepend new datum to beginning of list

  def prepend(self,datum):
    if self.first: self.first.prev = datum
    datum.prev = None
    datum.next = self.first
    if not self.last: self.last = datum
    self.first = datum
    self.nlist += 1

  # insert new datum between prev and next
  # prev and/or next can be NULL

  def insert(self,datum,prev,next):
    if prev: prev.next = datum
    else: self.first = datum
    if next: next.prev = datum
    else: self.last = datum
    datum.prev = prev
    datum.next = next
    self.nlist += 1

  # move existing datum to front of list

  def move2front(self,datum):
    if self.first == datum: return
    datum.prev.next = datum.next
    if datum.next: datum.next.prev = datum.prev
    else: self.last = datum.prev
    datum.prev = None
    datum.next = self.first
    self.first.prev = datum
    self.first = datum

  # remove existing datum from list

  def remove(self,datum):
    if datum.prev: datum.prev.next = datum.next
    else: self.first = datum.next
    if datum.next: datum.next.prev = datum.prev
    else: self.last = datum.prev
    self.nlist -= 1
