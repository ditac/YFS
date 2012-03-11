// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"


int lock_client_cache::last_port = 0;

pthread_mutex_t globalClientMutex;
pthread_cond_t gClient_cv;

lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
	pthread_mutex_lock(&globalClientMutex);
  lock_protocol::status ret = lock_protocol::OK;
	int r;
	//For starters lets just create a local lock and give it to the client
	//We can think about the server later. 
  //ret = cl->call(lock_protocol::acquire, cl->id(), lid);
	lockMapIter lIter = lockMap.find(lid);
	if(lIter == lockMap.end())
	{
		//Create a new lock and add it to the map!!
  	ret = cl->call(lock_protocol::acquire, lid,id, r);
		if(ret == lock_protocol::OK)
		{
			lockMap.insert(std::pair<lock_protocol::lockid_t,lock_client_state>(lid,locked));
		}
		else if(ret == lock_protocol::RETRY)
		{
			lockMap.insert(std::pair<lock_protocol::lockid_t,lock_client_state>(lid,acquiring));
			while(lockMap[lid] != free)
			{
				pthread_cond_wait(&gClient_cv, &globalClientMutex);
			}
			lockMap[lid] = locked;
		}
	}
	else 
	{
		while(lockMap[lid] != free && lockMap[lid] != releasing)
		{
			pthread_cond_wait(&gClient_cv, &globalClientMutex);
		}
		lockMap[lid] = locked;
	}
	pthread_mutex_unlock(&globalClientMutex);
  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
	pthread_mutex_lock(&globalClientMutex);
	lock_protocol::status ret = lock_protocol::OK;
	//TODO Handle acquiring and releasing from the server cache.
	lockMap[lid] = free;
	pthread_cond_signal(&gClient_cv);
	pthread_mutex_unlock(&globalClientMutex);
	return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  int ret = rlock_protocol::OK;
	pthread_mutex_lock(&globalClientMutex);
	lockMap[lid] = releasing;
	while(lockMap[lid] != free)
	{
			pthread_cond_wait(&gClient_cv, &globalClientMutex);
	}
	lockMap[lid] = none;
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  int ret = rlock_protocol::OK;
  return ret;
}

