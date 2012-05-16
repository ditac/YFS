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

  if(argc != 3){
    fprintf(stderr, "Usage: %s port and id\n", argv[0]);
    exit(1);
  }

  setvbuf(stdout, NULL, _IONBF, 0);

  char *count_env = getenv("RPC_COUNT");
  if(count_env != NULL){
    count = atoi(count_env);
  }

	int port = atoi(argv[1]);
  rpcs server(port, count);
  extent_server ls(port,0);
  extent_server ls1(port,1);
  extent_server ls2(port,2);
  extent_server ls3(port,3);
  
	port++;
	rpcs server1(port, count);
	port++;
  rpcs server2(port, count);
	port++;
  rpcs server3(port, count);

  server.reg(extent_protocol::get, &ls, &extent_server::get);
  server.reg(extent_protocol::getattr, &ls, &extent_server::getattr);
  server.reg(extent_protocol::put, &ls, &extent_server::put);
  server.reg(extent_protocol::remove, &ls, &extent_server::remove);
  server.reg(extent_protocol::getServer, &ls, &extent_server::getServer);

	/*
  server1.reg(extent_protocol::get, &ls1, &extent_server::get);
  server1.reg(extent_protocol::getattr, &ls1, &extent_server::getattr);
  server1.reg(extent_protocol::put, &ls1, &extent_server::put);
  server1.reg(extent_protocol::remove, &ls1, &extent_server::remove);
  server1.reg(extent_protocol::getServer, &ls1, &extent_server::getServer);

	server2.reg(extent_protocol::get, &ls2, &extent_server::get);
  server2.reg(extent_protocol::getattr, &ls2, &extent_server::getattr);
  server2.reg(extent_protocol::put, &ls2, &extent_server::put);
  server2.reg(extent_protocol::remove, &ls2, &extent_server::remove);
  server2.reg(extent_protocol::getServer, &ls2, &extent_server::getServer);

	server3.reg(extent_protocol::get, &ls3, &extent_server::get);
  server3.reg(extent_protocol::getattr, &ls3, &extent_server::getattr);
  server3.reg(extent_protocol::put, &ls3, &extent_server::put);
  server3.reg(extent_protocol::remove, &ls3, &extent_server::remove);
  server3.reg(extent_protocol::getServer, &ls3, &extent_server::getServer);
	*/

  while(1)
    sleep(1000);
}

