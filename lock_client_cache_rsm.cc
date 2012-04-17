// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache_rsm.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"

#include "rsm_client.h"
#include "rpc/fifo.h"

pthread_mutex_t gCltMutex;
pthread_mutex_t gRevokeMutex;
pthread_cond_t gAcquire_cv;
pthread_cond_t gRelease_cv;
pthread_cond_t gFree_cv;
pthread_cond_t gRevoke_cv;
pthread_cond_t gRetry_cv;

static void *
releasethread(void *x)
{
  lock_client_cache_rsm *cc = (lock_client_cache_rsm *) x;
  cc->releaser();
  return 0;
}

int lock_client_cache_rsm::last_port = 0;

lock_client_cache_rsm::lock_client_cache_rsm(std::string xdst, 
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
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache_rsm::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache_rsm::retry_handler);
  xid = 1;

	VERIFY(pthread_mutex_init(&gCltMutex, NULL) == 0);
	VERIFY(pthread_mutex_init(&gRevokeMutex, NULL) == 0);
	VERIFY(pthread_cond_init(&gAcquire_cv, NULL) == 0);
	VERIFY(pthread_cond_init(&gRelease_cv, NULL) == 0);
	VERIFY(pthread_cond_init(&gFree_cv, NULL) == 0);
	VERIFY(pthread_cond_init(&gRevoke_cv, NULL) == 0);
	VERIFY(pthread_cond_init(&gRetry_cv, NULL) == 0);

  // You fill this in Step Two, Lab 7
  // - Create rsmc, and use the object to do RPC 
  //   calls instead of the rpcc object of lock_client
  pthread_t th;
  int r = pthread_create(&th, NULL, &releasethread, (void *) this);
  VERIFY (r == 0);
}


void
lock_client_cache_rsm::releaser()
{
	pthread_mutex_lock(&gCltMutex);
	while(true)
	{
		while(releaseQ.size() == 0)
		{
			pthread_cond_wait(&gRevoke_cv, &gCltMutex);
		}
		lock_protocol::lockid_t lid;		
		releaseQ.deq(&lid);
		if(lockMap[lid] != releasing)
		{
			assert(false);
			continue;
		}
		lock_protocol::status ret = lock_protocol::OK;
		pthread_mutex_unlock(&gCltMutex);
		int r;
		ret = cl->call(lock_protocol::release, lid,id,xid,r);
		pthread_mutex_lock(&gCltMutex);

		if(ret == lock_protocol::OK)
		{
			lockMap[lid] = none;
			lockReleaseMap[lid] = false;
			pthread_cond_broadcast(&gRelease_cv);
			pthread_cond_broadcast(&gAcquire_cv);
		}
	}
	pthread_mutex_unlock(&gCltMutex);
}


lock_protocol::status
lock_client_cache_rsm::acquire(lock_protocol::lockid_t lid)
{
	pthread_mutex_lock(&gCltMutex);

start:
	//tprintf("\n Restart %s with lock state %d \n",id.c_str(),lockMap[lid]);
	switch (lockMap[lid])
	{
		case acquiring:
			while(lockMap[lid] == acquiring)
			{
				pthread_cond_wait(&gAcquire_cv, &gCltMutex);
			}	
			goto start;
			break;
		case releasing:
			while(lockMap[lid] == releasing)
			{
				pthread_cond_wait(&gRelease_cv, &gCltMutex);
			}
			goto start;
			break;
		case none:
			{
				lockMap[lid] = acquiring;
				lock_protocol::status ret = callAcquire(lid);
				while(ret == lock_protocol::RETRY)
				{
					struct timespec timeToWait;
					struct timeval now;
					gettimeofday(&now,NULL);
					timeToWait.tv_sec = now.tv_sec;
					timeToWait.tv_nsec = now.tv_usec*1000;
					timeToWait.tv_sec += 1;
					pthread_cond_timedwait(&gRetry_cv, &gCltMutex,&timeToWait);
					ret = callAcquire(lid);
				}
				if(ret == lock_protocol::OK)
				{
					lockMap[lid] = free;
					xid++;
					pthread_cond_broadcast(&gAcquire_cv);
				}
				else
				{
					//RPC Failed At the server. Retry
					lockMap[lid] = none;
				}
			}
			goto start;
			break;
		case free:
			lockMap[lid] = locked;
			break;
		case locked:
			while(lockMap[lid] == locked)
			{
				pthread_cond_wait(&gFree_cv, &gCltMutex);
			}
			goto start;
			break;
	}
	pthread_mutex_unlock(&gCltMutex);
	return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache_rsm::callAcquire(lock_protocol::lockid_t lid) 
{
	pthread_mutex_unlock(&gCltMutex);
	tprintf("Acquire Called %s\n",id.c_str());
	int r;
	lock_protocol::status ret = cl->call(lock_protocol::acquire, lid,id,xid,r);
	pthread_mutex_lock(&gCltMutex);
	return ret;
}

lock_protocol::status
lock_client_cache_rsm::release(lock_protocol::lockid_t lid)
{
	pthread_mutex_lock(&gCltMutex);
	switch (lockMap[lid])
	{
		case acquiring:
		case releasing:
		case none:
		case free:
			//tprintf("release %s for lock %d state %d\n",id.c_str(),lid,lockMap[lid]);
			assert(false);
			break;
		case locked:
			if(lockReleaseMap[lid])
			{
				lockMap[lid] = releasing;
				pthread_mutex_unlock(&gCltMutex);
				if(lu != NULL)
				{
					lu->dorelease(lid);
				}
				pthread_mutex_lock(&gCltMutex);
				releaseQ.enq(lid);
				pthread_cond_broadcast(&gRevoke_cv);
			}	
			else
			{
				lockMap[lid] = free;
				pthread_cond_broadcast(&gFree_cv);
			}
			break;
	}
	pthread_mutex_unlock(&gCltMutex);
  return lock_protocol::OK;
}


rlock_protocol::status
lock_client_cache_rsm::revoke_handler(lock_protocol::lockid_t lid, 
			          lock_protocol::xid_t xid, int &r)
{
	pthread_mutex_lock(&gCltMutex);
	tprintf("Revoke called \n");
	r=0;
  int ret = rlock_protocol::OK;
	switch (lockMap[lid])
	{
		case free:
			{
				lockMap[lid] = releasing;
				releaseQ.enq(lid);
				pthread_cond_broadcast(&gRevoke_cv);
			}
			break;
		case releasing:
		case none:
			break;
		case acquiring:
		case locked:
			lockReleaseMap[lid] = true;
			break;
	}
	pthread_mutex_unlock(&gCltMutex);
  return ret;
}

rlock_protocol::status
lock_client_cache_rsm::retry_handler(lock_protocol::lockid_t lid, 
			         lock_protocol::xid_t xid, int &)
{
  int ret = rlock_protocol::OK;
  return ret;
}


