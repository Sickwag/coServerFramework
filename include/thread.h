#pragma once

#include "utils/noncopyable.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace azzato {

class Thread : private Noncopyable {
  public:
	using ptr = std::shared_ptr<Thread>;

	explicit Thread(std::function<void()> callback, const std::string& name = "unknown");

	~Thread();

	const std::string& getName() const { return _name; }

	void join();

	static Thread* getThis();

	static const std::string& getNameOfThis();

	static void setNameOfThis(const std::string& name);

	static void yield();

	static uint64_t getCurrentThreadId();

	static uint32_t getTotalThreads();

  private:
	void runCallback();

  private:
	std::thread			  _thread;
	std::function<void()> _callback;
	std::string			  _name;
	uint64_t			  _threadId = 0;
};

}  // namespace azzato
