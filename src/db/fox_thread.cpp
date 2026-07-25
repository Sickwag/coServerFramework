#include "db/fox_thread.h"
#include "utils/config.h"
#include "utils/macro.h"
#include "utils/util.h"

#include <iomanip>
#include <list>
#include <stdexcept>
#include <thread>
#include <vector>

namespace azzato {

namespace {
ConfigVar<std::map<std::string, std::map<std::string, std::string>>>::ptr gThreadInfoSet =
	Config::lookup("fox_thread",
				   std::map<std::string, std::map<std::string, std::string>>(),
				   "config for thread");
}

static RWMutex						   sThreadMutex;
static std::map<uint64_t, std::string> sThreadNames;

thread_local FoxThread* s_thread = nullptr;

void FoxThread::read_cb(evutil_socket_t sock, short which, void* args) {
	FoxThread* thread = static_cast<FoxThread*>(args);
	uint8_t	   cmd[4096];
	if(recv(sock, cmd, sizeof(cmd), 0) > 0) {
		std::list<callback> callbacks;
		RWMutex::WriteLock	lock(thread->_mutex);
		callbacks.swap(thread->_callbacks);
		lock.unlock();
		thread->_working = true;
		for(auto it = callbacks.begin(); it != callbacks.end(); ++it) {
			if(*it) {
				try {
					(*it)();
				} catch(std::exception& ex) {
					AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "exception:" << ex.what();
				} catch(const char* c) {
					AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "exception:" << c;
				} catch(...) {
					AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "uncatch exception";
				}
			} else {
				event_base_loopbreak(thread->_base);
				thread->_start = false;
				thread->unsetThis();
				break;
			}
		}
		Atomic::addFetch(thread->_total, callbacks.size());
		thread->_working = false;
	}
}

FoxThread::FoxThread(const std::string& name, struct event_base* base)
	: _read(0)
	, _write(0)
	, _base(nullptr)
	, _event(nullptr)
	, _thread(nullptr)
	, _name(name)
	, _working(false)
	, _start(false)
	, _total(0) {
	int fds[2];
	if(evutil_socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1) {
		throw std::logic_error("thread init error");
	}

	evutil_make_socket_nonblocking(fds[0]);
	evutil_make_socket_nonblocking(fds[1]);

	_read  = fds[0];
	_write = fds[1];

	if(base) {
		_base = base;
		setThis();
	} else {
		_base = event_base_new();
	}
	_event = event_new(_base, _read, EV_READ | EV_PERSIST, read_cb, this);
	event_add(_event, nullptr);
}

void FoxThread::dump(std::ostream& os) {
	RWMutex::ReadLock lock(_mutex);
	os << "[thread name=" << _name << " working=" << _working << " tasks=" << _callbacks.size()
	   << " total=" << _total << "]" << std::endl;
}

std::thread::id FoxThread::getId() const {
	if(_thread) {
		return _thread->get_id();
	}
	return std::thread::id();
}

void* FoxThread::getData(const std::string& name) {
	auto it = _datas.find(name);
	return it == _datas.end() ? nullptr : it->second;
}

void FoxThread::setData(const std::string& name, void* v) { _datas[name] = v; }

FoxThread::~FoxThread() {
	if(_read) {
		close(_read);
	}
	if(_write) {
		close(_write);
	}
	stop();
	join();
	if(_event) {
		event_free(_event);
	}
	if(_base) {
		event_base_free(_base);
	}
}

void FoxThread::start() {
	if(_thread) {
		throw std::logic_error("FoxThread is running");
	}

	_thread = new std::thread(std::bind(&FoxThread::thread_cb, this));
	_start	= true;
}

void FoxThread::thread_cb() {
	setThis();
	pthread_setname_np(pthread_self(), _name.substr(0, 15).c_str());
	if(_initCallback) {
		_initCallback(this);
		_initCallback = nullptr;
	}
	event_base_loop(_base, 0);
}

bool FoxThread::dispatch(callback cb) {
	RWMutex::WriteLock lock(_mutex);
	_callbacks.push_back(std::move(cb));
	lock.unlock();
	uint8_t cmd = 1;
	if(send(_write, &cmd, sizeof(cmd), 0) <= 0) {
		return false;
	}
	return true;
}

bool FoxThread::dispatch(uint32_t id, callback cb) { return dispatch(std::move(cb)); }

bool FoxThread::batchDispatch(const std::vector<callback>& cbs) {
	RWMutex::WriteLock lock(_mutex);
	for(auto& i : cbs) {
		_callbacks.push_back(i);
	}
	lock.unlock();
	uint8_t cmd = 1;
	if(send(_write, &cmd, sizeof(cmd), 0) <= 0) {
		return false;
	}
	return true;
}

void FoxThread::broadcast(callback cb) { dispatch(std::move(cb)); }

void FoxThread::stop() {
	RWMutex::WriteLock lock(_mutex);
	_callbacks.push_back(nullptr);
	if(_thread) {
		uint8_t cmd = 0;
		send(_write, &cmd, sizeof(cmd), 0);
	}
}

void FoxThread::join() {
	if(_thread) {
		_thread->join();
		delete _thread;
		_thread = nullptr;
	}
}

FoxThreadPool::FoxThreadPool(uint32_t size, const std::string& name, bool advance)
	: _size(size)
	, _cur(0)
	, _name(name)
	, _advance(advance)
	, _start(false)
	, _total(0) {
	_threads.resize(_size);
	for(size_t i = 0; i < size; ++i) {
		_threads[i] = new FoxThread(name + "_" + std::to_string(i));
	}
}

FoxThreadPool::~FoxThreadPool() {
	for(size_t i = 0; i < _size; ++i) {
		delete _threads[i];
	}
}

void FoxThreadPool::start() {
	for(size_t i = 0; i < _size; ++i) {
		_threads[i]->setInitCb(_initCallback);
		_threads[i]->start();
		_freeFoxThreads.push_back(_threads[i]);
	}
	if(_initCallback) {
		_initCallback = nullptr;
	}
	_start = true;
	check();
}

void FoxThreadPool::stop() {
	for(size_t i = 0; i < _size; ++i) {
		_threads[i]->stop();
	}
	_start = false;
}

void FoxThreadPool::join() {
	for(size_t i = 0; i < _size; ++i) {
		_threads[i]->join();
	}
}

void FoxThreadPool::releaseFoxThread(FoxThread* t) {
	do {
		RWMutex::WriteLock lock(_mutex);
		_freeFoxThreads.push_back(t);
	} while(0);
	check();
}

bool FoxThreadPool::dispatch(callback cb) {
	do {
		Atomic::addFetch(_total, (uint64_t)1);
		RWMutex::WriteLock lock(_mutex);
		if(!_advance) {
			return _threads[_cur++ % _size]->dispatch(std::move(cb));
		}
		_callbacks.push_back(std::move(cb));
	} while(0);
	check();
	return true;
}

bool FoxThreadPool::batchDispatch(const std::vector<callback>& cbs) {
	Atomic::addFetch(_total, cbs.size());
	RWMutex::WriteLock lock(_mutex);
	if(!_advance) {
		for(auto cb : cbs) {
			_threads[_cur++ % _size]->dispatch(cb);
		}
		return true;
	}
	for(auto cb : cbs) {
		_callbacks.push_back(cb);
	}
	lock.unlock();
	check();
	return true;
}

void FoxThreadPool::check() {
	do {
		if(!_start) {
			break;
		}
		RWMutex::WriteLock lock(_mutex);
		if(_freeFoxThreads.empty() || _callbacks.empty()) {
			break;
		}

		std::shared_ptr<FoxThread> thr(
			_freeFoxThreads.front(),
			std::bind(&FoxThreadPool::releaseFoxThread, this, std::placeholders::_1));
		_freeFoxThreads.pop_front();

		callback cb = _callbacks.front();
		_callbacks.pop_front();
		lock.unlock();

		if(thr->isStart()) {
			thr->dispatch(std::bind(&FoxThreadPool::wrapcb, this, thr, std::move(cb)));
		} else {
			RWMutex::WriteLock lock(_mutex);
			_callbacks.push_front(std::move(cb));
		}
	} while(true);
}

void FoxThreadPool::wrapcb(std::shared_ptr<FoxThread> thr, callback cb) { cb(); }

bool FoxThreadPool::dispatch(uint32_t id, callback cb) {
	Atomic::addFetch(_total, (uint64_t)1);
	return _threads[id % _size]->dispatch(std::move(cb));
}

FoxThread* FoxThreadPool::getRandFoxThread() { return _threads[_cur++ % _size]; }

void FoxThreadPool::broadcast(callback cb) {
	for(size_t i = 0; i < _threads.size(); ++i) {
		_threads[i]->dispatch(cb);
	}
}

void FoxThreadPool::dump(std::ostream& os) {
	RWMutex::ReadLock lock(_mutex);
	os << "[FoxThreadPool name = " << _name << " thread_count = " << _threads.size()
	   << " tasks = " << _callbacks.size() << " total = " << _total << " advance = " << _advance << "]"
	   << std::endl;
	for(size_t i = 0; i < _threads.size(); ++i) {
		os << "    ";
		_threads[i]->dump(os);
	}
}

FoxThread* FoxThread::GetThis() { return s_thread; }

const std::string& FoxThread::GetFoxThreadName() {
	FoxThread* t = GetThis();
	if(t) {
		return t->_name;
	}

	uint64_t tid = getThreadId();
	do {
		RWMutex::ReadLock lock(sThreadMutex);
		auto			  it = sThreadNames.find(tid);
		if(it != sThreadNames.end()) {
			return it->second;
		}
	} while(0);

	do {
		RWMutex::WriteLock lock(sThreadMutex);
		sThreadNames[tid] = "UNNAME_" + std::to_string(tid);
		return sThreadNames[tid];
	} while(0);
}

void FoxThread::GetAllFoxThreadName(std::map<uint64_t, std::string>& names) {
	RWMutex::ReadLock lock(sThreadMutex);
	for(auto it = sThreadNames.begin(); it != sThreadNames.end(); ++it) {
		names.insert(*it);
	}
}

void FoxThread::setThis() {
	_name	 = _name + "_" + std::to_string(getThreadId());
	s_thread = this;

	RWMutex::WriteLock lock(sThreadMutex);
	sThreadNames[getThreadId()] = _name;
}

void FoxThread::unsetThis() {
	s_thread = nullptr;
	RWMutex::WriteLock lock(sThreadMutex);
	sThreadNames.erase(getThreadId());
}

IFoxThread::ptr FoxThreadManager::get(const std::string& name) {
	auto it = _threads.find(name);
	return it == _threads.end() ? nullptr : it->second;
}

void FoxThreadManager::add(const std::string& name, IFoxThread::ptr thr) { _threads[name] = thr; }

void FoxThreadManager::dispatch(const std::string& name, callback cb) {
	IFoxThread::ptr ti = get(name);
	AZZATO_ASSERT(ti);
	ti->dispatch(std::move(cb));
}

void FoxThreadManager::dispatch(const std::string& name, uint32_t id, callback cb) {
	IFoxThread::ptr ti = get(name);
	AZZATO_ASSERT(ti);
	ti->dispatch(id, std::move(cb));
}

void FoxThreadManager::batchDispatch(const std::string& name, const std::vector<callback>& cbs) {
	IFoxThread::ptr ti = get(name);
	AZZATO_ASSERT(ti);
	ti->batchDispatch(cbs);
}

void FoxThreadManager::broadcast(const std::string& name, callback cb) {
	IFoxThread::ptr ti = get(name);
	AZZATO_ASSERT(ti);
	ti->broadcast(std::move(cb));
}

void FoxThreadManager::dumpFoxThreadStatus(std::ostream& os) {
	os << "FoxThreadManager: " << std::endl;
	for(auto it = _threads.begin(); it != _threads.end(); ++it) {
		it->second->dump(os);
	}

	os << "All FoxThreads:" << std::endl;
	std::map<uint64_t, std::string> names;
	FoxThread::GetAllFoxThreadName(names);
	for(auto it = names.begin(); it != names.end(); ++it) {
		os << std::setw(30) << it->first << ": " << it->second << std::endl;
	}
}

void FoxThreadManager::init() {
	auto m = gThreadInfoSet->getValue();
	for(auto i : m) {
		auto num	 = getParamValue(i.second, "num", 0);
		auto name	 = i.first;
		auto advance = getParamValue(i.second, "advance", 0);
		if(num <= 0) {
			AZZATO_LOG_ERROR(AZZATO_LOG_ROOT())
				<< "thread pool:" << name << " num:" << num << " advance:" << advance << " invalid";
			continue;
		}
		if(num == 1) {
			_threads[i.first] = std::make_shared<FoxThread>(i.first);
			AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "init thread : " << i.first;
		} else {
			_threads[i.first] = std::make_shared<FoxThreadPool>(num, name, advance != 0);
			AZZATO_LOG_INFO(AZZATO_LOG_ROOT())
				<< "init thread pool:" << name << " num:" << num << " advance:" << advance;
		}
	}
}

void FoxThreadManager::start() {
	for(auto i : _threads) {
		AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "thread: " << i.first << " start begin";
		i.second->start();
		AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "thread: " << i.first << " start end";
	}
}

void FoxThreadManager::stop() {
	for(auto i : _threads) {
		AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "thread: " << i.first << " stop begin";
		i.second->stop();
		AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "thread: " << i.first << " stop end";
	}
	for(auto i : _threads) {
		AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "thread: " << i.first << " join begin";
		i.second->join();
		AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "thread: " << i.first << " join end";
	}
}

}  // namespace azzato
