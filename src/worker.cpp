#include "worker.h"
#include "utils/config.h"
#include "utils/util.h"

namespace azzato {

static azzato::ConfigVar<std::map<std::string, std::map<std::string, std::string>>>::ptr g_worker_config =
	azzato::Config::lookup("workers",
						   std::map<std::string, std::map<std::string, std::string>>(),
						   "worker config");

WorkerGroup::WorkerGroup(uint32_t batch_size, azzato::Scheduler* s)
	: _batchSize(batch_size)
	, _finish(false)
	, _scheduler(s)
	, _sem(batch_size) {}

WorkerGroup::~WorkerGroup() { waitAll(); }

void WorkerGroup::schedule(std::function<void()> cb, int thread) {
	_sem.wait();
	_scheduler->schedule(std::bind(&WorkerGroup::doWork, shared_from_this(), cb), thread);
}

void WorkerGroup::doWork(std::function<void()> cb) {
	cb();
	_sem.notify();
}

void WorkerGroup::waitAll() {
	if(!_finish) {
		_finish = true;
		for(uint32_t i = 0; i < _batchSize; ++i) {
			_sem.wait();
		}
	}
}

WorkerManager::WorkerManager()
	: _stop(false) {}

void WorkerManager::add(Scheduler::ptr s) { _datas[s->getName()].push_back(s); }

Scheduler::ptr WorkerManager::get(const std::string& name) {
	auto it = _datas.find(name);
	if(it == _datas.end()) {
		return nullptr;
	}
	if(it->second.size() == 1) {
		return it->second[0];
	}
	return it->second[rand() % it->second.size()];
}

IOManager::ptr WorkerManager::getAsIOManager(const std::string& name) {
	return std::dynamic_pointer_cast<IOManager>(get(name));
}

bool WorkerManager::init(const std::map<std::string, std::map<std::string, std::string>>& v) {
	for(auto& i : v) {
		std::string name	   = i.first;
		int32_t		thread_num = azzato::getParamValue(i.second, "thread_num", 1);
		int32_t		worker_num = azzato::getParamValue(i.second, "worker_num", 1);

		for(int32_t x = 0; x < worker_num; ++x) {
			Scheduler::ptr s;
			if(!x) {
				s = std::make_shared<IOManager>(thread_num, false, name);
			} else {
				s = std::make_shared<IOManager>(thread_num, false, name + "-" + std::to_string(x));
			}
			add(s);
		}
	}
	_stop = _datas.empty();
	return true;
}

bool WorkerManager::init() {
	auto workers = g_worker_config->getValue();
	return init(workers);
}

void WorkerManager::stop() {
	if(_stop) {
		return;
	}
	for(auto& i : _datas) {
		for(auto& n : i.second) {
			n->schedule([]() {});
			n->stop();
		}
	}
	_datas.clear();
	_stop = true;
}

uint32_t WorkerManager::getCount() { return _datas.size(); }

std::ostream& WorkerManager::dump(std::ostream& os) {
	for(auto& i : _datas) {
		for(auto& n : i.second) {
			n->dump(os) << std::endl;
		}
	}
	return os;
}

}  // namespace azzato
