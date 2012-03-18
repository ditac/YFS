// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"

class lock_server {
 public:
			enum lock_states{
					locked=0,
					free
				};
			typedef std::map<lock_protocol::lockid_t,lock_states>::iterator lockIter;
 protected:
  int nacquire;
	std::map<lock_protocol::lockid_t,lock_states> locks;

 public:
  lock_server();
  ~lock_server() {};
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif 







