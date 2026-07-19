#include "timer.h"
#include "utils/util.h"
#include "utils/macro.h"

namespace azzato {

bool Timer::Comparator::operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const {
	if(!lhs && !rhs) {
		return false;
	}
	if(!lhs) {
		return true;
	}
	if(!rhs) {
		return false;
	}
	if(lhs->_next < rhs->_next) {
		return true;
	}
	if(rhs->_next < lhs->_next) {
		return false;
	}
	return lhs.get() < rhs.get();
}

Timer::Timer(uint64_t ms, std::function<void()> callback, bool recurring, TimerManager* manager)
	: _recurring(recurring)
	, _ms(ms)
	, _callback(std::move(callback))
	, _manager(manager) {
	_next = getCurrentMS() + _ms;
}

Timer::Timer(uint64_t next)
	: _next(next) {}

bool Timer::cancel() {
	TimerManager::RWMutexType::WriteLock lock(_manager->_mutex);
	if(_callback) {
		_callback = nullptr;
		auto it	= _manager->_timers.find(shared_from_this());
		if(it != _manager->_timers.end()) {
			_manager->_timers.erase(it);
		}
		return true;
	}
	return false;
}

bool Timer::refresh() {
	TimerManager::RWMutexType::WriteLock lock(_manager->_mutex);
	if(!_callback) {
		return false;
	}
	auto it = _manager->_timers.find(shared_from_this());
	if(it == _manager->_timers.end()) {
		return false;
	}
	_manager->_timers.erase(it);
	_next = getCurrentMS() + _ms;
	_manager->_timers.insert(shared_from_this());
	return true;
}

bool Timer::reset(uint64_t ms, bool fromNow) {
	if(ms == _ms && !fromNow) {
		return true;
	}
	TimerManager::RWMutexType::WriteLock lock(_manager->_mutex);
	if(!_callback) {
		return false;
	}
	auto it = _manager->_timers.find(shared_from_this());
	if(it == _manager->_timers.end()) {
		return false;
	}
	_manager->_timers.erase(it);
	uint64_t start = 0;
	if(fromNow) {
		start = getCurrentMS();
	} else {
		start = _next - _ms;
	}
	_ms   = ms;
	_next = start + _ms;
	_manager->addTimer(shared_from_this(), lock);
	return true;
}

TimerManager::TimerManager()
	: _previousTime(getCurrentMS()) {}

TimerManager::~TimerManager() {}

Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> callback, bool recurring) {
	Timer::ptr			   timer(new Timer(ms, std::move(callback), recurring, this));
	RWMutexType::WriteLock lock(_mutex);
	addTimer(timer, lock);
	return timer;
}

namespace {
void onTimer(std::weak_ptr<void> weakCondition, std::function<void()> callback) {
	if(weakCondition.lock()) {
		callback();
	}
}
}  // namespace

Timer::ptr TimerManager::addConditionTimer(uint64_t				 ms,
										   std::function<void()> callback,
										   std::weak_ptr<void>	 weakCondition,
										   bool					 recurring) {
	return addTimer(ms, std::bind(&onTimer, weakCondition, std::move(callback)), recurring);
}

uint64_t TimerManager::getNextTimer() {
	RWMutexType::ReadLock lock(_mutex);
	_tickled = false;
	if(_timers.empty()) {
		return ~0ull;
	}
	const Timer::ptr& next	 = *_timers.begin();
	uint64_t		  now_ms = getCurrentMS();
	if(now_ms >= next->_next) {
		return 0;
	}
	return next->_next - now_ms;
}

void TimerManager::listExpiredCb(std::vector<std::function<void()>>& callbacks) {
	uint64_t				now_ms = getCurrentMS();
	std::vector<Timer::ptr> expired;
	{
		RWMutexType::ReadLock lock(_mutex);
		if(_timers.empty()) {
			return;
		}
	}
	RWMutexType::WriteLock lock(_mutex);
	if(_timers.empty()) {
		return;
	}
	bool rollover = detectClockRollover(now_ms);
	if(!rollover && ((*_timers.begin())->_next > now_ms)) {
		return;
	}

	Timer::ptr now_timer(new Timer(now_ms));
	auto	   it	 = rollover ? _timers.end() : _timers.lower_bound(now_timer);
	while(it != _timers.end() && (*it)->_next == now_ms) {
		++it;
	}
	expired.insert(expired.begin(), _timers.begin(), it);
	_timers.erase(_timers.begin(), it);
	callbacks.reserve(expired.size());

	for(auto& timer : expired) {
		callbacks.push_back(timer->_callback);
		if(timer->_recurring) {
			timer->_next = now_ms + timer->_ms;
			_timers.insert(timer);
		} else {
			timer->_callback = nullptr;
		}
	}
}

void TimerManager::addTimer(Timer::ptr timer, RWMutexType::WriteLock& lock) {
	auto it		= _timers.insert(timer).first;
	bool atFront = (it == _timers.begin()) && !_tickled;
	if(atFront) {
		_tickled = true;
	}
	lock.unlock();

	if(atFront) {
		onTimerInsertedAtFront();
	}
}

bool TimerManager::detectClockRollover(uint64_t nowMs) {
	bool rollover = false;
	if(nowMs < _previousTime && nowMs < (_previousTime - 60 * 60 * 1000)) {
		rollover = true;
	}
	_previousTime = nowMs;
	return rollover;
}

bool TimerManager::hasTimer() {
	RWMutexType::ReadLock lock(_mutex);
	return !_timers.empty();
}

}  // namespace azzato
