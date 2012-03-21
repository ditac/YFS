#pragma once
#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"
#include <set>

class lock
{
	public:
	enum lock_state
	{
		free,
		locked
	};
	lock_protocol::lockid_t id;
	lock_state state;
	std::string ownerStr;
	std::set<std::string> waitList;
};

class lock_server_cache 
{
 private:
  int nacquire;
 public:
	std::map<lock_protocol::lockid_t,lock *> locks;
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
	void dumpLocks();
};

namespace lock_server_utility
{
	void revoke(lock *l);
	void retry(lock *l);
	void* revokeThread(void *);
	void* retryThread(void *);
}

#endif
