// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server(int port,int id,int next,int prev) 
{
	VERIFY(pthread_mutex_init(&extent_server_m_, 0) == 0);
	std::string buf;
	extent_protocol::attr a;
	a.size = buf.size();
	a.atime = 0;
	a.mtime = (unsigned int) time(NULL);
	a.ctime = (unsigned int) time(NULL);
	this->port = port;
	this->id = id;
	nextServ = next;
	prevServ = prev;
	printf("Next Server id %d and Prev id %d \n\n", next, prev);
	if(next == 0 && prev == 0)
	{
		startId = 0;
		endId = 4294967295;
		fileList.insert(std::pair<extent_protocol::extentid_t,fileVal>(0x00000001,fileVal(buf,a)));
	}
	pingNextServ();
	pingPrevServ();
	printf("Starting id %llu and Ending id %llu \n\n", startId, endId);
}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
	ScopedLock rwl(&extent_server_m_);
	if(belongsToMe(id))
	{
		printf("Putting %llu and my id %d \n",id, this->id);
		extent_protocol::attr a;
		a.size = buf.size();
		a.atime = 0;
		a.mtime = (unsigned int) time(NULL);
		a.ctime = (unsigned int) time(NULL);
		fileVal val(buf,a);
		fileList[id] = val;
		fileListIter iter = fileList.find(1);
		return extent_protocol::OK;
	}
	else
	{
		return extent_protocol::ASK_SOMEONE_ELSE;
	}
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
	ScopedLock rwl(&extent_server_m_);
	if(belongsToMe(id))
	{
		printf("Getting %llu and my id %d \n",id, this->id);
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
	else
	{
		return extent_protocol::ASK_SOMEONE_ELSE;
	}
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
	ScopedLock rwl(&extent_server_m_);
	if(belongsToMe(id))
	{
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
	else
	{
		return extent_protocol::ASK_SOMEONE_ELSE;
	}
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
	ScopedLock rwl(&extent_server_m_);
	if(belongsToMe(id))
	{
		extent_protocol::xxstatus retVal = extent_protocol::NOENT;
		fileListIter iter = fileList.find(id);
		if(iter != fileList.end())
		{
			fileList.erase(id);
			retVal = extent_protocol::OK;
		}
		return retVal;
	}
	else
	{
		return extent_protocol::ASK_SOMEONE_ELSE;
	}
}

int extent_server::getServer(extent_protocol::extentid_t id, int &ret)
{
	ScopedLock rwl(&extent_server_m_);
	extent_protocol::xxstatus retVal = extent_protocol::OK;
	ret = nextServ; 
	return retVal;
}


bool extent_server::belongsToMe(extent_protocol::extentid_t eid)
{
	if(eid >= startId && eid <= endId )
	{
		return true;
	}
	return false;
}

void extent_server::pingNextServ()
{
	std::stringstream ss;//create a stringstream

	if(nextServ ==  0)
		return;
	ss.str("");
	ss << nextServ;
	
	sockaddr_in dstsock;
  make_sockaddr(ss.str().c_str(), &dstsock);
	rpcc *cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
		return;
  }
	std::string data;
  cl->call(extent_protocol::addNext, port, startId, endId, data);

		delete cl;
}

void extent_server::pingPrevServ()
{
	std::stringstream ss;//create a stringstream

	if(prevServ ==  0)
		return;
	ss.str("");
	ss << prevServ;
	
	sockaddr_in dstsock;
  make_sockaddr(ss.str().c_str(), &dstsock);
	rpcc *cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
		return;
  }
	std::string data;
	extent_protocol::extentid_t mid = 0;
	extent_protocol::extentid_t end = 0;
  cl->call(extent_protocol::getExtentMid, port, mid);
  cl->call(extent_protocol::getExtentEnd, port, end);
	startId = mid;
	endId = end;
  cl->call(extent_protocol::addPrev, port, mid, end,data);
	ss.str(data);
	char buf[512];
	while(ss.getline(buf,512,'['))
	{
		extent_protocol::extentid_t newId = atoi(buf);
		extent_protocol::attr a;
		ss.getline(buf,512,'[');
		std::string fileCont(buf);
		ss.getline(buf,512,'[');
		a.atime = atoi(buf);
		ss.getline(buf,512,'[');
		a.mtime = atoi(buf); 
		ss.getline(buf,512,'[');
		a.ctime = atoi(buf);
		ss.getline(buf,512,'[');
		a.size = atoi(buf);
		fileVal val(buf,a);
		fileList[newId] = val;
	}

	std::cout << "Final Size   " << fileList.size();
	delete cl;
}

int extent_server::addNext(int port, extent_protocol::extentid_t start,
			extent_protocol::extentid_t end, std::string &data)
{
	ScopedLock rwl(&extent_server_m_);
	prevServ = port;
		return extent_protocol::OK;
}

int extent_server::addPrev(int port, extent_protocol::extentid_t start,
			extent_protocol::extentid_t end, std::string &data)
{
	ScopedLock rwl(&extent_server_m_);
	nextServ = port;
	endId = start - 1;
	std::map<extent_protocol::extentid_t,fileVal>::iterator iter;
	std::stringstream ss;//create a stringstream
	for(iter = fileList.begin();iter!= fileList.end();iter++)
	{
		if(iter->first >= start)
		{
		ss << (iter->first);
		ss << '[';
		ss << (iter->second.buf);
		ss << '[';
		ss << (iter->second.attr.atime);
		ss << '[';
		ss << (iter->second.attr.mtime);
		ss << '[';
		ss << (iter->second.attr.ctime);
		ss << '[';
		ss << (iter->second.attr.size);
		ss << '[';
		}
	}
	data = ss.str();

	std::cout << "DATA----";
	std::cout << data;
	return extent_protocol::OK;
}

int extent_server::getExtentMid(int port,extent_protocol::extentid_t &mid)
{
	mid = startId + (endId - startId)/2;	
	return extent_protocol::OK;
}

int extent_server::getExtentEnd(int port,extent_protocol::extentid_t &end)
{
	end = endId;	
	return extent_protocol::OK;
}

