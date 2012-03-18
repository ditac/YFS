// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


pthread_mutex_t glockServerMutex;
pthread_cond_t glockServer_cv;

lock_server_cache::lock_server_cache()
{
	VERIFY(pthread_mutex_init(&glockServerMutex, 0) == 0);
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &r)
{
	pthread_mutex_lock(&glockServerMutex);
	r = nacquire;
  lock_protocol::status ret = lock_protocol::OK;
	std::map<lock_protocol::lockid_t,lock_Data *>::iterator iter =  locks.find(lid);

	if(iter == locks.end())
	{
		lock_Data* lockData = new lock_Data();
		lockData->id = lid;
		lockData->state = lock_Data::locked;
		lockData->cltId = id;
		locks.insert(std::pair<lock_protocol::lockid_t,lock_Data*>(lid,lockData));
	}
	else
	{
		
		lock_Data* lockData = iter->second;
		
		if(lockData->state == lock_Data::locked)
		{
			lockData->state = lock_Data::revoking;
			pthread_t thread;		
			pthread_create(&thread,NULL,retryRequest,(void *) iter->second);
			lockData->waitingClientsList.push_back(id);
			ret = lock_protocol::RETRY;
		}
		else if(lockData->state == lock_Data::free)
		{
			lockData->id = lid;
			lockData->state = lock_Data::locked;
			lockData->cltId = id;
		}
		else
		{
			ret = lock_protocol::RETRY;
			lockData->waitingClientsList.push_back(id);
		}
	}
	pthread_mutex_unlock(&glockServerMutex);
	
  return ret;
}

void* 
lock_server_cache::retryRequest(void* cltId)
{
	sockaddr_in dstsock;
	lock_Data* lockData = (lock_Data*) cltId;
  make_sockaddr(lockData->cltId.c_str(), &dstsock);
  rpcc* cl = new rpcc(dstsock);
  if (cl->bind() < 0) {
    printf("lock_server: call bind\n");
  }
	int r;
  cl->call(rlock_protocol::revoke,lockData->id ,r);
	delete cl;	
	pthread_mutex_lock(&glockServerMutex);
	lockData->state = lock_Data::retrying;
	pthread_mutex_unlock(&glockServerMutex);
	while(lockData->waitingClientsList.size() > 0)
	{
		pthread_mutex_lock(&glockServerMutex);
		std::string cltStr = lockData->waitingClientsList.front();
		lockData->waitingClientsList.pop_front();
		lockData->cltId = cltStr;
		pthread_mutex_unlock(&glockServerMutex);
  	make_sockaddr(cltStr.c_str(), &dstsock);
		cl = new rpcc(dstsock);
		if (cl->bind() < 0) {
    printf("lock_server: call bind\n");
  	}
  	cl->call(rlock_protocol::retry,lockData->id ,r);
		delete cl;
	}
	pthread_mutex_lock(&glockServerMutex);
	lockData->state = lock_Data::free;
	pthread_mutex_unlock(&glockServerMutex);
	pthread_exit(NULL);
}

int 			
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
	pthread_mutex_lock(&glockServerMutex);
	r = nacquire;
  lock_protocol::status ret = lock_protocol::OK;
	lock_Data* lockData = locks[lid];
	lockData->state = lock_Data::free;
	pthread_mutex_unlock(&glockServerMutex);
  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

