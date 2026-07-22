#pragma once

#include "mutex.h"
#include "utils/noncopyable.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <vector>

namespace azzato {

class TimerManager;

/**
 * @brief A single timer entry managed by a TimerManager.
 */
class Timer : public std::enable_shared_from_this<Timer>, private Noncopyable {
	friend class TimerManager;

  public:
	using ptr = std::shared_ptr<Timer>;

	struct Comparator {
		bool operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const;
	};

	Timer(uint64_t ms, std::function<void()> callback, bool recurring, TimerManager* manager);

	Timer(uint64_t next);

	bool cancel();

	bool refresh();

	bool reset(uint64_t ms, bool fromNow);

	uint64_t getNext() const { return _next; }

	bool isRecurring() const { return _recurring; }

	bool isCancelled() const { return _cancelled; }

	std::function<void()> getCallback() const { return _callback; }

  private:
	bool				  _recurring = false;
	uint64_t			  _ms		 = 0;
	uint64_t			  _next		 = 0;
	std::function<void()> _callback;
	TimerManager*		  _manager	 = nullptr;
	bool				  _cancelled = false;
};

/**
 * @brief Min-heap style timer management (ordered by deadline).
 *
 * Subclasses must implement onTimerInsertedAtFront() to wake the event loop
 * when a timer that sorts to the front is inserted.
 */
class TimerManager : private Noncopyable {
	friend class Timer;

  public:
	using RWMutexType = RWMutex;

	TimerManager();

	virtual ~TimerManager();

	Timer::ptr addTimer(uint64_t ms, std::function<void()> callback, bool recurring = false);

	Timer::ptr addConditionTimer(uint64_t			   ms,
								 std::function<void()> callback,
								 std::weak_ptr<void>   weakCondition,
								 bool				   recurring = false);

	/**
	 * @return Milliseconds until the earliest timer fires, or ~0ull if none.
	 */
	uint64_t getNextTimer();

	/**
	 * @brief Collect callbacks of all expired timers (rescheduling recurring ones).
	 */
	void listExpiredCb(std::vector<std::function<void()>>& callbacks);

	bool hasTimer();

  protected:
	virtual void onTimerInsertedAtFront() = 0;

	void addTimer(Timer::ptr timer, RWMutexType::WriteLock& lock);

	bool detectClockRollover(uint64_t nowMs);

  private:
	RWMutexType								_mutex;
	std::set<Timer::ptr, Timer::Comparator> _timers;
	bool									_tickled	  = false;
	uint64_t								_previousTime = 0;
};

}  // namespace azzato
