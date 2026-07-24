#pragma once

#include "iomanager.h"
#include "log.h"
#include "mutex.h"
#include "utils/singleton.h"

namespace azzato {

class WorkerGroup : Noncopyable, public std::enable_shared_from_this<WorkerGroup> {
  public:
	typedef std::shared_ptr<WorkerGroup> ptr;

	static WorkerGroup::ptr Create(uint32_t batch_size, azzato::Scheduler* s = azzato::Scheduler::getThis()) {
		return std::make_shared<WorkerGroup>(batch_size, s);
	}

	WorkerGroup(uint32_t batch_size, azzato::Scheduler* s = azzato::Scheduler::getThis());
	~WorkerGroup();

	void schedule(std::function<void()> cb, int thread = -1);
	void waitAll();

  private:
	void doWork(std::function<void()> cb);

  private:
	uint32_t	   _batchSize;
	bool		   _finish;
	Scheduler*	   _scheduler;
	FiberSemaphore _sem;
};

class WorkerManager {
  public:
	WorkerManager();
	void		   add(Scheduler::ptr s);
	Scheduler::ptr get(const std::string& name);
	IOManager::ptr getAsIOManager(const std::string& name);

	template <class FiberOrCb>
	void schedule(const std::string& name, FiberOrCb fc, int thread = -1) {
		auto s = get(name);
		if(s) {
			s->schedule(fc, thread);
		} else {
			static azzato::Logger::ptr s_logger = AZZATO_LOG_NAME("system");
			AZZATO_LOG_ERROR(s_logger) << "schedule name=" << name << " not exists";
		}
	}

	template <class Iter>
	void schedule(const std::string& name, Iter begin, Iter end) {
		auto s = get(name);
		if(s) {
			s->schedule(begin, end);
		} else {
			static azzato::Logger::ptr s_logger = AZZATO_LOG_NAME("system");
			AZZATO_LOG_ERROR(s_logger) << "schedule name=" << name << " not exists";
		}
	}

	bool init();
	bool init(const std::map<std::string, std::map<std::string, std::string>>& v);
	void stop();

	bool isStoped() const { return _stop; }

	std::ostream& dump(std::ostream& os);

	uint32_t getCount();

  private:
	std::map<std::string, std::vector<Scheduler::ptr>> _datas;
	bool											   _stop;
};

typedef azzato::Singleton<WorkerManager> WorkerMgr;

}  // namespace azzato
