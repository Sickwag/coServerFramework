#include "http/session_data.h"
#include "utils/hash_util.h"
#include "utils/util.h"

#include <cstdlib>
#include <ctime>
#include <sstream>

namespace azzato {
namespace http {

SessionData::SessionData(bool autoGen)
	: _lastAccessTime(time(0)) {
	if(autoGen) {
		std::stringstream ss;
		ss << getCurrentUS() << "|" << rand() << "|" << rand() << "|" << rand();
		_id = md5(ss.str());
	}
}

void SessionData::del(const std::string& key) {
	RWMutex::WriteLock lock(_mutex);
	_datas.erase(key);
}

bool SessionData::has(const std::string& key) {
	RWMutex::ReadLock lock(_mutex);
	return _datas.find(key) != _datas.end();
}

void SessionDataManager::add(SessionData::ptr info) {
	RWMutex::WriteLock lock(_mutex);
	_datas[info->getId()] = info;
}

SessionData::ptr SessionDataManager::get(const std::string& id) {
	RWMutex::ReadLock lock(_mutex);
	auto			  it = _datas.find(id);
	if(it != _datas.end()) {
		it->second->setLastAccessTime(time(0));
		return it->second;
	}
	return nullptr;
}

void SessionDataManager::check(int64_t ts) {
	uint64_t				 now = time(0) - ts;
	std::vector<std::string> keys;
	RWMutex::ReadLock		 lock(_mutex);
	for(auto& i : _datas) {
		if(i.second->getLastAccessTime() < now) {
			keys.push_back(i.first);
		}
	}
	lock.unlock();
	for(auto& i : keys) {
		del(i);
	}
}

void SessionDataManager::del(const std::string& id) {
	RWMutex::WriteLock lock(_mutex);
	_datas.erase(id);
}

}  // namespace http
}  // namespace azzato
