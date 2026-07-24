#include "streams/async_socket_stream.h"
#include "log.h"
#include "utils/macro.h"
#include "utils/util.h"

namespace azzato {

static azzato::Logger::ptr g_logger = AZZATO_LOG_NAME("system");

AsyncSocketStream::Ctx::Ctx()
	: sn(0)
	, timeout(0)
	, result(0)
	, timed(false)
	, scheduler(nullptr) {}

void AsyncSocketStream::Ctx::doRsp() {
	Scheduler* scd = scheduler;
	if(!azzato::Atomic::compareAndSwapBool(scheduler, scd, (Scheduler*)nullptr)) {
		return;
	}
	if(!scd || !fiber) {
		return;
	}
	if(timer) {
		timer->cancel();
		timer = nullptr;
	}

	if(timed) {
		result = TIMEOUT;
	}
	scd->schedule(fiber);
}

AsyncSocketStream::AsyncSocketStream(Socket::ptr sock, bool owner)
	: SocketStream(sock, owner)
	, _waitSem(2)
	, _sn(0)
	, _autoConnect(false)
	, _iomanager(nullptr)
	, _worker(nullptr) {}

bool AsyncSocketStream::start() {
	if(!_iomanager) {
		_iomanager = azzato::IOManager::getThis();
	}
	if(!_worker) {
		_worker = azzato::IOManager::getThis();
	}

	do {
		waitFiber();

		if(_timer) {
			_timer->cancel();
			_timer = nullptr;
		}

		if(!isConnected()) {
			if(!_socket->reconnect()) {
				innerClose();
				_waitSem.notify();
				_waitSem.notify();
				break;
			}
		}

		if(_connectCb) {
			if(!_connectCb(shared_from_this())) {
				innerClose();
				_waitSem.notify();
				_waitSem.notify();
				break;
			}
		}

		startRead();
		startWrite();
		return true;
	} while(false);

	if(_autoConnect) {
		if(_timer) {
			_timer->cancel();
			_timer = nullptr;
		}

		_timer = _iomanager->addTimer(2 * 1000, std::bind(&AsyncSocketStream::start, shared_from_this()));
	}
	return false;
}

void AsyncSocketStream::doRead() {
	try {
		while(isConnected()) {
			auto ctx = doRecv();
			if(ctx) {
				ctx->doRsp();
			}
		}
	} catch(...) {
		// TODO log
	}

	AZZATO_LOG_DEBUG(g_logger) << "doRead out " << this;
	innerClose();
	_waitSem.notify();

	if(_autoConnect) {
		_iomanager->addTimer(10, std::bind(&AsyncSocketStream::start, shared_from_this()));
	}
}

void AsyncSocketStream::doWrite() {
	try {
		while(isConnected()) {
			_sem.wait();
			std::list<SendCtx::ptr> ctxs;
			{
				RWMutexType::WriteLock lock(_queueMutex);
				_queue.swap(ctxs);
			}
			auto self = shared_from_this();
			for(auto& i : ctxs) {
				if(!i->doSend(self)) {
					innerClose();
					break;
				}
			}
		}
	} catch(...) {
		// TODO log
	}
	AZZATO_LOG_DEBUG(g_logger) << "doWrite out " << this;
	{
		RWMutexType::WriteLock lock(_queueMutex);
		_queue.clear();
	}
	_waitSem.notify();
}

void AsyncSocketStream::startRead() {
	_iomanager->schedule(std::bind(&AsyncSocketStream::doRead, shared_from_this()));
}

void AsyncSocketStream::startWrite() {
	_iomanager->schedule(std::bind(&AsyncSocketStream::doWrite, shared_from_this()));
}

void AsyncSocketStream::onTimeOut(Ctx::ptr ctx) {
	{
		RWMutexType::WriteLock lock(_mutex);
		_ctxs.erase(ctx->sn);
	}
	ctx->timed = true;
	ctx->doRsp();
}

AsyncSocketStream::Ctx::ptr AsyncSocketStream::getCtx(uint32_t sn) {
	RWMutexType::ReadLock lock(_mutex);
	auto				  it = _ctxs.find(sn);
	return it != _ctxs.end() ? it->second : nullptr;
}

AsyncSocketStream::Ctx::ptr AsyncSocketStream::getAndDelCtx(uint32_t sn) {
	Ctx::ptr			   ctx;
	RWMutexType::WriteLock lock(_mutex);
	auto				   it = _ctxs.find(sn);
	if(it != _ctxs.end()) {
		ctx = it->second;
		_ctxs.erase(it);
	}
	return ctx;
}

bool AsyncSocketStream::addCtx(Ctx::ptr ctx) {
	RWMutexType::WriteLock lock(_mutex);
	_ctxs.insert(std::make_pair(ctx->sn, ctx));
	return true;
}

bool AsyncSocketStream::enqueue(SendCtx::ptr ctx) {
	AZZATO_ASSERT(ctx);
	RWMutexType::WriteLock lock(_queueMutex);
	bool				   empty = _queue.empty();
	_queue.push_back(ctx);
	lock.unlock();
	if(empty) {
		_sem.notify();
	}
	return empty;
}

bool AsyncSocketStream::innerClose() {
	AZZATO_ASSERT(_iomanager == azzato::IOManager::getThis());
	if(isConnected() && _disconnectCb) {
		_disconnectCb(shared_from_this());
	}
	SocketStream::close();
	_sem.notify();
	std::unordered_map<uint32_t, Ctx::ptr> ctxs;
	{
		RWMutexType::WriteLock lock(_mutex);
		ctxs.swap(_ctxs);
	}
	{
		RWMutexType::WriteLock lock(_queueMutex);
		_queue.clear();
	}
	for(auto& i : ctxs) {
		i.second->result = IO_ERROR;
		i.second->doRsp();
	}
	return true;
}

bool AsyncSocketStream::waitFiber() {
	_waitSem.wait();
	_waitSem.wait();
	return true;
}

void AsyncSocketStream::close() {
	_autoConnect = false;
	SchedulerSwitcher ss(_iomanager);
	if(_timer) {
		_timer->cancel();
	}
	SocketStream::close();
}

AsyncSocketStreamManager::AsyncSocketStreamManager()
	: _size(0)
	, _idx(0) {}

void AsyncSocketStreamManager::add(AsyncSocketStream::ptr stream) {
	RWMutexType::WriteLock lock(_mutex);
	_datas.push_back(stream);
	++_size;

	if(_connectCb) {
		stream->setConnectCb(_connectCb);
	}

	if(_disconnectCb) {
		stream->setDisconnectCb(_disconnectCb);
	}
}

void AsyncSocketStreamManager::clear() {
	RWMutexType::WriteLock lock(_mutex);
	for(auto& i : _datas) {
		i->close();
	}
	_datas.clear();
	_size = 0;
}

void AsyncSocketStreamManager::setConnection(const std::vector<AsyncSocketStream::ptr>& streams) {
	auto				   cs = streams;
	RWMutexType::WriteLock lock(_mutex);
	cs.swap(_datas);
	_size = _datas.size();
	if(_connectCb || _disconnectCb) {
		for(auto& i : _datas) {
			if(_connectCb) {
				i->setConnectCb(_connectCb);
			}
			if(_connectCb) {
				i->setDisconnectCb(_disconnectCb);
			}
		}
	}
	lock.unlock();

	for(auto& i : cs) {
		i->close();
	}
}

AsyncSocketStream::ptr AsyncSocketStreamManager::get() {
	RWMutexType::ReadLock lock(_mutex);
	for(uint32_t i = 0; i < _size; ++i) {
		auto idx = azzato::Atomic::addFetch(_idx, 1);
		if(_datas[idx % _size]->isConnected()) {
			return _datas[idx % _size];
		}
	}
	return nullptr;
}

void AsyncSocketStreamManager::setConnectCb(connect_callback v) {
	_connectCb = v;
	RWMutexType::WriteLock lock(_mutex);
	for(auto& i : _datas) {
		i->setConnectCb(_connectCb);
	}
}

void AsyncSocketStreamManager::setDisconnectCb(disconnect_callback v) {
	_disconnectCb = v;
	RWMutexType::WriteLock lock(_mutex);
	for(auto& i : _datas) {
		i->setDisconnectCb(_disconnectCb);
	}
}

}  // namespace azzato
