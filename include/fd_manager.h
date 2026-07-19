#pragma once

#include "mutex.h"
#include "utils/noncopyable.h"
#include "utils/singleton.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace azzato {

/**
 * @brief Per-fd runtime state used by the hook layer (socket flag, timeouts).
 */
class FdCtx : public std::enable_shared_from_this<FdCtx>, private Noncopyable {
  public:
	using ptr = std::shared_ptr<FdCtx>;

	explicit FdCtx(int fd);

	~FdCtx();

	bool init();

	bool isInit() const { return _isInit; }

	bool isSocket() const { return _isSocket; }

	bool isClosed() const { return _isClosed; }

	void setUserNonblock(bool value) { _userNonblock = value; }

	bool getUserNonblock() const { return _userNonblock; }

	void setSysNonblock(bool value) { _sysNonblock = value; }

	bool getSysNonblock() const { return _sysNonblock; }

	void setTimeout(int type, uint64_t value);

	uint64_t getTimeout(int type);

  private:
	bool	 _isInit		= false;
	bool	 _isSocket		= false;
	bool	 _sysNonblock	= false;
	bool	 _userNonblock	= false;
	bool	 _isClosed		= false;
	int		 _fd			 = -1;
	uint64_t _recvTimeout	 = static_cast<uint64_t>(-1);
	uint64_t _sendTimeout	 = static_cast<uint64_t>(-1);
};

/**
 * @brief Registry of FdCtx, indexed by fd.
 */
class FdManager {
  public:
	using RWMutexType = RWMutex;

	FdManager();

	FdCtx::ptr get(int fd, bool autoCreate = false);

	void del(int fd);

  private:
	RWMutexType			   _mutex;
	std::vector<FdCtx::ptr> _datas;
};

using FdMgr = Singleton<FdManager>;

}  // namespace azzato
