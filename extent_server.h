// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include "extent_protocol.h"

class extent_server {

 public:
  extent_server();

  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);

 private:
	pthread_mutex_t extent_server_m_; // protect reply window et al
	struct fileVal{
		fileVal(std::string inBuf, extent_protocol::attr inAttr)
		{
			buf = inBuf;
			attr = inAttr;
		}
		std::string buf;
		extent_protocol::attr attr;
	};
	std::map<extent_protocol::extentid_t,fileVal> fileList;
	typedef std::map<extent_protocol::extentid_t,fileVal>::iterator fileListIter;
};

#endif 







