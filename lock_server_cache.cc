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
	//std::cout << "\n\nAcquire called____" << lid << "by___" << id;
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
			//std::cout << "\n\nWe tried to revoke    " << lid;
			lockData->state = lock_Data::revoking;
			pthread_t thread;		
			pthread_create(&thread,NULL,retryRequest,(void *) iter->second);
			lockData->waitingClientsList.push_back(id);
			ret = lock_protocol::RETRY;
			std::cout << "\nRetry sent to  " <<id <<"for " << lid;
			std::cout << "\nLock held by " << lockData->cltId<<"for " << lid;
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
	//std::cout << "\n\nAcquired " << lid << "by___" << id;
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
	free(cl);	
	pthread_mutex_lock(&glockServerMutex);
	lockData->state = lock_Data::locked;
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
		//std::cout << "\nWe called retry on" << cltStr;
  	cl->call(rlock_protocol::retry,lockData->id ,r);
		free(cl);
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
	//std::cout << "Never called \n\n";
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

