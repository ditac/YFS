// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server() {}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
	ScopedLock rwl(&extent_server_m_);
	extent_protocol::attr a;
	a.size = buf.size();
  a.atime = 0;
  a.mtime = (unsigned int) time(NULL);
  a.ctime = (unsigned int) time(NULL);
	fileList.insert(std::pair<extent_protocol::extentid_t,fileVal>(id,fileVal(buf,a)));
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
	ScopedLock rwl(&extent_server_m_);
	extent_protocol::xxstatus retVal = extent_protocol::NOENT;
	fileListIter iter = fileList.find(id);
	if(iter != fileList.end())
	{
		buf = iter->second.buf;
		iter->second.attr.atime = (unsigned int) time(NULL);
		fileList.insert(std::pair<extent_protocol::extentid_t,fileVal>(id,iter->second));
		retVal = extent_protocol::OK;	
	}
  return retVal;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
	ScopedLock rwl(&extent_server_m_);
	extent_protocol::xxstatus retVal = extent_protocol::NOENT;
	fileListIter iter = fileList.find(id);
	if(iter != fileList.end())
	{
		a.size = iter->second.attr.size;
		a.atime = iter->second.attr.atime;
  	a.mtime = iter->second.attr.mtime;
  	a.ctime = iter->second.attr.ctime;
		retVal = extent_protocol::OK;	
	}
  return retVal;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
	ScopedLock rwl(&extent_server_m_);
	extent_protocol::xxstatus retVal = extent_protocol::NOENT;
	fileListIter iter = fileList.find(id);
	if(iter != fileList.end())
	{
		fileList.erase(id);
		retVal = extent_protocol::OK;
	}
  return retVal;
}

