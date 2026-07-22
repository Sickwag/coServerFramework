#include "fd_manager.h"
#include "hook.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace azzato {

FdCtx::FdCtx(int fd)
	: _fd(fd) {
	init();
}

FdCtx::~FdCtx() {}

bool FdCtx::init() {
	if(_isInit) {
		return true;
	}
	_recvTimeout = static_cast<uint64_t>(-1);
	_sendTimeout = static_cast<uint64_t>(-1);

	struct stat fd_stat;
	if(-1 == fstat(_fd, &fd_stat)) {
		_isInit	  = false;
		_isSocket = false;
	} else {
		_isInit	  = true;
		_isSocket = S_ISSOCK(fd_stat.st_mode);
	}

	if(_isSocket) {
		int flags = fcntl_f(_fd, F_GETFL, 0);
		if(!(flags & O_NONBLOCK)) {
			fcntl_f(_fd, F_SETFL, flags | O_NONBLOCK);
		}
		_sysNonblock = true;
	} else {
		_sysNonblock = false;
	}

	_userNonblock = false;
	_isClosed	  = false;
	return _isInit;
}

void FdCtx::setTimeout(int type, uint64_t value) {
	if(type == SO_RCVTIMEO) {
		_recvTimeout = value;
	} else {
		_sendTimeout = value;
	}
}

uint64_t FdCtx::getTimeout(int type) {
	if(type == SO_RCVTIMEO) {
		return _recvTimeout;
	}
	return _sendTimeout;
}

FdManager::FdManager() { _datas.resize(64); }

FdCtx::ptr FdManager::get(int fd, bool autoCreate) {
	if(fd == -1) {
		return nullptr;
	}
	RWMutexType::ReadLock lock(_mutex);
	if(static_cast<int>(_datas.size()) <= fd) {
		if(!autoCreate) {
			return nullptr;
		}
	} else {
		if(_datas[fd] || !autoCreate) {
			return _datas[fd];
		}
	}
	lock.unlock();

	RWMutexType::WriteLock lock2(_mutex);
	FdCtx::ptr			   ctx(new FdCtx(fd));
	if(fd >= static_cast<int>(_datas.size())) {
		_datas.resize(static_cast<size_t>(fd * 1.5));
	}
	_datas[fd] = ctx;
	return ctx;
}

void FdManager::del(int fd) {
	RWMutexType::WriteLock lock(_mutex);
	if(static_cast<int>(_datas.size()) <= fd) {
		return;
	}
	_datas[fd].reset();
}

}  // namespace azzato
