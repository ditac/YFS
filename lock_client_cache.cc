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
pthread_cond_t gReleaseThread_cv;

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
	tprintf("acquire %s for lock %d state %d\n",id.c_str(),lid,lockMap[lid]);
	int r;
  lock_protocol::status ret = lock_protocol::OK;
	do
	{
		ret = lock_protocol::OK;
		while(lockMap[lid] == locked || lockMap[lid] == acquiring)
		{
			pthread_cond_wait(&gClient_cv, &globalClientMutex);
			tprintf("Woke Up %s for lock %d state %d\n",id.c_str(),lid,lockMap[lid]);
		}
		tprintf("Got out %s for lock %d\n",id.c_str(),lockMap[lid]);
		switch(lockMap[lid])
		{
			case free:
				//We just assign lock
				break;
			case none:
				lockMap[lid] = acquiring;
				pthread_mutex_unlock(&globalClientMutex);
				ret = cl->call(lock_protocol::acquire, lid,id, r);
				pthread_mutex_lock(&globalClientMutex);
				break;
			default:
				assert(false);
		}
		if(ret == lock_protocol::RETRY)
		{
			//Error REVOKE
			if(lockMap[lid] == free)
			{
				assert(false);
			}
			if(lockMap[lid] == acquiring)
			{
				continue;
			}
			else if(lockMap[lid] == releasing)
			{
				//Server wants lock back already
				assert(false);
			}
		}
	}while(ret == lock_protocol::RETRY);
	if(lockMap[lid] != releasing)
	{
		lockMap[lid] = locked;
	}
	else
	{
		//We grant lock but keep it in releasing
	}
	tprintf("granted %s for lock %d state %d\n",id.c_str(),lid,lockMap[lid]);
	pthread_mutex_unlock(&globalClientMutex);
  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid) 
{ 
	pthread_mutex_lock(&globalClientMutex);
	tprintf("release %s for lock %d state %d\n",id.c_str(),lid,lockMap[lid]);
	lock_protocol::status ret = lock_protocol::OK;
	int r;
	switch(lockMap[lid])
	{
		case none:
		case free:
		case acquiring:
			assert(false);
			break;
		case locked:
			lockMap[lid] = free;
			break;
		case releasing:
			pthread_mutex_unlock(&globalClientMutex);
			ret = cl->call(lock_protocol::release, lid,id, r);
			pthread_mutex_lock(&globalClientMutex);
			lockMap[lid] = none;
			break;
	}
	pthread_cond_broadcast(&gClient_cv);
	pthread_mutex_unlock(&globalClientMutex);
	return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &r)
{
	pthread_mutex_lock(&globalClientMutex);
	tprintf("revoke %s for lock %d state %d\n",id.c_str(),lid,lockMap[lid]);
  int ret = rlock_protocol::OK;
	switch(lockMap[lid])
	{
		case none:
			//We have already released. Fix server.
			assert(false);
			break;
		case releasing:
			//More people want the lock.. Release fast
			assert(false);
			break;
		case free:
			{
				lockMap[lid] = none;
				pthread_t release_t;		
				lock_client_utility::releaseData *data = new lock_client_utility::releaseData(lid,id,cl,&lockMap);
				pthread_create(&release_t,NULL,lock_client_utility::releaseThread,(void *)data);
			}
			break;
		case acquiring:
		case locked:
			lockMap[lid] = releasing;
			break;
	}
	pthread_mutex_unlock(&globalClientMutex);
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
	pthread_mutex_lock(&globalClientMutex);
	tprintf("retry %s for lock %d state %d\n",id.c_str(),lid,lockMap[lid]);
	int ret = rlock_protocol::OK;
	switch(lockMap[lid])
	{
		case none:
			//TODO Fix this
			break;
		case releasing:
		case free:
		case locked:
			//TODO Fix this
			tprintf("retry %s for lock %d state %d\n",id.c_str(),lid,lockMap[lid]);
			break;
		case acquiring:
			lockMap[lid] = none;
			break;
	}
	pthread_cond_broadcast(&gClient_cv);
	pthread_mutex_unlock(&globalClientMutex);
  return ret;
}

void* 
lock_client_utility::releaseThread(void *in)
{
	lock_client_utility::releaseData* data = (lock_client_utility::releaseData *)in;
	rpcc *cl = data->cl;
	std::string id = data->cltId;
	lock_protocol::lockid_t lid = data->lid;
	tprintf("release RPC%s for lock %d state \n",id.c_str(),lid);
	int r;
	cl->call(lock_protocol::release,lid,id, r);
	pthread_mutex_lock(&globalClientMutex);
	(*(data->lockMap))[lid] = lock_client_cache::none;
	pthread_cond_broadcast(&gClient_cv);
	pthread_mutex_unlock(&globalClientMutex);
	pthread_exit(NULL);	
}

