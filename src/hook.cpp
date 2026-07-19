#include "hook.h"
#include <cstdarg>
#include <dlfcn.h>

#include "fd_manager.h"
#include "fiber.h"
#include "iomanager.h"
#include "utils/config.h"
#include "utils/macro.h"

namespace azzato {

namespace {
ConfigVar<int>::ptr g_tcpConnectTimeout = Config::lookup("tcp.connect.timeout", 5000, "tcp connect timeout");

thread_local bool t_hookEnable = false;
}  // namespace

#define HOOK_FUN(XX) \
	XX(sleep)        \
	XX(usleep)       \
	XX(nanosleep)    \
	XX(socket)       \
	XX(connect)      \
	XX(accept)       \
	XX(read)         \
	XX(readv)        \
	XX(recv)         \
	XX(recvfrom)     \
	XX(recvmsg)      \
	XX(write)        \
	XX(writev)       \
	XX(send)         \
	XX(sendto)       \
	XX(sendmsg)      \
	XX(close)        \
	XX(fcntl)        \
	XX(ioctl)        \
	XX(getsockopt)   \
	XX(setsockopt)

void hookInit() {
	static bool isInited = false;
	if(isInited) {
		return;
	}
#define XX(name) name##_f = (name##_fun)dlsym(RTLD_NEXT, #name);
	HOOK_FUN(XX);
#undef XX
}

namespace {
uint64_t s_connectTimeout = static_cast<uint64_t>(-1);

struct HookIniter {
	HookIniter() {
		hookInit();
		s_connectTimeout = g_tcpConnectTimeout->getValue();
		g_tcpConnectTimeout->addListener([](const int&, const int& newValue) {
			AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "tcp connect timeout changed to " << newValue;
			s_connectTimeout = newValue;
		});
	}
};

HookIniter s_hookIniter;
}  // namespace

bool isHookEnable() { return t_hookEnable; }

void setHookEnable(bool flag) { t_hookEnable = flag; }

}  // namespace azzato

namespace {

struct TimerInfo {
	int cancelled = 0;
};

}  // namespace

namespace azzato {

template <typename OriginFun, typename... Args>
static ssize_t
doIo(int fd, OriginFun fun, const char* hookFunName, uint32_t event, int timeoutSo, Args&&... args) {
	if(!t_hookEnable) {
		return fun(fd, std::forward<Args>(args)...);
	}

	FdCtx::ptr ctx = FdMgr::getInstance()->get(fd);
	if(!ctx) {
		return fun(fd, std::forward<Args>(args)...);
	}

	if(ctx->isClosed()) {
		errno = EBADF;
		return -1;
	}

	if(!ctx->isSocket() || ctx->getUserNonblock()) {
		return fun(fd, std::forward<Args>(args)...);
	}

	uint64_t					timeout = ctx->getTimeout(timeoutSo);
	std::shared_ptr<TimerInfo> tinfo(new TimerInfo);

retry:
	ssize_t n = fun(fd, std::forward<Args>(args)...);
	while(n == -1 && errno == EINTR) {
		n = fun(fd, std::forward<Args>(args)...);
	}
	if(n == -1 && errno == EAGAIN) {
		IOManager*				  iom = IOManager::getThis();
		Timer::ptr				  timer;
		std::weak_ptr<TimerInfo> winfo(tinfo);

		if(timeout != static_cast<uint64_t>(-1)) {
			timer = iom->addConditionTimer(
				timeout,
				[winfo, fd, iom, event]() {
					auto t = winfo.lock();
					if(!t || t->cancelled) {
						return;
					}
					t->cancelled = ETIMEDOUT;
					iom->cancelEvent(fd, static_cast<IOManager::Event>(event));
				},
				winfo);
		}

		int rt = iom->addEvent(fd, static_cast<IOManager::Event>(event));
		if(AZZATO_UNLIKELY(rt)) {
			AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << hookFunName << " addEvent(" << fd << ", " << event << ")";
			if(timer) {
				timer->cancel();
			}
			return -1;
		} else {
			Fiber::yieldToHold();
			if(timer) {
				timer->cancel();
			}
			if(tinfo->cancelled) {
				errno = tinfo->cancelled;
				return -1;
			}
			goto retry;
		}
	}

	return n;
}

}  // namespace azzato

