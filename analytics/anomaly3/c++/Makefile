# Makefile for anomaly detection analytic

CC = g++
INCLUDE = -I./
CFLAGS += $(INCLUDE) -Wall -O3 -I ../../include
CFLAGS += $(objinc)
LINK = -lm

anomaly3: anomaly3.o
	$(CC) $(CFLAGS) $< -o $@ $(LINK)

%.o:%.cpp
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f anomaly3 *.o
