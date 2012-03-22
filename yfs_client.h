#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

#include "lock_protocol.h"
#include "lock_client_cache.h"

class extent_cache;
class yfs_client {
  extent_cache *ec;
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

class extent_cache : public lock_release_user {
	public:
	extent_cache(std::string);
	int put(extent_protocol::extentid_t id, std::string);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id);

	private:
  extent_client *ec;
	class fileVal{
		public:
		std::string buf;
		extent_protocol::attr attr;
		bool dirty;
		bool remove;

		fileVal()
		{
			dirty = false;
			remove = false;
		}
		fileVal(std::string inBuf, extent_protocol::attr inAttr)
		{
			buf = inBuf;
			attr = inAttr;
			dirty = false;
			remove = false;
		} 
	};
  virtual void dorelease(lock_protocol::lockid_t);
	private:
	std::map<extent_protocol::extentid_t,fileVal> fileList;
	typedef std::map<extent_protocol::extentid_t,fileVal>::iterator fileListIter;
};

#endif 
