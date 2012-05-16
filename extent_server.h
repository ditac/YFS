// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include "extent_protocol.h"

class extent_server {

 public:
  extent_server(int port = 0,int id=0, int next=0, int prev=0);

  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);
  int getServer(extent_protocol::extentid_t id,int &);
	int addNext(int port, extent_protocol::extentid_t start,
			extent_protocol::extentid_t end, std::string &);
	int addPrev(int port, extent_protocol::extentid_t start,
			extent_protocol::extentid_t end, std::string &);
	int getExtentMid(int port,extent_protocol::extentid_t &);
	int getExtentEnd(int port,extent_protocol::extentid_t &);

 private:
	pthread_mutex_t extent_server_m_; // protect reply window et al
	struct fileVal{
		fileVal()
		{

		}
		fileVal(std::string inBuf, extent_protocol::attr inAttr)
		{
			buf = inBuf;
			attr = inAttr;
		} 
		
		std::string buf;
		extent_protocol::attr attr;
	};
	int nextServ;
	int prevServ;
	extent_protocol::extentid_t startId;
	extent_protocol::extentid_t endId;
	int port;
	int id;
	std::map<extent_protocol::extentid_t,fileVal> fileList;
	typedef std::map<extent_protocol::extentid_t,fileVal>::iterator fileListIter;
  bool belongsToMe(extent_protocol::extentid_t id);
	void pingNextServ();
	void pingPrevServ();
};

#endif 

