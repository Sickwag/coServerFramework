#pragma once

#include "mutex.h"
#include "utils/singleton.h"
#include <boost/any.hpp>
#include <unordered_map>

namespace azzato {
namespace http {

class SessionData {
  public:
	typedef std::shared_ptr<SessionData> ptr;
	SessionData(bool auto_gen = false);

	template <class T>
	void setData(const std::string& key, const T& v) {
		azzato::RWMutex::WriteLock lock(_mutex);
		_datas[key] = v;
	}

	template <class T>
	T getData(const std::string& key, const T& def = T()) {
		azzato::RWMutex::ReadLock lock(_mutex);
		auto					  it = _datas.find(key);
		if(it == _datas.end()) {
			return def;
		}
		boost::any v = it->second;
		lock.unlock();
		try {
			return boost::any_cast<T>(v);
		} catch(...) {
		}
		return def;
	}

	void del(const std::string& key);

	bool has(const std::string& key);

	uint64_t getLastAccessTime() const { return _lastAccessTime; }

	void setLastAccessTime(uint64_t v) { _lastAccessTime = v; }

	const std::string& getId() const { return _id; }

	void setId(const std::string& val) { _id = val; }

  private:
	azzato::RWMutex								_mutex;
	std::unordered_map<std::string, boost::any> _datas;
	uint64_t									_lastAccessTime;
	std::string									_id;
};

class SessionDataManager {
  public:
	void			 add(SessionData::ptr info);
	void			 del(const std::string& id);
	SessionData::ptr get(const std::string& id);
	void			 check(int64_t ts = 3600);

  private:
	azzato::RWMutex									  _mutex;
	std::unordered_map<std::string, SessionData::ptr> _datas;
};

typedef azzato::Singleton<SessionDataManager> SessionDataMgr;

}  // namespace http
}  // namespace azzato
