#pragma once

#include "fiber.h"
#include "mutex.h"
#include "thread.h"
#include "utils/noncopyable.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace azzato {

class Scheduler : private Noncopyable {
  public:
	using ptr		= std::shared_ptr<Scheduler>;
	using MutexType = Mutex;

	struct FiberAndThread {
		Fiber::ptr			  fiber;
		std::function<void()> callback;
		int					  thread = -1;

		void reset() {
			fiber	 = nullptr;
			callback = nullptr;
			thread	 = -1;
		}
	};

	explicit Scheduler(size_t threads = 1, bool useCaller = true, const std::string& name = "scheduler");

	virtual ~Scheduler();

	const std::string& getName() const { return _name; }

	void start();

	void stop();

	void switchTo(int thread = -1);

	std::ostream& dump(std::ostream& os);

	/**
	 * @brief Push a fiber or a callback onto the scheduler queue.
	 *        thread == -1 means "any thread"; otherwise it targets a specific worker.
	 */
	template <typename FiberOrCb>
	void schedule(FiberOrCb&& fc, int thread = -1) {
		bool need_tickle = false;
		{
			MutexType::Lock lock(_mutex);
			need_tickle = scheduleLock(std::forward<FiberOrCb>(fc), thread);
		}
		if(need_tickle) {
			tickle();
		}
	}

	template <typename Iterator>
	void schedule(Iterator begin, Iterator end) {
		bool need_tickle = false;
		{
			MutexType::Lock lock(_mutex);
			for(; begin != end; ++begin) {
				need_tickle |= scheduleLock(*begin, -1);
			}
		}
		if(need_tickle) {
			tickle();
		}
	}

	static Scheduler* getThis();

	static Fiber::ptr getMainFiber();

	static void setMainFiber(Fiber::ptr fiber);

	static void setThis(Scheduler* scheduler);

	static const std::string& getSchedulerName();

	static uint32_t getThreadCount();

	static void setNameOfThread(const std::string& name);

	static void setThisFiber();

  protected:
	virtual void tickle();

	virtual void run();

	virtual void idle();

	virtual bool stopping();

	bool hasIdleThreads() { return _idleThreadCount.load() > 0; }

	void setThreadCount(uint32_t count) { _threadCount = count; }

	void setRoot(bool root) { _root = root; }

  private:
	bool scheduleLock(Fiber::ptr fiber, int thread = -1);

	bool scheduleLock(std::function<void()> callback, int thread = -1);

  private:
	MutexType				  _mutex;
	std::vector<Thread::ptr>  _threads;
	std::vector<int>		  _threadIds;
	std::list<FiberAndThread> _fibers;
	std::string				  _name;
	std::atomic<size_t>		  _activeThreadCount{0};
	std::atomic<size_t>		  _idleThreadCount{0};
	std::atomic<bool>		  _stopping{true};
	bool					  _autoStop	   = false;
	int						  _rootThread  = -1;
	bool					  _useCaller   = false;
	bool					  _root		   = false;
	uint32_t				  _threadCount = 0;
	Fiber::ptr				  _rootFiber;
};

/**
 * @brief RAII helper: switch the current fiber onto a target scheduler,
 *        restoring the caller's scheduler on destruction.
 */
class SchedulerSwitcher : private Noncopyable {
  public:
	explicit SchedulerSwitcher(Scheduler* target);

	~SchedulerSwitcher();

  private:
	Scheduler* _caller;
};

}  // namespace azzato
