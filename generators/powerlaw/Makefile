# Makefile for biased power-law stream generator

CC = gcc
INCLUDE = -I./
CFLAGS += $(INCLUDE) -Wall -O3 -std=gnu99 -I ../../include
CFLAGS += $(objinc)
LINK = -lm

powerlaw: powerlaw.o
	$(CC) $(CFLAGS) $< -o $@ $(LINK) 

.o: %.c

clean:
	rm -f powerlaw *.o
