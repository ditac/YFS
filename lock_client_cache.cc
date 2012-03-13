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
  rpcs *rlsrpc = new rpcs(0);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlsrpc->port();
  id = host.str();
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
	pthread_mutex_lock(&globalClientMutex);
	std::cout << "Acquire"<<lid<<" Called by" << id << "\n";
  lock_protocol::status ret = lock_protocol::OK;
	int r;
	while(lockMap[lid] != free)
	{
		if(lockMap[lid] == none)
		{
			lockMap[lid] = acquiring;
			pthread_mutex_unlock(&globalClientMutex);
			ret = cl->call(lock_protocol::acquire, lid,id, r);
			pthread_mutex_lock(&globalClientMutex);
			if(ret == lock_protocol::OK)
			{
				lockMap[lid] = free;
				break;
			}
			else if(ret == lock_protocol::RETRY)
			{
				tprintf("\nRetry arrived\n");
				lockMap[lid] = waitingForRetry;
				//Incase retry has arrived and is waiting
				pthread_cond_broadcast(&gClient_cv);
			}
		}
		pthread_cond_wait(&gClient_cv, &globalClientMutex);
	}
	if(lockMap[lid] == free)
	{
		//Got the lock.
		lockMap[lid] = locked;
	}

	pthread_cond_broadcast(&gClient_cv);
	pthread_mutex_unlock(&globalClientMutex);
	//std::cout << "\nWe granted here" << lid;
	std::cout << "Acquired____" << lid <<" by" << id << "\n";
  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid) 
{ 
	pthread_mutex_lock(&globalClientMutex); 
	lock_protocol::status ret = lock_protocol::OK;
	lockMap[lid] = free;
	pthread_cond_broadcast(&gClient_cv);
	pthread_mutex_unlock(&globalClientMutex);
	std::cout << "Setting Free" << lid << "by  "<< id << "\n";
	return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
	std::cout << "Just another revoke __" << lockMap[lid] << "\n";
  int ret = rlock_protocol::OK;
	pthread_mutex_lock(&globalClientMutex);
	while(lockMap[lid] != locked && lockMap[lid] != free)
	{
		std::cout << "REVOKE WHEN LOCK IS NOT WITH US client+++" << id <<"For Lock____"<< lid;
		pthread_cond_wait(&gClient_cv, &globalClientMutex);
	}
	//std::cout << "\n\nRevoke  client++++" << id <<"  " << lockMap[lid];
	if(lockMap[lid] == locked)
	{
		lockMap[lid] = releasing;
	}
	while(lockMap[lid] != free)
	{	
		//std::cout << "Revoke waiting for free__" << lid << "\n";
		pthread_cond_wait(&gClient_cv, &globalClientMutex);
	}
	lockMap[lid] = none;
	pthread_mutex_unlock(&globalClientMutex);
	std::cout << "Revoked success client\n";
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  int ret = rlock_protocol::OK;
	pthread_mutex_lock(&globalClientMutex);
	while(lockMap[lid] != waitingForRetry)
	{
		std::cout << "RETRY WHEN LOCK IS WITH US +++" << lid;
		pthread_cond_wait(&gClient_cv, &globalClientMutex);
	}
	lockMap[lid] = free;
	pthread_cond_broadcast(&gClient_cv);
	//std::cout << "Retry  waiting for free__" << lid << "\n";
	pthread_cond_wait(&gClient_cv, &globalClientMutex);
	while(lockMap[lid] != free)
	{
		pthread_cond_wait(&gClient_cv, &globalClientMutex);
	}
	if(lockMap[lid] == acquiring || lockMap[lid] == releasing)
	{
		std::cout << "\n\nWTF\n\n";
	}
	lockMap[lid] = none;
	pthread_mutex_unlock(&globalClientMutex);
  return ret;
}

