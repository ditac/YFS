#include "rpc.h"
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include "extent_server.h"

// Main loop of extent server

int
main(int argc, char *argv[])
{
  int count = 0;

  if(argc != 5){
    fprintf(stderr, "Usage: %s port and id and nextServ\n", argv[0]);
    exit(1);
  }

  setvbuf(stdout, NULL, _IONBF, 0);

  char *count_env = getenv("RPC_COUNT");
  if(count_env != NULL){
    count = atoi(count_env);
  }

	int port = atoi(argv[1]);
	int id = atoi(argv[2]);
	int nextServ = atoi(argv[3]);
	int prevServ = atoi(argv[4]);
  rpcs server(port, count);
	if(port == nextServ)
	{
		nextServ = 0;
		prevServ = 0;
	}

  extent_server ls(port, id,nextServ,prevServ);
  server.reg(extent_protocol::get, &ls, &extent_server::get);
  server.reg(extent_protocol::getattr, &ls, &extent_server::getattr);
  server.reg(extent_protocol::put, &ls, &extent_server::put);
  server.reg(extent_protocol::remove, &ls, &extent_server::remove);
  server.reg(extent_protocol::getServer, &ls, &extent_server::getServer);
  server.reg(extent_protocol::addNext, &ls, &extent_server::addNext);
  server.reg(extent_protocol::addPrev, &ls, &extent_server::addPrev);
  server.reg(extent_protocol::getExtentMid, &ls, &extent_server::getExtentMid);
  server.reg(extent_protocol::getExtentEnd, &ls, &extent_server::getExtentEnd);

  while(1)
    sleep(1000);
}

