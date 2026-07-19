#include "iomanager.h"
#include "utils/macro.h"

#include <errno.h>
#include <fcntl.h>
#include <cstring>
#include <sys/epoll.h>
#include <unistd.h>

namespace azzato {

namespace {

enum class EpollCtlOp {};

std::ostream& operator<<(std::ostream& os, EpollCtlOp op) {
	switch(static_cast<int>(op)) {
#define XX(ctl) \
	case ctl:   \
		return os << #ctl;
		XX(EPOLL_CTL_ADD);
		XX(EPOLL_CTL_MOD);
		XX(EPOLL_CTL_DEL);
#undef XX
	default:
		return os << static_cast<int>(op);
	}
}

std::ostream& operator<<(std::ostream& os, EPOLL_EVENTS events) {
	if(!events) {
		return os << "0";
	}
	bool first = true;
#define XX(E)          \
	if(events & E) {   \
		if(!first) {   \
			os << "|"; \
		}              \
		os << #E;      \
		first = false; \
	}
	XX(EPOLLIN);
	XX(EPOLLPRI);
	XX(EPOLLOUT);
	XX(EPOLLRDNORM);
	XX(EPOLLRDBAND);
	XX(EPOLLWRNORM);
	XX(EPOLLWRBAND);
	XX(EPOLLMSG);
	XX(EPOLLERR);
	XX(EPOLLHUP);
	XX(EPOLLRDHUP);
	XX(EPOLLONESHOT);
	XX(EPOLLET);
#undef XX
	return os;
}

}  // namespace

IOManager::FdContext::EventContext& IOManager::FdContext::getContext(Event event) {
	switch(event) {
	case Read:
		return read;
	case Write:
		return write;
	default:
		AZZATO_ASSERT2(false, "getContext");
	}
	throw std::invalid_argument("getContext invalid event");
}

void IOManager::FdContext::resetContext(EventContext& ctx) {
	ctx.scheduler = nullptr;
	ctx.fiber.reset();
	ctx.callback = nullptr;
}

void IOManager::FdContext::triggerEvent(Event event) {
	AZZATO_ASSERT(events & event);
	events		   = static_cast<Event>(events & ~event);
	EventContext& ctx = getContext(event);
	if(ctx.callback) {
		ctx.scheduler->schedule(ctx.callback);
	} else {
		ctx.scheduler->schedule(ctx.fiber);
	}
	resetContext(ctx);
}

IOManager::IOManager(size_t threads, bool useCaller, const std::string& name)
	: Scheduler(threads, useCaller, name) {
	_epfd = epoll_create(5000);
	AZZATO_ASSERT(_epfd > 0);

	int rt = pipe(_tickleFds);
	AZZATO_ASSERT(!rt);

	epoll_event event;
	std::memset(&event, 0, sizeof(epoll_event));
	event.events  = EPOLLIN | EPOLLET;
	event.data.fd = _tickleFds[0];

	rt		   = fcntl(_tickleFds[0], F_SETFL, O_NONBLOCK);
	AZZATO_ASSERT(!rt);

	rt = epoll_ctl(_epfd, EPOLL_CTL_ADD, _tickleFds[0], &event);
	AZZATO_ASSERT(!rt);

	contextResize(32);

	start();
}

IOManager::~IOManager() {
	stop();
	::close(_epfd);
	::close(_tickleFds[0]);
	::close(_tickleFds[1]);

	for(auto ctx : _fdContexts) {
		delete ctx;
	}
}

void IOManager::contextResize(size_t size) {
	_fdContexts.resize(size);
	for(size_t i = 0; i < _fdContexts.size(); ++i) {
		if(!_fdContexts[i]) {
			_fdContexts[i]	  = new FdContext;
			_fdContexts[i]->fd = static_cast<int>(i);
		}
	}
}

int IOManager::addEvent(int fd, Event event, std::function<void()> callback) {
	FdContext* fd_ctx = nullptr;
	{
		RWMutexType::ReadLock lock(_mutex);
		if(static_cast<int>(_fdContexts.size()) > fd) {
			fd_ctx = _fdContexts[fd];
		}
	}
	if(!fd_ctx) {
		RWMutexType::WriteLock lock(_mutex);
		contextResize(static_cast<size_t>(fd * 1.5));
		fd_ctx = _fdContexts[fd];
	}

	Mutex::Lock lock2(fd_ctx->mutex);
	if(AZZATO_UNLIKELY(fd_ctx->events & event)) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "addEvent assert fd=" << fd << " event="
											<< static_cast<EPOLL_EVENTS>(event) << " fd_ctx.event="
											<< static_cast<EPOLL_EVENTS>(fd_ctx->events);
		AZZATO_ASSERT(!(fd_ctx->events & event));
	}

