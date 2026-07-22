#pragma once

#include "utils/util.h"
#include <cstdint>
#include <sstream>
#include <string>

namespace azzato {

class CacheStatus {
  public:
	CacheStatus() = default;

	int64_t incGet(int64_t v = 1) { return Atomic::addFetch(_get, v); }

	int64_t incSet(int64_t v = 1) { return Atomic::addFetch(_set, v); }

	int64_t incDel(int64_t v = 1) { return Atomic::addFetch(_del, v); }

	int64_t incTimeout(int64_t v = 1) { return Atomic::addFetch(_timeout, v); }

	int64_t incPrune(int64_t v = 1) { return Atomic::addFetch(_prune, v); }

	int64_t incHit(int64_t v = 1) { return Atomic::addFetch(_hit, v); }

	int64_t decGet(int64_t v = 1) { return Atomic::subFetch(_get, v); }

	int64_t decSet(int64_t v = 1) { return Atomic::subFetch(_set, v); }

	int64_t decDel(int64_t v = 1) { return Atomic::subFetch(_del, v); }

	int64_t decTimeout(int64_t v = 1) { return Atomic::subFetch(_timeout, v); }

	int64_t decPrune(int64_t v = 1) { return Atomic::subFetch(_prune, v); }

	int64_t decHit(int64_t v = 1) { return Atomic::subFetch(_hit, v); }

	int64_t getGet() const { return _get; }

	int64_t getSet() const { return _set; }

	int64_t getDel() const { return _del; }

	int64_t getTimeout() const { return _timeout; }

	int64_t getPrune() const { return _prune; }

	int64_t getHit() const { return _hit; }

	double getHitRate() const { return _get ? (_hit * 1.0 / _get) : 0; }

	void merge(const CacheStatus& other) {
		_get += other._get;
		_set += other._set;
		_del += other._del;
		_timeout += other._timeout;
		_prune += other._prune;
		_hit += other._hit;
	}

	std::string toString() const {
		std::stringstream ss;
		ss << "get=" << _get << " set=" << _set << " del=" << _del << " prune=" << _prune
		   << " timeout=" << _timeout << " hit=" << _hit << " hit_rate=" << (getHitRate() * 100.0) << "%";
		return ss.str();
	}

  private:
	int64_t _get	 = 0;
	int64_t _set	 = 0;
	int64_t _del	 = 0;
	int64_t _timeout = 0;
	int64_t _prune	 = 0;
	int64_t _hit	 = 0;
};

}  // namespace azzato