extern "C" {
#define XX(name) name##_fun name##_f = nullptr;
HOOK_FUN(XX);
#undef XX

unsigned int sleep(unsigned int seconds) {
	if(!azzato::t_hookEnable) {
		return sleep_f(seconds);
	}

	azzato::Fiber::ptr fiber = azzato::Fiber::getThis();
	azzato::IOManager* iom	 = azzato::IOManager::getThis();
	if(!iom) {
		return sleep_f(seconds);
	}
	iom->addTimer(seconds * 1000, [fiber, iom]() { iom->schedule(fiber); });
	azzato::Fiber::yieldToHold();
	return 0;
}

int usleep(useconds_t usec) {
	if(!azzato::t_hookEnable) {
		return usleep_f(usec);
	}
	azzato::Fiber::ptr fiber = azzato::Fiber::getThis();
	azzato::IOManager* iom	 = azzato::IOManager::getThis();
	if(!iom) {
		return usleep_f(usec);
	}
	iom->addTimer(usec / 1000, [fiber, iom]() { iom->schedule(fiber); });
	azzato::Fiber::yieldToHold();
	return 0;
}

int nanosleep(const struct timespec* req, struct timespec* rem) {
	if(!azzato::t_hookEnable) {
		return nanosleep_f(req, rem);
	}

	int				  timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;
	azzato::Fiber::ptr fiber	 = azzato::Fiber::getThis();
	azzato::IOManager* iom		 = azzato::IOManager::getThis();
	if(!iom) {
		return nanosleep_f(req, rem);
	}
	iom->addTimer(timeout_ms, [fiber, iom]() { iom->schedule(fiber); });
	azzato::Fiber::yieldToHold();
	return 0;
}

int socket(int domain, int type, int protocol) {
	if(!azzato::t_hookEnable) {
		return socket_f(domain, type, protocol);
	}
	int fd = socket_f(domain, type, protocol);
	if(fd == -1) {
		return fd;
	}
	azzato::FdMgr::getInstance()->get(fd, true);
	return fd;
}

int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeout_ms) {
	if(!azzato::t_hookEnable) {
		return connect_f(fd, addr, addrlen);
	}
	azzato::FdCtx::ptr ctx = azzato::FdMgr::getInstance()->get(fd);
	if(!ctx || ctx->isClosed()) {
		errno = EBADF;
		return -1;
	}

	if(!ctx->isSocket()) {
		return connect_f(fd, addr, addrlen);
	}

	if(ctx->getUserNonblock()) {
		return connect_f(fd, addr, addrlen);
	}

	int n = connect_f(fd, addr, addrlen);
	if(n == 0) {
		return 0;
	} else if(n != -1 || errno != EINPROGRESS) {
		return n;
	}

	azzato::IOManager*		 iom = azzato::IOManager::getThis();
	azzato::Timer::ptr		 timer;
	std::shared_ptr<TimerInfo> tinfo(new TimerInfo);
	std::weak_ptr<TimerInfo>   winfo(tinfo);

	if(timeout_ms != static_cast<uint64_t>(-1)) {
		timer = iom->addConditionTimer(
			timeout_ms,
			[winfo, fd, iom]() {
				auto t = winfo.lock();
				if(!t || t->cancelled) {
					return;
				}
				t->cancelled = ETIMEDOUT;
				iom->cancelEvent(fd, azzato::IOManager::Write);
			},
			winfo);
	}

	int rt = iom->addEvent(fd, azzato::IOManager::Write);
	if(rt == 0) {
		azzato::Fiber::yieldToHold();
		if(timer) {
			timer->cancel();
		}
		if(tinfo->cancelled) {
			errno = tinfo->cancelled;
			return -1;
		}
	} else {
		if(timer) {
			timer->cancel();
		}
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "connect addEvent(" << fd << ", WRITE) error";
	}

	int		  error = 0;
	socklen_t len	= sizeof(int);
	if(-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)) {
		return -1;
	}
	if(!error) {
		return 0;
	}
	errno = error;
	return -1;
}

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
	return connect_with_timeout(sockfd, addr, addrlen, azzato::s_connectTimeout);
}

int accept(int s, struct sockaddr* addr, socklen_t* addrlen) {
	int fd = azzato::doIo(s, accept_f, "accept", azzato::IOManager::Read, SO_RCVTIMEO, addr, addrlen);
	if(fd >= 0) {
		azzato::FdMgr::getInstance()->get(fd, true);
	}
	return fd;
}

