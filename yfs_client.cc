// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  srand ( time(NULL) );	
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
  if(inum & 0x80000000)
    return true;
  return false;
}

bool
yfs_client::isdir(inum inum)
{
  return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;

  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

 release:

  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;

  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

 release:
  return r;
}

int
yfs_client::read(inum inum,std::string &buf, off_t offset,size_t size)
{
	int r = OK;	
  if (ec->get(inum, buf) != extent_protocol::OK) 
	{
		r = IOERR;
	}
	else
	{
		buf = buf.substr(offset,size);	
	}
	return r;
}

int
yfs_client::write(inum inum,const char *buf, off_t offset,size_t size)
{
	int r = OK;	
	std::string prevBuf;
	std::string newBuf;
	ec->get(inum,prevBuf);
	if(offset + size > prevBuf.size())
	{
		if(offset > prevBuf.size())
		{
			newBuf = prevBuf + std::string(offset - prevBuf.size(),'\0') + std::string(buf,size);
		}
		else
		{
			newBuf = prevBuf.substr(0,offset) + std::string(buf,size);
		}
	}
	else
	{
		newBuf = prevBuf.substr(0,offset) + std::string(buf,size) + prevBuf.substr(offset+size);
	}
  if (ec->put(inum, newBuf) != extent_protocol::OK) 
	{
		r = IOERR;
	}
	return r;
}

yfs_client::xxstatus
yfs_client::create(inum pinum,const char *name,inum& inum)
{
	xxstatus r = OK;	
	int randInt = rand();
	unsigned long long randInode = 0x00000000 | randInt;
	inum = 0x80000000 | randInode;
	std::string buf;
	std::string strName(name);
	ec->get(pinum,buf);
	size_t found = buf.find(strName);
	if(found != std::string::npos)
	{
		//We have to handle this case	
		r = EXIST; 
	}
	else
	{
		std::string empty;
		ec->put(inum,empty);
		std::string newEntry = "<" + strName + "," + filename(inum) + ">\n";
		buf.append(newEntry);
		ec->put(pinum,buf);
	}
	return r;
}

int
yfs_client::lookup(inum pinum,const char *name,inum& inum)
{
	int r = OK;	
	std::string buf;
	std::string strName(name);
	ec->get(pinum,buf);
	size_t found = buf.find(strName);
	if(found == std::string::npos)
	{
		r = NOENT;
	}
	else
	{
		size_t start = buf.find(",",found) + 1;
		size_t end = buf.find(">",start);
		std::string strInum = buf.substr(start, end - start);
		inum = n2i(strInum);
	}
	return r;
}

yfs_client::dirmap 
yfs_client::getDirList(inum pinode)
{
	yfs_client::dirmap dirList;
	std::string buf;
	ec->get(pinode,buf);
	
	size_t nameStart = buf.find("<");
	size_t nameEnd = buf.find(",",nameStart);
	size_t numberEnd = buf.find(">",nameEnd + 1);
	while(nameStart != std::string::npos)
	{
		std::string name = buf.substr(nameStart + 1,nameEnd - nameStart -1);
		std::string strInum = buf.substr(nameEnd + 1, numberEnd - nameEnd - 1);
		inum inode= n2i(strInum);
		dirList[name] = inode;
	nameStart = buf.find("<",numberEnd);
	nameEnd = buf.find(",",nameStart);
	numberEnd = buf.find(">",nameEnd + 1);
	}
	return dirList;
}


int 
yfs_client::setSize(inum inum,int size)
{
	int r = OK;	
	std::string prevBuf;
	std::string newBuf;
	ec->get(inum,prevBuf);
	if(size < prevBuf.size())
	{
		newBuf = prevBuf.substr(0,size);
	}
	else
	{
		newBuf = prevBuf + std::string(size - prevBuf.size(),'\0');
	}
	if (ec->put(inum, newBuf) != extent_protocol::OK) 
	{
		r = IOERR;
	}
	return r;
}
