#pragma once

#include "mutex.h"
#include "scheduler.h"
#include "timer.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace azzato {

/**
 * @brief Epoll-based I/O manager: a Scheduler plus an epoll event loop
 *        and the TimerManager. Fibers can block on fd events without
 *        blocking a thread (used by the hook layer).
 */
class IOManager : public Scheduler, public TimerManager {
  public:
	using ptr		   = std::shared_ptr<IOManager>;
	using RWMutexType = RWMutex;

	enum Event : uint32_t {
		None  = 0x0,
		Read  = 0x1,
		Write = 0x4,
	};

	IOManager(size_t threads = 1, bool useCaller = true, const std::string& name = "iomanager");

	~IOManager();

	int addEvent(int fd, Event event, std::function<void()> callback = nullptr);

	bool delEvent(int fd, Event event);

	bool cancelEvent(int fd, Event event);

	bool cancelAll(int fd);

	static IOManager* getThis();

  protected:
	void tickle() override;

	bool stopping() override;

	void idle() override;

	void onTimerInsertedAtFront() override;

	void contextResize(size_t size);

	bool triggerEvent(int fd, Event event);

  private:
	struct FdContext {
		struct EventContext {
			Scheduler*			  scheduler = nullptr;
			Fiber::ptr			  fiber;
			std::function<void()> callback;
		};

		EventContext& getContext(Event event);

		void resetContext(EventContext& ctx);

		void triggerEvent(Event event);

		int			 fd = 0;
		EventContext read;
		EventContext write;
		uint32_t	 events = 0;
		Mutex		 mutex;
	};

	bool stopping(uint64_t& timeout);

  private:
	int						  _epfd = 0;
	int						  _tickleFds[2];
	std::atomic<size_t>		  _pendingEventCount{0};
	RWMutexType				  _mutex;
	std::vector<FdContext*>   _fdContexts;
};

}  // namespace azzato
