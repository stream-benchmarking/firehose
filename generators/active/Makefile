# Makefile for active-set stream generator

CC = gcc
INCLUDE = -I./
CFLAGS += $(INCLUDE) -Wall -O3 -std=gnu99 -I ../../include
CFLAGS += $(objinc)
LINK = -lm

active: active.o
	$(CC) $(CFLAGS) $< -o $@ $(LINK) 

.o: %.c

clean:
	rm -f active *.o
