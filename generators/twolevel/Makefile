# Makefile for two-level active-set stream generator

CC = gcc
INCLUDE = -I./
CFLAGS += $(INCLUDE) -Wall -O3 -std=gnu99 -I ../../include
CFLAGS += $(objinc)
LINK = -lm

twolevel: twolevel.o
	$(CC) $(CFLAGS) $< -o $@ $(LINK) 

.o: %.c

clean:
	rm -f twolevel *.o
