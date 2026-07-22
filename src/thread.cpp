#include "thread.h"
#include "utils/macro.h"

#include <atomic>
#include <cstring>
#include <exception>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace azzato {

namespace {
thread_local Thread*	 t_thread	   = nullptr;
thread_local std::string t_thread_name = "unknown";
std::atomic<uint32_t>	 s_threadCount{0};
std::atomic<uint64_t>	 s_threadId{0};
}  // namespace

Thread::Thread(std::function<void()> callback, const std::string& name)
	: _callback(std::move(callback))
	, _name(name) {
	AZZATO_ASSERT(_callback);
	_thread = std::thread([this] {
		t_thread	  = this;
		t_thread_name = _name;
		_threadId	  = static_cast<uint64_t>(::syscall(SYS_gettid));
		s_threadId	  = _threadId;
		++s_threadCount;
		pthread_setname_np(pthread_self(), _name.substr(0, 15).c_str());
		try {
			_callback();
		} catch(const std::exception& ex) {
			AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "Thread callback threw: " << ex.what();
		} catch(...) {
			AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "Thread callback threw a non-standard exception";
		}
	});
}

Thread::~Thread() {
	if(_thread.joinable()) {
		_thread.join();
	}
}

void Thread::join() {
	if(_thread.joinable()) {
		_thread.join();
	}
}

Thread* Thread::getThis() { return t_thread; }

const std::string& Thread::getNameOfThis() { return t_thread_name; }

void Thread::setNameOfThis(const std::string& name) {
	t_thread_name = name;
	pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());
}

void Thread::yield() { ::sched_yield(); }

uint64_t Thread::getCurrentThreadId() { return static_cast<uint64_t>(::syscall(SYS_gettid)); }

uint32_t Thread::getTotalThreads() { return s_threadCount.load(); }

void Thread::runCallback() { _callback(); }

}  // namespace azzato
