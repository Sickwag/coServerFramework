#include "fiber.h"
#include "scheduler.h"
#include "thread.h"
#include "utils/macro.h"

#include <atomic>
#include <cstring>

namespace azzato {

namespace {
constexpr size_t DEFAULT_STACK_SIZE	   = 128 * 1024;  // 128KB

thread_local Fiber*		t_fiber		   = nullptr;  // fiber currently executing on this thread
thread_local Fiber::ptr t_thread_fiber = nullptr;  // this thread's main fiber

std::atomic<uint64_t> s_fiberId{0};
std::atomic<uint64_t> s_fiberCount{0};
}  // namespace

Fiber::Fiber()
	: _state(FiberState::Execute)
	, _fiberId(++s_fiberId)
	, _threadId(Thread::getCurrentThreadId()) {
	++s_fiberCount;
	t_fiber = this;
	if(getcontext(&_context) != 0) {
		throw std::logic_error("getcontext failed for main fiber");
	}
}

Fiber::Fiber(std::function<void()> callback, size_t stackSize, bool useCaller)
	: _callback(std::move(callback))
	, _fiberId(++s_fiberId)
	, _threadId(Thread::getCurrentThreadId()) {
	++s_fiberCount;
	_stackSize = stackSize == 0 ? DEFAULT_STACK_SIZE : stackSize;
	_stack	   = std::make_unique<char[]>(_stackSize);

	if(getcontext(&_context) != 0) {
		throw std::logic_error("getcontext failed");
	}
	_context.uc_link		  = nullptr;
	_context.uc_stack.ss_sp	  = _stack.get();
	_context.uc_stack.ss_size = _stackSize;
	if(useCaller) {
		makecontext(&_context, &Fiber::callerMainFunc, 0);
	} else {
		makecontext(&_context, &Fiber::mainFunc, 0);
	}
}

Fiber::~Fiber() { --s_fiberCount; }

void Fiber::reset(std::function<void()> callback) {
	AZZATO_ASSERT(_stack);
	AZZATO_ASSERT(getState() == FiberState::Terminate || getState() == FiberState::Exception
				  || getState() == FiberState::Init);
	_callback = std::move(callback);
	if(getcontext(&_context) != 0) {
		throw std::logic_error("getcontext failed on reset");
	}
	_context.uc_link		  = nullptr;
	_context.uc_stack.ss_sp	  = _stack.get();
	_context.uc_stack.ss_size = _stackSize;
	makecontext(&_context, &Fiber::mainFunc, 0);
	_state = FiberState::Init;
}

void Fiber::call() {
	setThis(shared_from_this());
	_state = FiberState::Execute;
	AZZATO_ASSERT(t_thread_fiber);
	if(swapcontext(&t_thread_fiber->_context, &_context) != 0) {
		throw std::logic_error("swapcontext failed in call()");
	}
}

void Fiber::back() {
	setThis(t_thread_fiber);
	AZZATO_ASSERT(t_thread_fiber);
	if(swapcontext(&_context, &t_thread_fiber->_context) != 0) {
		throw std::logic_error("swapcontext failed in back()");
	}
}

void Fiber::swapIn() {
	setThis(shared_from_this());
	AZZATO_ASSERT(getState() != FiberState::Execute);
	_state			= FiberState::Execute;
	auto main_fiber = Scheduler::getMainFiber();
	if(!main_fiber) {
		main_fiber = t_thread_fiber;
	}
	AZZATO_ASSERT(main_fiber);
	if(swapcontext(&main_fiber->_context, &_context) != 0) {
		throw std::logic_error("swapcontext failed in swapIn()");
	}
}

void Fiber::swapOut() {
	auto main_fiber = Scheduler::getMainFiber();
	if(!main_fiber) {
		main_fiber = t_thread_fiber;
	}
	AZZATO_ASSERT(main_fiber);
	setThis(main_fiber);
	if(swapcontext(&_context, &main_fiber->_context) != 0) {
		throw std::logic_error("swapcontext failed in swapOut()");
	}
}

void Fiber::setThis(Fiber::ptr fiber) {
	AZZATO_ASSERT(fiber);
	t_fiber = fiber.get();
}

Fiber::ptr Fiber::getThis() {
	if(t_fiber) {
		return t_fiber->shared_from_this();
	}
	Fiber::ptr main_fiber(new Fiber);
	AZZATO_ASSERT(t_fiber == main_fiber.get());
	t_thread_fiber = main_fiber;
	return t_fiber->shared_from_this();
}

void Fiber::yieldToReady() {
	auto current = getThis();
	AZZATO_ASSERT(current->getState() == FiberState::Execute);
	current->_state = FiberState::Ready;
	current->swapOut();
}

void Fiber::yieldToHold() {
	auto current = getThis();
	AZZATO_ASSERT(current->getState() == FiberState::Execute);
	current->_state = FiberState::Holding;
	current->swapOut();
}

uint64_t Fiber::getTotalFibers() { return s_fiberCount.load(); }

uint64_t Fiber::getFiberIdOfThis() {
	if(t_fiber) {
		return t_fiber->getFiberId();
	}
	return 0;
}

void Fiber::mainFunc() {
	auto current = getThis();
	AZZATO_ASSERT(current);
	try {
		if(current->_callback) {
			current->_callback();
		}
		current->_callback = nullptr;
		current->_state	   = FiberState::Terminate;
	} catch(...) {
		current->_state = FiberState::Exception;
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT())
			<< "Fiber caught exception, fiber_id=" << current->_fiberId << "\n"
			<< backtraceToString(100, 2, "    ");
	}
	auto raw_ptr = current.get();
	current.reset();
	raw_ptr->swapOut();

	AZZATO_ASSERT2(false, "never reach fiber_id=" + std::to_string(raw_ptr->_fiberId));
}

void Fiber::callerMainFunc() {
	auto current = getThis();
	AZZATO_ASSERT(current);
	try {
		if(current->_callback) {
			current->_callback();
		}
		current->_callback = nullptr;
		current->_state	   = FiberState::Terminate;
	} catch(...) {
		current->_state = FiberState::Exception;
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT())
			<< "Fiber caught exception, fiber_id=" << current->_fiberId << "\n"
			<< backtraceToString(100, 2, "    ");
	}
	auto raw_ptr = current.get();
	current.reset();
	raw_ptr->back();

	AZZATO_ASSERT2(false, "never reach fiber_id=" + std::to_string(raw_ptr->_fiberId));
}

}  // namespace azzato
