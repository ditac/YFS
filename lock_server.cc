// the lock server implementation

#include "pthread.h"
#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

pthread_mutex_t lockMutex;
pthread_cond_t lockId_cv;

lock_server::lock_server():
	nacquire (0)
{
}

	lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
	lock_protocol::status ret = lock_protocol::OK;
	printf("stat request from clt %d\n", clt);
	r = nacquire;
	return ret;
}

	lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
	pthread_mutex_lock(&lockMutex);
	lock_protocol::status ret = lock_protocol::OK;
	lockIter lIter = locks.find(lid);
	if(lIter == locks.end())
	{
		locks.insert(std::pair<lock_protocol::lockid_t,lock_states>(lid,locked));
	}
	else if(lIter->second == free)
	{
		lIter->second = locked;
	}
	else
	{
		while(lIter->second == locked)
		{
			pthread_cond_wait(&lockId_cv, &lockMutex);
		}
		lIter->second = locked;
	}
	pthread_mutex_unlock(&lockMutex);
	return ret;
}

	lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
	pthread_mutex_lock(&lockMutex);
	lock_protocol::status ret = lock_protocol::OK;
	lockIter lIter = locks.find(lid);
	if(lIter != locks.end())
	{
		lIter->second = free;
	}
	pthread_cond_signal(&lockId_cv);
	pthread_mutex_unlock(&lockMutex);
	return ret;
}

