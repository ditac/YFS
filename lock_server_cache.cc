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
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &)
{
	pthread_mutex_lock(&glockServerMutex);
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
			pthread_t thread;		
			pthread_create(&thread,NULL,retryRequest,(void *) iter->second);
			lockData->state = lock_Data::revoking;
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
			lockData->waitingClientsList.push_back(id);
		}
		/*
		while(lockData->state == lock_Data::locked)
		{
			pthread_cond_wait(&glockServer_cv, &glockServerMutex);
		}
		lockData->state = lock_Data::locked;
		*/
		//If the lock already exists we send RETRY for now. In the future we
		//should add the client to our list and send a retry rpc when the 
		//lock is eventually free. 

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
	while(lockData->waitingClientsList.size() > 0)
	{
		pthread_mutex_lock(&glockServerMutex);
		lockData->state = lock_Data::locked;

		pthread_mutex_unlock(&glockServerMutex);
	}
	free(cl);	
	pthread_exit(NULL);
}

int 			
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
	pthread_mutex_lock(&glockServerMutex);
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

