#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"

class lock_Data
{
	public:
	enum lock_state
	{
		free,
		locked,
		revoking,
		retrying
	};
	lock_protocol::lockid_t id;
	lock_state state;
	std::string cltId;
	std::list<std::string> waitingClientsList;
};

class lock_server_cache 
{
 private:
  int nacquire;
 public:
	std::map<lock_protocol::lockid_t,lock_Data *> locks;
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
	static void* retryRequest(void* cltId);
};

#endif
