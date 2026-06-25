#pragma once

#include <functional>
#include <memory>
#include <ucontext.h>

namespace azzato {
class Scheduler;

class Fiber : public std::enable_shared_from_this<Fiber> {
	friend class Scheduler;

  public:
	using ptr = std::shared_ptr<Fiber>;

	enum class FiberState { Init, Holding, Execute, Terminate, Ready, Exception };

	Fiber(std::function<void()> callback, size_t stacksize = 0, bool useCaller = false);
	~Fiber();

  private:
	Fiber();
};
}  // namespace azzato
