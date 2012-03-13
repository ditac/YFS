// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"

namespace lock_server_utility
{
	std::list<lock *> retryList;
	std::list<lock *> revokeList;
}

//------------MUTEXES-----------
pthread_mutex_t glockServerMutex;
pthread_cond_t glockServer_cv;
pthread_cond_t grevokeThread_cv;
pthread_cond_t gretryThread_cv;

lock_server_cache::lock_server_cache()
{
	pthread_t revoke_t;		
	pthread_create(&revoke_t,NULL,lock_server_utility::revokeThread,NULL);

	pthread_t retry_t;		
	pthread_create(&retry_t,NULL,lock_server_utility::retryThread,NULL);
}

void* 
lock_server_utility::revokeThread(void *)
{
	pthread_mutex_lock(&glockServerMutex);
	while(1)
	{
		while(!revokeList.empty())
		{
			lock *l = revokeList.front();
			sockaddr_in dstsock;
			make_sockaddr(l->ownerStr.c_str(), &dstsock);
			rpcc* cl = new rpcc(dstsock);
			if (cl->bind() < 0) {
				printf("lock_server: call bind\n");
			}
			int r;
			cl->call(rlock_protocol::revoke,l->id ,r);
			delete cl;
			retry(l);
			revokeList.pop_front();
		}
		pthread_cond_wait(&grevokeThread_cv, &glockServerMutex);
	}
	pthread_mutex_unlock(&glockServerMutex);
	pthread_exit(NULL);	
}


void* 
lock_server_utility::retryThread(void *)
{
	/*
	pthread_mutex_lock(&glockServerMutex);
	while(1)
	{
		while(!retryList.empty())
		{
			lock *l = retryList.front();
			sockaddr_in dstsock;
			make_sockaddr(l->ownerStr.c_str(), &dstsock);
			rpcc* cl = new rpcc(dstsock);
			if (cl->bind() < 0) {
				printf("lock_server: call bind\n");
			}
			int r;
			cl->call(rlock_protocol::revoke,lockData->id ,r);
			free(cl);	
			retryList.pop_front();
		}
		pthread_cond_wait(&gretryThread_cv, &glockServerMutex);
	}
	pthread_mutex_unlock(&glockServerMutex);
	*/
	pthread_exit(NULL);	
}

int 
lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &r)
{
	pthread_mutex_lock(&glockServerMutex);
	tprintf("acquire\n");
  lock_protocol::status ret = lock_protocol::OK;
	
	if(locks[lid] == NULL)
	{
		locks[lid] = new lock();
	}
	switch(locks[lid]->state)
	{
		tprintf("FAULT\n");
		case lock::locked:
		lock_server_utility::revoke(locks[lid]);
		break;
		case lock::free:
		//Grant Lock
			locks[lid]->state = lock::locked;
			locks[lid]->ownerStr = id;
			locks[lid]->id = lid;
		break;	
	}
  r = nacquire;
	pthread_mutex_unlock(&glockServerMutex);
  return ret;


	/*
	pthread_mutex_lock(&glockServerMutex);
  lock_protocol::status ret = lock_protocol::OK;
	std::map<lock_protocol::lockid_t,lock_Data *>::iterator iter =  locks.find(lid);

	if(iter == locks.end())
		std::cout << "\nAcquire called by "<< id << " FOR Lock ___" << lid << "Currently Held By Nobody" <<"\n";
	else
		std::cout << "\nAcquire called by "<< id << " FOR Lock ___" << lid << "Currently Held By _____" << iter->second->cltId << "\n";
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
		if(lockData->state == lock_Data::locked && lockData->cltId == id)
		{
			std::cout << "\n\n\nHOHOHO \n\n\n";
		}
		if(lockData->state == lock_Data::locked)
		{
			//std::cout << "\n\nWe tried to revoke    " << lid;
			lockData->state = lock_Data::revoking;
			//std::cout << "\nRevoke  "<< lockData->cltId << " FOR Lock ___" << lockData->id << "For Client___" << id <<"\n";
			pthread_t thread;		
			pthread_create(&thread,NULL,retryRequest,(void *) iter->second);
			lockData->waitingClientsList.push_back(id);
			ret = lock_protocol::RETRY;
			//std::cout << "\nRetry sent to  " <<id <<"for " << lid;
			//std::cout << "\nLock held by " << lockData->cltId<<"for " << lid;
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
	//std::cout << "\n\nAcquired " << lid << "by___" << id;
	if(ret == lock_protocol::OK)
	{
		//std::cout << "\nLock " << lid << "granted to___" << id;
	}
//dumpLocks();
  return ret;
	*/
}

void
lock_server_utility::revoke(lock *l)
{
	revokeList.push_back(l);	
	pthread_cond_signal(&grevokeThread_cv);
}

void
lock_server_utility::retry(lock *l)
{
	lock_server_utility::retryList.push_back(l);	
	pthread_cond_signal(&gretryThread_cv);
}
/*
void* 
lock_server_cache::retryRequest(void* cltId)
{
	sockaddr_in dstsock;
	//std::cout << "\nRetry Revoke entry" ;
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
		//std::cout << "\nWe called retry on" << cltStr;
  	cl->call(rlock_protocol::retry,lockData->id ,r);
		std::cout << "\nLock " << lockData->id << "granted to___" << cltStr;
		free(cl);
	}
	pthread_mutex_lock(&glockServerMutex);
	lockData->state = lock_Data::free;
	//std::cout << "\nRetry Revoke success" ;
	pthread_mutex_unlock(&glockServerMutex);
	pthread_exit(NULL);
}
*/

int 			
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{ 
	tprintf("release\n");
  r = nacquire;
  return lock_protocol::OK;

	/*
	//std::cout << "Never called \n\n";
	pthread_mutex_lock(&glockServerMutex);
  lock_protocol::status ret = lock_protocol::OK;
	lock_Data* lockData = locks[lid];
	lockData->state = lock_Data::free;
	pthread_mutex_unlock(&glockServerMutex);
  return ret;
	*/
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

void 
lock_server_cache::dumpLocks()
{
	std::map<lock_protocol::lockid_t,lock *>::iterator iter =  locks.begin();
	std::cout << "\n\nDumping Locks \n\n";
	for(;iter != locks.end();iter++)
	{
		std::cout << "Lock number+++" << iter->first << "held by" << iter->second->ownerStr;
	}
}

