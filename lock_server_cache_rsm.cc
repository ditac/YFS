// the caching lock server implementation

#include "lock_server_cache_rsm.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"

pthread_mutex_t gServerMutex;
pthread_cond_t grevoke_cv;
pthread_cond_t gretry_cv;

static void *
revokethread(void *x)
{
  lock_server_cache_rsm *sc = (lock_server_cache_rsm *) x;
  sc->revoker();
  return 0;
}

static void *
retrythread(void *x)
{
  lock_server_cache_rsm *sc = (lock_server_cache_rsm *) x;
  sc->retryer();
  return 0;
}

lock_server_cache_rsm::lock_server_cache_rsm(class rsm *_rsm) 
  : rsm (_rsm)
{
  pthread_t th;
  int r = pthread_create(&th, NULL, &revokethread, (void *) this);
  VERIFY (r == 0);
  r = pthread_create(&th, NULL, &retrythread, (void *) this);
  VERIFY (r == 0);
	VERIFY(pthread_mutex_init(&gServerMutex, NULL) == 0);
	VERIFY(pthread_cond_init(&grevoke_cv, NULL) == 0);
	VERIFY(pthread_cond_init(&gretry_cv, NULL) == 0);
}

void
lock_server_cache_rsm::revoker()
{
	pthread_mutex_lock(&gServerMutex);
	while(true)
	{
		while(revokeQ.size() == 0)
		{
			struct timespec timeToWait;
			struct timeval now;
			gettimeofday(&now,NULL);
			timeToWait.tv_sec = now.tv_sec + 3;
			timeToWait.tv_nsec = now.tv_usec*1000;
			pthread_cond_timedwait(&grevoke_cv, &gServerMutex,&timeToWait);
		}
		lock_protocol::lockid_t lid;
		revokeQ.deq(&lid);
		lock* l = locks[lid];
		if(l->state == lock::locked)
		{
			sockaddr_in dstsock;
			make_sockaddr(l->ownerStr.c_str(), &dstsock);
			rpcc* cl = new rpcc(dstsock);
			if (cl->bind() < 0) {
				printf("lock_server: call bind\n");
			}
			int r;
			pthread_mutex_unlock(&gServerMutex);
			cl->call(rlock_protocol::revoke,l->id,l->xidMap[l->ownerStr],r);
			pthread_mutex_lock(&gServerMutex);
			delete cl;
		}
	}
	pthread_mutex_unlock(&gServerMutex);
}


void
lock_server_cache_rsm::retryer()
{

  // This method should be a continuous loop, waiting for locks
  // to be released and then sending retry messages to those who
  // are waiting for it.
}


int lock_server_cache_rsm::acquire(lock_protocol::lockid_t lid, std::string id, 
             lock_protocol::xid_t xid, int &r)
{
	pthread_mutex_lock(&gServerMutex);
  r = nacquire;
	lock_protocol::status ret = lock_protocol::OK;
	
	if(locks[lid] == NULL)
	{
		locks[lid] = new lock();
	}
	tprintf("Acquire Called %s \n",id.c_str());
	//I dont know why but this works and I am going to keep it for now.
	std::map<std::string, lock_protocol::xid_t> test;
	test[id] = xid;
	locks[lid]->xidMap = test;
	if( xid < locks[lid]->xidMap[id])
	{
		pthread_mutex_unlock(&gServerMutex);
		return ret;
	}

	switch(locks[lid]->state)
	{
		case lock::locked:
			if(xid > locks[lid]->xidMap[id] && id == locks[lid]->ownerStr)
			{
				assert(false);
			}
			else if(xid == locks[lid]->xidMap[id] && id == locks[lid]->ownerStr)
			{
				//We have received this already.	
				ret = lock_protocol::OK;
			} 
			else
			{
				revokeQ.enq(lid);
				locks[lid]->waitList.insert(id);
				ret = lock_protocol::RETRY;
				//tprintf("We told you to revoke");
				pthread_cond_broadcast(&grevoke_cv);
			}
		break;
		case lock::free:
		//Grant Lock
			locks[lid]->state = lock::locked;
			locks[lid]->ownerStr = id;
			locks[lid]->id = lid;
			locks[lid]->xidMap[id] = xid;
			tprintf("Granted %s \n",id.c_str());
			//tprintf("granted %s for lock %d\n",id.c_str(),lid);
		break;	
	}
	pthread_mutex_unlock(&gServerMutex);
  return ret;
}

int 
lock_server_cache_rsm::release(lock_protocol::lockid_t lid, std::string id, 
         lock_protocol::xid_t xid, int &r)
{
	pthread_mutex_lock(&gServerMutex);
	r = nacquire;
  lock_protocol::status ret = lock_protocol::OK;
	if(xid < locks[lid]->xidMap[id])
	{
		pthread_mutex_unlock(&gServerMutex);
		return ret;
	}
	if(id == locks[lid]->ownerStr)
	{
		if(xid == locks[lid]->xidMap[id])
		{
		//This is ok
		}
		lock* lockData = locks[lid];
		lockData->state = lock::free;
		lockData->ownerStr = "";
		tprintf("Released %s \n",id.c_str());
	}
	//lock_server_utility::released = true;
	//pthread_cond_signal(&gretryThread_cv);
	pthread_mutex_unlock(&gServerMutex);
  return ret;
}

std::string
lock_server_cache_rsm::marshal_state()
{
  std::ostringstream ost;
  std::string r;
  return r;
}

void
lock_server_cache_rsm::unmarshal_state(std::string state)
{
}

lock_protocol::status
lock_server_cache_rsm::stat(lock_protocol::lockid_t lid, int &r)
{
  printf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

