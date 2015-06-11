// MINNOW udp
// read from UDP socket, send datums as strings

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <signal.h>
#include <netinet/in.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include <phish.hpp>

int shutdown(char *);
void options(int, char **);

/* ---------------------------------------------------------------------- */

// UDP port

int port;

// defaults for command-line switches

int nsenders = 1;
int countflag = 0;

// packet stats

int nrecv = 0;
int nshut = 0;
int *shut;
int *count;

/* ---------------------------------------------------------------------- */

int main(int narg, char **args)
{
  phish::init(narg,args);
  phish::output(0);
  phish::check();

  options(narg,args);

  // sender stats and stop flags

  count = new int[nsenders];
  shut = new int[nsenders];
  for (int i = 0; i < nsenders; i++) count[i] = shut[i] = 0;

  // setup UDP port

  const int socket = ::socket(PF_INET6, SOCK_DGRAM, 0);
  if (socket == -1)
    throw std::runtime_error(std::string("socket(): ") + ::strerror(errno));

  struct sockaddr_in6 address;
  ::memset(&address, 0, sizeof(address));
  address.sin6_family = AF_INET6;
  address.sin6_addr = in6addr_any;
  address.sin6_port = htons(port);
  if(-1 == ::bind(socket, reinterpret_cast<sockaddr*>(&address), 
                  sizeof(address)))
    throw std::runtime_error(std::string("bind(): ") + ::strerror(errno));
  
  // 1 megabyte buffer, unless you're a disk vendor

  std::vector<char> buffer(1024*1024); 
  int ipacket;

  // loop on reading packets

  while (true) {

    // read a packet with Nbytes

    const int nbytes = ::recv(socket,&buffer[0],buffer.size()-1,0);
    buffer[nbytes] = '\0';

    // check if STOP packet
    // exit if have received STOP packet from every sender

    if (nbytes < 8) {
      if (shutdown(&buffer[0])) break;
      continue;
    }

    // send packet downstream

    phish::pack(&buffer[0],nbytes);
    phish::send();

    nrecv++;

    // tally stats on packets from each sender
    
    if (countflag) {
      sscanf(&buffer[0],"packet %d",&ipacket);
      count[ipacket % nsenders]++;
    }
  }

  phish::exit();

  // close UDP port and print stats

  ::close(socket);

  if (countflag && nsenders > 1) {
    for (int i = 0; i < nsenders; i++) printf("%d ",count[i]);
    printf("packets received per sender\n");
  }
}

/* ---------------------------------------------------------------------- */

// parse command line
// regular argument = port to read from
// optional arguments:
//   -p = # of generators sending to me, only used to count STOP packets
//   -c = 1 to print stats on packets received from each sender

void options(int narg, char **args)
{
  int op;
  
  while ( (op = getopt(narg,args,"p:c:")) != EOF) {
    switch (op) {
    case 'p':
      nsenders = atoi(optarg);
      break;
    case 'c':
      int countflag = atoi(optarg);
      break;
    }
  }

  if (optind != narg-1) phish::error("Syntax: udp options port");
  if (nsenders <= 0) phish::error("invalid command-line switch");

  // set UDP port

  port = atoi(args[optind]);
}

/* ---------------------------------------------------------------------- */

// process STOP packets
// return 1 if have received STOP packet from every sender
// else return 0 if not ready to STOP

int shutdown(char *buf)
{
  int iwhich = atoi(buf);
  if (shut[iwhich]) return 0;
  shut[iwhich] = 1;
  nshut++;
  if (nshut == nsenders) return 1;
  return 0;
}
