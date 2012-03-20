#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

#include "lock_protocol.h"
#include "lock_client.h"

class yfs_client {
  extent_client *ec;
	lock_client *lc;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;
	typedef std::map<std::string,inum> dirmap;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  static std::string filename(inum);
  static inum n2i(std::string);
 public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);
	int read(inum inum,std::string &buf, off_t offset,size_t size);
	int write(inum inum,const char *buf, off_t offset,size_t size);
	xxstatus create(inum pinum,const char *name, inum& inum);
	int lookup(inum pinum,const char *name,inum& inum);
	dirmap getDirList(inum inode);
	int setSize(inum inum,int size);
	int mkdir(inum pinum, const char *name,inum &inum);
	int unlink(inum pinum, const char *name);

};

class extent_cache
{
};
#endif 
