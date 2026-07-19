#include "scheduler.h"
#include "utils/macro.h"

namespace azzato {

namespace {
thread_local Scheduler*	t_scheduler	  = nullptr;
thread_local Fiber::ptr t_scheduler_fiber = nullptr;
}  // namespace

Scheduler::Scheduler(size_t threads, bool useCaller, const std::string& name)
	: _name(name) {
	AZZATO_ASSERT(threads > 0);

	if(useCaller) {
		Fiber::getThis();  // ensure the caller thread's main fiber exists
		--threads;

		AZZATO_ASSERT(t_scheduler == nullptr);
		setThis(this);

		_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
		setNameOfThread(_name);

		t_scheduler_fiber = _rootFiber;
		_rootThread		   = static_cast<int>(Thread::getCurrentThreadId());
		_threadIds.push_back(_rootThread);
	} else {
		_rootThread = -1;
	}
	_threadCount = static_cast<uint32_t>(threads);
}

Scheduler::~Scheduler() {
	AZZATO_ASSERT(_stopping.load());
	if(getThis() == this) {
		setThis(nullptr);
	}
}

Scheduler* Scheduler::getThis() { return t_scheduler; }

Fiber::ptr Scheduler::getMainFiber() { return t_scheduler_fiber; }

void Scheduler::setMainFiber(Fiber::ptr fiber) { t_scheduler_fiber = std::move(fiber); }

void Scheduler::setThis(Scheduler* scheduler) { t_scheduler = scheduler; }

void Scheduler::start() {
	MutexType::Lock lock(_mutex);
	if(!_stopping.load()) {
		return;
	}
	_stopping.store(false);
	AZZATO_ASSERT(_threads.empty());

	_threads.resize(_threadCount);
	for(size_t i = 0; i < _threadCount; ++i) {
		_threads[i].reset(new Thread(std::bind(&Scheduler::run, this), _name + "_" + std::to_string(i)));
	}
}

void Scheduler::stop() {
	_autoStop = true;
	if(_rootFiber && _threadCount == 0
	   && (_rootFiber->getState() == Fiber::FiberState::Terminate || _rootFiber->getState() == Fiber::FiberState::Init)) {
		AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << this << " stopped";
		_stopping.store(true);

		if(stopping()) {
			return;
		}
	}

	if(_rootThread != -1) {
		AZZATO_ASSERT(getThis() == this);
	} else {
		AZZATO_ASSERT(getThis() != this);
	}

	_stopping.store(true);
	for(size_t i = 0; i < _threadCount; ++i) {
		tickle();
	}

	if(_rootFiber) {
		tickle();
	}

	if(_rootFiber) {
		if(!stopping()) {
			_rootFiber->call();
		}
	}

	std::vector<Thread::ptr> threads;
	{
		MutexType::Lock lock(_mutex);
		threads.swap(_threads);
	}

	for(auto& thread : threads) {
		thread->join();
	}
}

void Scheduler::switchTo(int thread) {
	AZZATO_ASSERT(Scheduler::getThis() != nullptr);
	if(Scheduler::getThis() == this) {
		if(thread == -1 || thread == static_cast<int>(Thread::getCurrentThreadId())) {
			return;
		}
	}
	schedule(Fiber::getThis(), thread);
	Fiber::yieldToHold();
}

std::ostream& Scheduler::dump(std::ostream& os) {
	os << "[Scheduler name=" << _name << " size=" << _threadCount
	   << " active_count=" << _activeThreadCount.load() << " idle_count=" << _idleThreadCount.load()
	   << " stopping=" << _stopping.load() << " ]" << std::endl
	   << "    ";
	for(size_t i = 0; i < _threadIds.size(); ++i) {
		if(i) {
			os << ", ";
		}
		os << _threadIds[i];
	}
	return os;
}

void Scheduler::tickle() {
	AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT()) << "tickle";
}

bool Scheduler::stopping() {
	MutexType::Lock lock(_mutex);
	return _autoStop && _stopping.load() && _fibers.empty() && _activeThreadCount.load() == 0;
}

void Scheduler::idle() {
	while(!stopping()) {
		Fiber::yieldToHold();
	}
}

