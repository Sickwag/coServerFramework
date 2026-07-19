#pragma once

#include <atomic>
#include <list>
#include <memory>
#include <semaphore.h>
#include <thread>

#include "fiber.h"
#include "utils/noncopyable.h"

namespace azzato {
class Semaphore : Noncopyable {
  public:
	Semaphore(uint32_t count = 0);
	~Semaphore();
	void wait();
	void notify();

  private:
	sem_t _semaphore;
};

template <typename T>
struct ScopedLockImpl {
	ScopedLockImpl(T& mutex)
		: _mutex(mutex) {
		_mutex.lock();
	}

	~ScopedLockImpl() { _mutex.unlock(); }

	void lock() { _mutex.lock(); };

	void unlock() { _mutex.unlock(); };

  private:
	T& _mutex;
};

template <typename T>
class ReadScopedLockImpl {
  public:
	ReadScopedLockImpl(T& mutex)
		: _mutex(mutex) {
		_mutex.rdlock();
	}

	~ReadScopedLockImpl() { _mutex.unlock(); }

	void lock() { _mutex.rdlock(); }

	void unlock() { _mutex.unlock(); }

  private:
	T& _mutex;
};

template <typename T>
class WriteScopedLockImpl {
  public:
	WriteScopedLockImpl(T& mutex)
		: _mutex(mutex) {
		_mutex.wrlock();
	}

	~WriteScopedLockImpl() { _mutex.unlock(); }

	void lock() { _mutex.wrlock(); }

	void unlock() { _mutex.unlock(); }

  private:
	T& _mutex;
};

class Mutex : Noncopyable {
  public:
	using Lock = ScopedLockImpl<Mutex>;

	Mutex() { pthread_mutex_init(&_mutex, nullptr); }

	~Mutex() { pthread_mutex_destroy(&_mutex); }

	void lock() { pthread_mutex_lock(&_mutex); }

	void unlock() { pthread_mutex_unlock(&_mutex); }

  private:
	pthread_mutex_t _mutex;
};

/// @brief Null Lock, for debug
class NullMutex : Noncopyable {
  public:
	using Lock	= ScopedLockImpl<NullMutex>;

	NullMutex() = default;

	void lock() {}

	void unlock() {}
};

class RWMutex : Noncopyable {
  public:
	using ReadLock	= ReadScopedLockImpl<RWMutex>;
	using WriteLock = WriteScopedLockImpl<RWMutex>;

	RWMutex() { pthread_rwlock_init(&_lock, nullptr); }

	~RWMutex() { pthread_rwlock_destroy(&_lock); }

	void rdlock() { pthread_rwlock_rdlock(&_lock); }

	void wrlock() { pthread_rwlock_wrlock(&_lock); }

	void unlock() { pthread_rwlock_unlock(&_lock); }

  private:
	pthread_rwlock_t _lock;
};

class NullRWMutex : Noncopyable {
  public:
	using ReadLock	= ReadScopedLockImpl<NullRWMutex>;
	using WriteLock = WriteScopedLockImpl<NullRWMutex>;

	NullRWMutex()	= default;

	void rdlock() {}

	void wrlock() {}

	void unlock() {}
};

class CASLock : Noncopyable {
  public:
	using Lock = ScopedLockImpl<CASLock>;

	CASLock() { _mutex.clear(); }

	~CASLock() {}

	void lock() {
		while(std::atomic_flag_test_and_set_explicit(&_mutex, std::memory_order_acquire))
			;
	}

	void unlock() { std::atomic_flag_clear_explicit(&_mutex, std::memory_order_release); }

  private:
	volatile std::atomic_flag _mutex;
};

class Spinlock : Noncopyable {
  public:
	using Lock = ScopedLockImpl<Spinlock>;

	Spinlock() { pthread_spin_init(&_mutex, 0); }

	~Spinlock() { pthread_spin_destroy(&_mutex); }

	void lock() { pthread_spin_lock(&_mutex); }

	void unlock() { pthread_spin_unlock(&_mutex); }

  private:
	pthread_spinlock_t _mutex;
};

class Scheduler;
class Fiber;

class FiberSemaphore : Noncopyable {
  public:
	using MutexType = Spinlock;

	FiberSemaphore(size_t initial_concurrency = 0);
	~FiberSemaphore();

	bool tryWait();
	void wait();
	void notify();

	size_t getConcurrency() const { return _concurrency; }

	void reset() { _concurrency = 0; }

  private:
	MutexType									 _mutex;
	std::list<std::pair<Scheduler*, Fiber::ptr>> _waiters;
	size_t										 _concurrency;
};
}  // namespace azzato