	int			op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
	epoll_event epevent;
	epevent.events	 = EPOLLET | fd_ctx->events | event;
	epevent.data.ptr = fd_ctx;

	int rt = epoll_ctl(_epfd, op, fd, &epevent);
	if(rt) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "epoll_ctl(" << _epfd << ", " << static_cast<EpollCtlOp>(op) << ", "
											<< fd << ", " << static_cast<EPOLL_EVENTS>(epevent.events) << "):" << rt
											<< " (" << errno << ") (" << std::strerror(errno) << ") fd_ctx->events="
											<< static_cast<EPOLL_EVENTS>(fd_ctx->events);
		return -1;
	}

	++_pendingEventCount;
	fd_ctx->events = static_cast<Event>(fd_ctx->events | event);
	FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
	AZZATO_ASSERT(!event_ctx.scheduler && !event_ctx.fiber && !event_ctx.callback);

	event_ctx.scheduler = Scheduler::getThis();
	if(callback) {
		event_ctx.callback.swap(callback);
	} else {
		event_ctx.fiber = Fiber::getThis();
		AZZATO_ASSERT2(event_ctx.fiber->getState() == Fiber::FiberState::Execute,
					   "state=" << static_cast<int>(event_ctx.fiber->getState()));
	}
	return 0;
}

bool IOManager::delEvent(int fd, Event event) {
	RWMutexType::ReadLock lock(_mutex);
	if(static_cast<int>(_fdContexts.size()) <= fd) {
		return false;
	}
	FdContext* fd_ctx = _fdContexts[fd];
	lock.unlock();

	Mutex::Lock lock2(fd_ctx->mutex);
	if(AZZATO_UNLIKELY(!(fd_ctx->events & event))) {
		return false;
	}

	Event		new_events = static_cast<Event>(fd_ctx->events & ~event);
	int			op		   = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
	epoll_event epevent;
	epevent.events	 = EPOLLET | new_events;
	epevent.data.ptr = fd_ctx;

	int rt = epoll_ctl(_epfd, op, fd, &epevent);
	if(rt) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "epoll_ctl(" << _epfd << ", " << static_cast<EpollCtlOp>(op) << ", "
											<< fd << ", " << static_cast<EPOLL_EVENTS>(epevent.events) << "):" << rt
											<< " (" << errno << ") (" << std::strerror(errno) << ")";
		return false;
	}

	--_pendingEventCount;
	fd_ctx->events = new_events;
	fd_ctx->resetContext(fd_ctx->getContext(event));
	return true;
}

bool IOManager::cancelEvent(int fd, Event event) {
	RWMutexType::ReadLock lock(_mutex);
	if(static_cast<int>(_fdContexts.size()) <= fd) {
		return false;
	}
	FdContext* fd_ctx = _fdContexts[fd];
	lock.unlock();

	Mutex::Lock lock2(fd_ctx->mutex);
	if(AZZATO_UNLIKELY(!(fd_ctx->events & event))) {
		return false;
	}

	Event		new_events = static_cast<Event>(fd_ctx->events & ~event);
	int			op		   = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
	epoll_event epevent;
	epevent.events	 = EPOLLET | new_events;
	epevent.data.ptr = fd_ctx;

	int rt = epoll_ctl(_epfd, op, fd, &epevent);
	if(rt) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "epoll_ctl(" << _epfd << ", " << static_cast<EpollCtlOp>(op) << ", "
											<< fd << ", " << static_cast<EPOLL_EVENTS>(epevent.events) << "):" << rt
											<< " (" << errno << ") (" << std::strerror(errno) << ")";
		return false;
	}

	fd_ctx->triggerEvent(event);
	--_pendingEventCount;
	return true;
}

