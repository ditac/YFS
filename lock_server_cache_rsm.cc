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
	tprintf("We are being created");
  pthread_t th;
	rsm->set_state_transfer(this);
  int r = pthread_create(&th, NULL, &revokethread, (void *) this);
  VERIFY (r == 0);
  r = pthread_create(&th, NULL, &retrythread, (void *) this);
  VERIFY (r == 0);
	VERIFY(pthread_mutex_init(&gServerMutex, NULL) == 0);
	VERIFY(pthread_cond_init(&grevoke_cv, NULL) == 0);
	VERIFY(pthread_cond_init(&gretry_cv, NULL) == 0);
	tprintf("Creation succesful");
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
	if(!rsm->amiprimary())
	{
		return rsm_client_protocol::NOTPRIMARY;
	}
	pthread_mutex_lock(&gServerMutex);
  r = nacquire;
	lock_protocol::status ret = lock_protocol::OK;
	
	if(locks[lid] == NULL)
	{
		locks[lid] = new lock();
	}
	tprintf("Acquire Called %s with xid %d\n",id.c_str(),xid);
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
	if(!rsm->amiprimary())
	{
		return rsm_client_protocol::NOTPRIMARY;
	}
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
	tprintf("Marshall Called");
	pthread_mutex_lock(&gServerMutex);
	tprintf("Marshall Lock Acquired");
	marshall rep;
	unsigned size = locks.size();
	rep << size;
	std::map<lock_protocol::lockid_t,lock *>::iterator iter_lock;
	for (iter_lock = locks.begin(); iter_lock != locks.end(); iter_lock++) 
	{
		lock_protocol::lockid_t lid = iter_lock->first;
		lock* l = iter_lock->second;
		rep << lid;
		rep << l->state;
		rep << l->ownerStr;
		size = l->waitList.size();
		rep << size;
		std::set<std::string>::iterator waitIter;

		for(waitIter=l->waitList.begin();waitIter != l->waitList.end(); waitIter++)
		{
			std::string waitingClient = (*waitIter);        
			rep << waitingClient;
		}
		size = l->xidMap.size();
		rep << size;
		std::map<std::string,lock_protocol::xid_t>::iterator xidIter;
		for(xidIter=l->xidMap.begin();xidIter != l->xidMap.end(); xidIter++)
		{
			rep << xidIter->first;  
			rep << xidIter->second;
		}
	}
	tprintf("Marshall finished");
	pthread_mutex_unlock(&gServerMutex);
	return rep.str();
}

void
lock_server_cache_rsm::unmarshal_state(std::string state)
{
	tprintf("Unmarshall called");
	pthread_mutex_lock(&gServerMutex);
	tprintf("Unmarshall Lock Acquired");
	unmarshall rep(state);
	unsigned size;
	rep >> size;

	for (unsigned i=0;i<size;i++) 
	{
		lock_protocol::lockid_t lid ;
		lock* l  = new lock();
		rep >> lid;
		int state;      
		rep >> state;
		l->state = (lock::lock_state)state;
		rep >> l->ownerStr;
		unsigned sizeOfWaitList;
		rep >> sizeOfWaitList;

		for(unsigned j=0;j<sizeOfWaitList;j++)
		{
			std::string waitingClient;      
			rep >> waitingClient;
			l->waitList.insert(waitingClient);
		}
	tprintf("breadcrumb");
		unsigned sizeOfXid;
		rep >> sizeOfXid;
		std::map<std::string,lock_protocol::xid_t>::iterator xidIter;
		std::map<std::string, lock_protocol::xid_t> test;
		l->xidMap = test;
		for(unsigned k=0;k<sizeOfXid;k++)
		{
			std::string xid;
			lock_protocol::xid_t id;
			rep >> xid;     
			rep >> id;
			l->xidMap[xid] = id;
	tprintf("breadcrumb in for");
		}
	tprintf("breadcrumb");
		locks[lid] = l;
	}
	pthread_mutex_unlock(&gServerMutex);

}

lock_protocol::status
lock_server_cache_rsm::stat(lock_protocol::lockid_t lid, int &r)
{
  printf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

