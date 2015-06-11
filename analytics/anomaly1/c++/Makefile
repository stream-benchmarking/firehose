# Makefile for anomaly detection analytic

CC = g++
INCLUDE = -I./
CFLAGS += $(INCLUDE) -Wall -O3 -I ../../include
CFLAGS += $(objinc)
LINK = -lm

anomaly1: anomaly1.o
	$(CC) $(CFLAGS) $< -o $@ $(LINK)

%.o:%.cpp
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f anomaly1 *.o