bool IOManager::cancelAll(int fd) {
	RWMutexType::ReadLock lock(_mutex);
	if(static_cast<int>(_fdContexts.size()) <= fd) {
		return false;
	}
	FdContext* fd_ctx = _fdContexts[fd];
	lock.unlock();

	Mutex::Lock lock2(fd_ctx->mutex);
	if(!fd_ctx->events) {
		return false;
	}

	int			op = EPOLL_CTL_DEL;
	epoll_event epevent;
	epevent.events	 = 0;
	epevent.data.ptr = fd_ctx;

	int rt = epoll_ctl(_epfd, op, fd, &epevent);
	if(rt) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "epoll_ctl(" << _epfd << ", " << static_cast<EpollCtlOp>(op) << ", "
											<< fd << ", " << static_cast<EPOLL_EVENTS>(epevent.events) << "):" << rt
											<< " (" << errno << ") (" << std::strerror(errno) << ")";
		return false;
	}

	if(fd_ctx->events & Read) {
		fd_ctx->triggerEvent(Read);
		--_pendingEventCount;
	}
	if(fd_ctx->events & Write) {
		fd_ctx->triggerEvent(Write);
		--_pendingEventCount;
	}

	AZZATO_ASSERT(fd_ctx->events == 0);
	return true;
}

IOManager* IOManager::getThis() { return dynamic_cast<IOManager*>(Scheduler::getThis()); }

void IOManager::tickle() {
	if(!hasIdleThreads()) {
		return;
	}
	int rt = ::write(_tickleFds[1], "T", 1);
	AZZATO_ASSERT(rt == 1);
}

bool IOManager::stopping(uint64_t& timeout) {
	timeout = getNextTimer();
	return timeout == ~0ull && _pendingEventCount.load() == 0 && Scheduler::stopping();
}

bool IOManager::stopping() {
	uint64_t timeout = 0;
	return stopping(timeout);
}

void IOManager::idle() {
	AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT()) << "idle";
	constexpr uint64_t	  MAX_EVENTS = 256;
	auto				  events	  = std::make_unique<epoll_event[]>(MAX_EVENTS);

	while(true) {
		uint64_t next_timeout = 0;
		if(AZZATO_UNLIKELY(stopping(next_timeout))) {
			AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "name=" << getName() << " idle stopping exit";
			// Cascade wakeup: other idle threads may still be blocked in
			// epoll_wait and rely on this write to observe _stopping.
			::write(_tickleFds[1], "T", 1);
			break;
		}

		int rt = 0;
		do {
			static const int MAX_TIMEOUT = 3000;
			if(next_timeout != ~0ull) {
				next_timeout = static_cast<int>(next_timeout) > MAX_TIMEOUT ? MAX_TIMEOUT : next_timeout;
			} else {
				next_timeout = MAX_TIMEOUT;
			}
			rt = epoll_wait(_epfd, events.get(), MAX_EVENTS, static_cast<int>(next_timeout));
			if(rt < 0 && errno == EINTR) {
				// retry on EINTR
			} else {
				break;
			}
		} while(true);

		std::vector<std::function<void()>> callbacks;
		listExpiredCb(callbacks);
		if(!callbacks.empty()) {
			schedule(callbacks.begin(), callbacks.end());
		}

		for(int i = 0; i < rt; ++i) {
			epoll_event& event = events[i];
			if(event.data.fd == _tickleFds[0]) {
				uint8_t dummy[256];
				while(::read(_tickleFds[0], dummy, sizeof(dummy)) > 0) {
				}
				continue;
			}

			FdContext*	fd_ctx = static_cast<FdContext*>(event.data.ptr);
			Mutex::Lock lock(fd_ctx->mutex);
			if(event.events & (EPOLLERR | EPOLLHUP)) {
				event.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
			}
			int real_events = None;
			if(event.events & EPOLLIN) {
				real_events |= Read;
			}
			if(event.events & EPOLLOUT) {
				real_events |= Write;
			}

			if((fd_ctx->events & real_events) == None) {
				continue;
			}

			int left_events = (fd_ctx->events & ~real_events);
			int op			= left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
			event.events	= EPOLLET | left_events;

			int rt2 = epoll_ctl(_epfd, op, fd_ctx->fd, &event);
			if(rt2) {
				AZZATO_LOG_ERROR(AZZATO_LOG_ROOT())
					<< "epoll_ctl(" << _epfd << ", " << static_cast<EpollCtlOp>(op) << ", " << fd_ctx->fd << ", "
					<< static_cast<EPOLL_EVENTS>(event.events) << "):" << rt2 << " (" << errno << ") ("
					<< std::strerror(errno) << ")";
				continue;
			}

			if(real_events & Read) {
				fd_ctx->triggerEvent(Read);
				--_pendingEventCount;
			}
			if(real_events & Write) {
				fd_ctx->triggerEvent(Write);
				--_pendingEventCount;
			}
		}

		Fiber::ptr current = Fiber::getThis();
		auto	   raw_ptr = current.get();
		current.reset();
		raw_ptr->swapOut();
	}
}

void IOManager::onTimerInsertedAtFront() { tickle(); }

}  // namespace azzato
