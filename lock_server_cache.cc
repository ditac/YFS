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
	std::map<lock_protocol::lockid_t, lock *> retryList;
	std::list<lock *> revokeList;
	bool released;
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
			if(l->ownerStr.empty())
			{
				//Revoke Sent already
				revokeList.pop_front();
				continue;
			}
			make_sockaddr(l->ownerStr.c_str(), &dstsock);
			tprintf("revoke %s\n",l->ownerStr.c_str());
			//Reset Owner
			l->ownerStr = "";
			rpcc* cl = new rpcc(dstsock);
			if (cl->bind() < 0) {
				printf("lock_server: call bind\n");
			}
			int r;
			revokeList.pop_front();
			pthread_mutex_unlock(&glockServerMutex);
			cl->call(rlock_protocol::revoke,l->id ,r);
			pthread_mutex_lock(&glockServerMutex);
			tprintf("revoke Completed \n");
			delete cl;
		}
		pthread_cond_wait(&grevokeThread_cv, &glockServerMutex);
	}
	pthread_mutex_unlock(&glockServerMutex);
	pthread_exit(NULL);	
}


void* 
lock_server_utility::retryThread(void *)
{
	pthread_mutex_lock(&glockServerMutex);
	while(1)
	{
		released = true;
		while(released)
		{
			std::list<std::pair<std::string,lock_protocol::lockid_t> > cltList;
			std::map<lock_protocol::lockid_t,lock *>::iterator iter = retryList.begin();
			released = false;
			for(;iter!=retryList.end();iter++)
			{
				lock *l = iter->second;
				std::set<std::string>::iterator waitListIter = l->waitList.begin();
				for(;waitListIter!=l->waitList.end();waitListIter++)
				{
					std::string cltStr = *waitListIter;
					cltList.push_back(std::pair<std::string,lock_protocol::lockid_t>(cltStr,iter->first));
					l->waitList.erase(waitListIter);
				}
				retryList.erase(iter);
			}
			tprintf("retry %d\n",cltList.size());
			pthread_mutex_unlock(&glockServerMutex);
			std::list<std::pair<std::string,lock_protocol::lockid_t> >::iterator cltIter = cltList.begin();
			for(;cltIter!= cltList.end();cltIter++)
			{
				sockaddr_in dstsock;
				make_sockaddr((*cltIter).first.c_str(), &dstsock);
				rpcc* cl = new rpcc(dstsock);
				if (cl->bind() < 0) {
					printf("lock_server: call bind\n");
				}
				int r;
				cl->call(rlock_protocol::retry,(*cltIter).second ,r);
				delete cl;
			}
			pthread_mutex_lock(&glockServerMutex);
		}

		tprintf("exit retry \n");
		pthread_cond_wait(&gretryThread_cv, &glockServerMutex);
	}
	pthread_mutex_unlock(&glockServerMutex);
	pthread_exit(NULL);	
}

int 
lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &r)
{
	pthread_mutex_lock(&glockServerMutex);
	tprintf("acquire %s for lock %d\n",id.c_str(),lid);
  lock_protocol::status ret = lock_protocol::OK;
	
	if(locks[lid] == NULL)
	{
		locks[lid] = new lock();
	}
	switch(locks[lid]->state)
	{
		case lock::locked:
			if(id == locks[lid]->ownerStr)
			{
				assert(false);
			}
		lock_server_utility::revoke(locks[lid]);
		locks[lid]->waitList.insert(id);
		lock_server_utility::retry(locks[lid]);
		ret = lock_protocol::RETRY;
		break;
		case lock::free:
		//Grant Lock
			locks[lid]->state = lock::locked;
			locks[lid]->ownerStr = id;
			locks[lid]->id = lid;
			tprintf("granted %s for lock %d\n",id.c_str(),lid);
		break;	
	}
  r = nacquire;
	pthread_mutex_unlock(&glockServerMutex);
  return ret;
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
	lock_server_utility::retryList[l->id] = l;	
}

int 			
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{ 
	tprintf("release %s\n",id.c_str());
  r = nacquire;
	pthread_mutex_lock(&glockServerMutex);
  lock_protocol::status ret = lock_protocol::OK;
	lock* lockData = locks[lid];
	lockData->state = lock::free;
	lock_server_utility::released = true;
	pthread_cond_signal(&gretryThread_cv);
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