void Scheduler::run() {
	AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT()) << _name << " run";
	setThis(this);
	if(Thread::getCurrentThreadId() != static_cast<uint64_t>(_rootThread)) {
		t_scheduler_fiber = Fiber::getThis();
	}

	Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));
	Fiber::ptr cb_fiber;
	FiberAndThread ft;

	while(true) {
		ft.reset();
		bool tickle_me = false;
		bool is_active = false;
		{
			MutexType::Lock lock(_mutex);
			auto			it = _fibers.begin();
			while(it != _fibers.end()) {
				if(it->thread != -1 && it->thread != static_cast<int>(Thread::getCurrentThreadId())) {
					++it;
					tickle_me = true;
					continue;
				}

				AZZATO_ASSERT(it->fiber || it->callback);
				if(it->fiber && it->fiber->getState() == Fiber::FiberState::Execute) {
					++it;
					continue;
				}

				ft		  = *it;
				_fibers.erase(it++);
				++_activeThreadCount;
				is_active = true;
				break;
			}
			tickle_me = tickle_me || (it != _fibers.end());
		}

		if(tickle_me) {
			tickle();
		}

		if(ft.fiber && ft.fiber->getState() != Fiber::FiberState::Terminate
		   && ft.fiber->getState() != Fiber::FiberState::Exception) {
			ft.fiber->swapIn();
			--_activeThreadCount;

			if(ft.fiber->getState() == Fiber::FiberState::Ready) {
				schedule(ft.fiber);
			} else if(ft.fiber->getState() != Fiber::FiberState::Terminate
					  && ft.fiber->getState() != Fiber::FiberState::Exception) {
				ft.fiber->_state = Fiber::FiberState::Holding;
			}
			ft.reset();
		} else if(ft.callback) {
			if(cb_fiber) {
				cb_fiber->reset(ft.callback);
			} else {
				cb_fiber.reset(new Fiber(ft.callback));
			}
			ft.reset();
			cb_fiber->swapIn();
			--_activeThreadCount;
			if(cb_fiber->getState() == Fiber::FiberState::Ready) {
				schedule(cb_fiber);
				cb_fiber.reset();
			} else if(cb_fiber->getState() == Fiber::FiberState::Exception
					  || cb_fiber->getState() == Fiber::FiberState::Terminate) {
				cb_fiber->reset(nullptr);
			} else {  // Holding
				cb_fiber->_state = Fiber::FiberState::Holding;
				cb_fiber.reset();
			}
		} else {
			if(is_active) {
				--_activeThreadCount;
				continue;
			}
			if(idle_fiber->getState() == Fiber::FiberState::Terminate) {
				AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "idle fiber term";
				break;
			}

			++_idleThreadCount;
			idle_fiber->swapIn();
			--_idleThreadCount;
			if(idle_fiber->getState() != Fiber::FiberState::Terminate && idle_fiber->getState() != Fiber::FiberState::Exception) {
				idle_fiber->_state = Fiber::FiberState::Holding;
			}
		}
	}
}

const std::string& Scheduler::getSchedulerName() {
	static thread_local std::string t_name = "unknown";
	if(t_scheduler) {
		t_name = t_scheduler->getName();
	}
	return t_name;
}

uint32_t Scheduler::getThreadCount() {
	if(t_scheduler) {
		return t_scheduler->_threadCount;
	}
	return 0;
}

void Scheduler::setNameOfThread(const std::string& name) { Thread::setNameOfThis(name); }

void Scheduler::setThisFiber() { Fiber::getThis(); }

bool Scheduler::scheduleLock(Fiber::ptr fiber, int thread) {
	bool need_tickle = _fibers.empty();
	_fibers.push_back(FiberAndThread{fiber, nullptr, thread});
	return need_tickle;
}

bool Scheduler::scheduleLock(std::function<void()> callback, int thread) {
	bool need_tickle = _fibers.empty();
	_fibers.push_back(FiberAndThread{nullptr, std::move(callback), thread});
	return need_tickle;
}

SchedulerSwitcher::SchedulerSwitcher(Scheduler* target) {
	_caller = Scheduler::getThis();
	if(target) {
		target->switchTo();
	}
}

SchedulerSwitcher::~SchedulerSwitcher() {
	if(_caller) {
		_caller->switchTo();
	}
}

}  // namespace azzato
