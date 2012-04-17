#ifndef lock_server_cache_rsm_h
#define lock_server_cache_rsm_h

#include <string>

#include "lock_protocol.h"
#include "rpc.h"
#include "rsm_state_transfer.h"
#include "rsm.h"
#include <set>
#include <map>

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
	std::map<std::string,lock_protocol::xid_t> xidMap;
};

class lock_server_cache_rsm : public rsm_state_transfer {
 private:
  int nacquire;
  class rsm *rsm;
 public:
  lock_server_cache_rsm(class rsm *rsm = 0);
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  void revoker();
  void retryer();
  std::string marshal_state();
  void unmarshal_state(std::string state);
  int acquire(lock_protocol::lockid_t, std::string id, 
	      lock_protocol::xid_t, int &);
  int release(lock_protocol::lockid_t, std::string id, lock_protocol::xid_t,
	      int &);
	std::map<lock_protocol::lockid_t,lock *> locks;
	fifo<lock_protocol::lockid_t> revokeQ;
	fifo<lock_protocol::lockid_t> retryQ;
};

#endif
