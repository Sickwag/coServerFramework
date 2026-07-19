#pragma once

#include "utils/noncopyable.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <ucontext.h>

namespace azzato {

class Scheduler;

/**
 * @brief User-space coroutine (fiber) built on ucontext.
 *
 * A fiber is an independent execution context with its own stack. It cooperates
 * with a Scheduler: swapIn() transfers control from the scheduler's main fiber
 * to this fiber, and swapOut() yields back. call()/back() are the caller-thread
 * variants used when a fiber runs directly on the caller's stack.
 */
class Fiber : public std::enable_shared_from_this<Fiber>, private Noncopyable {
	friend class Scheduler;

  public:
	using ptr = std::shared_ptr<Fiber>;

	enum class FiberState {
		Init,
		Holding,
		Execute,
		Terminate,
		Ready,
		Exception
	};

	explicit Fiber(std::function<void()> callback, size_t stackSize = 0, bool useCaller = false);

	~Fiber();

	void reset(std::function<void()> callback);

	void call();

	void back();

	FiberState getState() const { return _state; }

	uint64_t getFiberId() const { return _fiberId; }

	uint64_t getThreadId() const { return _threadId; }

	static void setThis(ptr fiber);

	static ptr getThis();

	static void yieldToReady();

	static void yieldToHold();

	static uint64_t getTotalFibers();

	static uint64_t getFiberIdOfThis();

  private:
	// Only used to create the thread's main fiber.
	Fiber();

	void swapIn();

	void swapOut();

	static void mainFunc();

	static void callerMainFunc();

  private:
	ucontext_t			   _context{};
	std::function<void()>  _callback;
	std::unique_ptr<char[]> _stack;
	size_t				   _stackSize = 0;
	FiberState			   _state	   = FiberState::Init;
	uint64_t			   _fiberId   = 0;
	uint64_t			   _threadId  = 0;
};

}  // namespace azzato