ssize_t read(int fd, void* buf, size_t count) {
	return azzato::doIo(fd, read_f, "read", azzato::IOManager::Read, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec* iov, int iovcnt) {
	return azzato::doIo(fd, readv_f, "readv", azzato::IOManager::Read, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
	return azzato::doIo(sockfd, recv_f, "recv", azzato::IOManager::Read, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd,
				 void*		 buf,
				 size_t		 len,
				 int		 flags,
				 struct sockaddr* src_addr,
				 socklen_t*	 addrlen) {
	return azzato::doIo(
		sockfd, recvfrom_f, "recvfrom", azzato::IOManager::Read, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
	return azzato::doIo(sockfd, recvmsg_f, "recvmsg", azzato::IOManager::Read, SO_RCVTIMEO, msg, flags);
}

ssize_t write(int fd, const void* buf, size_t count) {
	return azzato::doIo(fd, write_f, "write", azzato::IOManager::Write, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec* iov, int iovcnt) {
	return azzato::doIo(fd, writev_f, "writev", azzato::IOManager::Write, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int s, const void* msg, size_t len, int flags) {
	return azzato::doIo(s, send_f, "send", azzato::IOManager::Write, SO_SNDTIMEO, msg, len, flags);
}

ssize_t sendto(int s,
			   const void* msg,
			   size_t		 len,
			   int		 flags,
			   const struct sockaddr* to,
			   socklen_t	 tolen) {
	return azzato::doIo(s, sendto_f, "sendto", azzato::IOManager::Write, SO_SNDTIMEO, msg, len, flags, to, tolen);
}

ssize_t sendmsg(int s, const struct msghdr* msg, int flags) {
	return azzato::doIo(s, sendmsg_f, "sendmsg", azzato::IOManager::Write, SO_SNDTIMEO, msg, flags);
}

int close(int fd) {
	if(!azzato::t_hookEnable) {
		return close_f(fd);
	}

	azzato::FdCtx::ptr ctx = azzato::FdMgr::getInstance()->get(fd);
	if(ctx) {
		auto iom = azzato::IOManager::getThis();
		if(iom) {
			iom->cancelAll(fd);
		}
		azzato::FdMgr::getInstance()->del(fd);
	}
	return close_f(fd);
}

int fcntl(int fd, int cmd, ... /* arg */) {
	va_list va;
	va_start(va, cmd);
	switch(cmd) {
	case F_SETFL: {
		int arg = va_arg(va, int);
		va_end(va);
		azzato::FdCtx::ptr ctx = azzato::FdMgr::getInstance()->get(fd);
		if(!ctx || ctx->isClosed() || !ctx->isSocket()) {
			return fcntl_f(fd, cmd, arg);
		}
		ctx->setUserNonblock(arg & O_NONBLOCK);
		if(ctx->getSysNonblock()) {
			arg |= O_NONBLOCK;
		} else {
			arg &= ~O_NONBLOCK;
		}
		return fcntl_f(fd, cmd, arg);
	} break;
	case F_GETFL: {
		va_end(va);
		int				  arg = fcntl_f(fd, cmd);
		azzato::FdCtx::ptr ctx = azzato::FdMgr::getInstance()->get(fd);
		if(!ctx || ctx->isClosed() || !ctx->isSocket()) {
			return arg;
		}
		if(ctx->getUserNonblock()) {
			return arg | O_NONBLOCK;
		}
		return arg & ~O_NONBLOCK;
	} break;
	case F_DUPFD:
	case F_DUPFD_CLOEXEC:
	case F_SETFD:
	case F_SETOWN:
	case F_SETSIG:
	case F_SETLEASE:
	case F_NOTIFY:
#ifdef F_SETPIPE_SZ
	case F_SETPIPE_SZ:
#endif
	{
		int arg = va_arg(va, int);
		va_end(va);
		return fcntl_f(fd, cmd, arg);
	} break;
	case F_GETFD:
	case F_GETOWN:
	case F_GETSIG:
	case F_GETLEASE:
#ifdef F_GETPIPE_SZ
	case F_GETPIPE_SZ:
#endif
	{
		va_end(va);
		return fcntl_f(fd, cmd);
	} break;
	case F_SETLK:
	case F_SETLKW:
	case F_GETLK: {
		struct flock* arg = va_arg(va, struct flock*);
		va_end(va);
		return fcntl_f(fd, cmd, arg);
	} break;
	case F_GETOWN_EX:
	case F_SETOWN_EX: {
		struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
		va_end(va);
		return fcntl_f(fd, cmd, arg);
	} break;
	default:
		va_end(va);
		return fcntl_f(fd, cmd);
	}
}

int ioctl(int d, unsigned long int request, ...) {
	va_list va;
	va_start(va, request);
	void* arg = va_arg(va, void*);
	va_end(va);

	if(FIONBIO == request) {
		bool			  userNonblock = !!(*(int*)arg);
		azzato::FdCtx::ptr ctx			= azzato::FdMgr::getInstance()->get(d);
		if(!ctx || ctx->isClosed() || !ctx->isSocket()) {
			return ioctl_f(d, request, arg);
		}
		ctx->setUserNonblock(userNonblock);
	}
	return ioctl_f(d, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void* optval, socklen_t* optlen) {
	return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen) {
	if(!azzato::t_hookEnable) {
		return setsockopt_f(sockfd, level, optname, optval, optlen);
	}
	if(level == SOL_SOCKET) {
		if(optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
			azzato::FdCtx::ptr ctx = azzato::FdMgr::getInstance()->get(sockfd);
			if(ctx) {
				const timeval* v = static_cast<const timeval*>(optval);
				ctx->setTimeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
			}
		}
	}
	return setsockopt_f(sockfd, level, optname, optval, optlen);
}
}  // extern "C"
