#include "mutex.h"
#include "scheduler.h"
#include "utils/macro.h"

namespace azzato {
Semaphore::Semaphore(uint32_t count) {
	if(sem_init(&_semaphore, 0, count)) {
		throw std::logic_error("sem_init error");
	}
}

Semaphore::~Semaphore() { sem_destroy(&_semaphore); }

void Semaphore::wait() {
	if((sem_wait(&_semaphore))) {
		throw std::logic_error("sem_wait error");
	}
}

void Semaphore::notify() {
	if((sem_post(&_semaphore))) {
		throw std::logic_error("sem_post error");
	}
}

FiberSemaphore::FiberSemaphore(size_t initial_concurrency)
	: _concurrency(initial_concurrency) {}

FiberSemaphore::~FiberSemaphore() { AZZATO_ASSERT(_waiters.empty()); }

bool FiberSemaphore::tryWait() {
	AZZATO_ASSERT(Scheduler::getThis());
	{
		MutexType::Lock lock(_mutex);
		if(_concurrency > 0u) {
			--_concurrency;
			return true;
		}
		return false;
	}
}

void FiberSemaphore::wait() {
	AZZATO_ASSERT(Scheduler::getThis());
	{
		MutexType::Lock lock(_mutex);
		if(_concurrency > 0u) {
			--_concurrency;
			return;
		}
		_waiters.push_back(std::make_pair(Scheduler::getThis(), Fiber::getThis()));
	}
	Fiber::yieldToHold();
}

void FiberSemaphore::notify() {
	MutexType::Lock lock(_mutex);
	if(!_waiters.empty()) {
		auto next = _waiters.front();
		_waiters.pop_front();
		next.first->schedule(next.second);
	} else {
		++_concurrency;
	}
}
}  // namespace azzato
