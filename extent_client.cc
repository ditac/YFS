// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
	cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
	
	/*
	int val = atoi(dst.c_str());
	std::stringstream ss;//create a stringstream

	ss.str("");
	val++;
	ss << val;
	printf("Server Add %s \n", ss.str().c_str());
	
  make_sockaddr(ss.str().c_str(), &dstsock);
	cl1 = new rpcc(dstsock);
  if (cl1->bind() != 0) {
    printf("extent_client: bind failed\n");
  }

	ss.str("");
	val++;
	ss << val;
	printf("Server Add %s \n", ss.str().c_str());
	
  make_sockaddr(ss.str().c_str(), &dstsock);
	cl2 = new rpcc(dstsock);
  if (cl2->bind() != 0) {
    printf("extent_client: bind failed\n");
  }

	ss.str("");
	val++;
	ss << val;
	printf("Server Add %s \n", ss.str().c_str());
	
  make_sockaddr(ss.str().c_str(), &dstsock);
	cl3 = new rpcc(dstsock);
  if (cl3->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
	*/
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
	//rpcc *clt = getServerForId(eid);
  ret = cl->call(extent_protocol::get, eid, buf);
	while(ret == extent_protocol::ASK_SOMEONE_ELSE)
	{
  	ret = extent_protocol::OK;
		int server;
		cl->call(extent_protocol::getServer,eid,server);
		std::stringstream ss;//create a stringstream
		ss << server;
		sockaddr_in dstsock;
  	make_sockaddr(ss.str().c_str(), &dstsock);
		delete cl;
		cl = new rpcc(dstsock);
		if (cl->bind() != 0) {
			printf("extent_client: bind failed\n");
		}
  	ret = cl->call(extent_protocol::get, eid, buf);
	}
  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
	//rpcc *clt = getServerForId(eid);
  ret = cl->call(extent_protocol::getattr, eid, attr);
	while(ret == extent_protocol::ASK_SOMEONE_ELSE)
	{
  	ret = extent_protocol::OK;
		int server;
		cl->call(extent_protocol::getServer,eid,server);
		std::stringstream ss;//create a stringstream
		ss << server;
		sockaddr_in dstsock;
  	make_sockaddr(ss.str().c_str(), &dstsock);
		delete cl;
		cl = new rpcc(dstsock);
		if (cl->bind() != 0) {
			printf("extent_client: bind failed\n");
		}
  	ret = cl->call(extent_protocol::getattr, eid, attr);
	}
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
	//rpcc *clt = getServerForId(eid);
  ret = cl->call(extent_protocol::put, eid, buf, r);
	while(ret == extent_protocol::ASK_SOMEONE_ELSE)
	{
  	ret = extent_protocol::OK;
		int server;
		cl->call(extent_protocol::getServer,eid,server);
		std::stringstream ss;//create a stringstream
		ss << server;
		sockaddr_in dstsock;
  	make_sockaddr(ss.str().c_str(), &dstsock);
		delete cl;
		cl = new rpcc(dstsock);
		if (cl->bind() != 0) {
			printf("extent_client: bind failed\n");
		}
  	ret = cl->call(extent_protocol::put, eid, buf, r);
	}
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
	//rpcc *clt = getServerForId(eid);
  ret = cl->call(extent_protocol::remove, eid, r);
	while(ret == extent_protocol::ASK_SOMEONE_ELSE)
	{
  	ret = extent_protocol::OK;
		std::string server;
		cl->call(extent_protocol::getServer,eid,server);
		std::stringstream ss;//create a stringstream
		ss << server;
		sockaddr_in dstsock;
  	make_sockaddr(ss.str().c_str(), &dstsock);
		delete cl;
		cl = new rpcc(dstsock);
		if (cl->bind() != 0) {
			printf("extent_client: bind failed\n");
		}
  	ret = cl->call(extent_protocol::remove, eid, r);
	}
  return ret;
}

rpcc*
extent_client::getServerForId(extent_protocol::extentid_t eid)
{
 	int val = eid%4;	
	rpcc *ret;
	switch(val)
	{
		case 0:
			ret = cl;
			break;
		case 1:
			ret = cl1;
			break;
		case 2:
			ret = cl2;
			break;
		case 3:
			ret = cl3;
			break;
	}
	return ret;
}

