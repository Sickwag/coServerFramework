#pragma once

#include "iomanager.h"
#include "mutex.h"
#include "streams/socket_stream.h"
#include <boost/any.hpp>
#include <list>
#include <unordered_map>

namespace azzato {

class AsyncSocketStream : public SocketStream, public std::enable_shared_from_this<AsyncSocketStream> {
  public:
	typedef std::shared_ptr<AsyncSocketStream>			ptr;
	typedef azzato::RWMutex								RWMutexType;
	typedef std::function<bool(AsyncSocketStream::ptr)> connect_callback;
	typedef std::function<void(AsyncSocketStream::ptr)> disconnect_callback;

	AsyncSocketStream(Socket::ptr sock, bool owner = true);

	virtual bool start();
	virtual void close() override;

  public:
	enum Error {
		OK			= 0,
		TIMEOUT		= -1,
		IO_ERROR	= -2,
		NOT_CONNECT = -3,
	};

  protected:
	struct SendCtx {
	  public:
		typedef std::shared_ptr<SendCtx> ptr;

		virtual ~SendCtx() {}

		virtual bool doSend(AsyncSocketStream::ptr stream) = 0;
	};

	struct Ctx : public SendCtx {
	  public:
		typedef std::shared_ptr<Ctx> ptr;

		virtual ~Ctx() {}

		Ctx();

		uint32_t sn;
		uint32_t timeout;
		uint32_t result;
		bool	 timed;

		Scheduler* scheduler;
		Fiber::ptr fiber;
		Timer::ptr timer;

		virtual void doRsp();
	};

  public:
	void setWorker(azzato::IOManager* v) { _worker = v; }

	azzato::IOManager* getWorker() const { return _worker; }

	void setIOManager(azzato::IOManager* v) { _iomanager = v; }

	azzato::IOManager* getIOManager() const { return _iomanager; }

	bool isAutoConnect() const { return _autoConnect; }

	void setAutoConnect(bool v) { _autoConnect = v; }

	connect_callback getConnectCb() const { return _connectCb; }

	disconnect_callback getDisconnectCb() const { return _disconnectCb; }

	void setConnectCb(connect_callback v) { _connectCb = v; }

	void setDisconnectCb(disconnect_callback v) { _disconnectCb = v; }

	template <class T>
	void setData(const T& v) {
		_data = v;
	}

	template <class T>
	T getData() const {
		try {
			return boost::any_cast<T>(_data);
		} catch(...) {
		}
		return T();
	}

  protected:
	virtual void	 doRead();
	virtual void	 doWrite();
	virtual void	 startRead();
	virtual void	 startWrite();
	virtual void	 onTimeOut(Ctx::ptr ctx);
	virtual Ctx::ptr doRecv() = 0;

	Ctx::ptr getCtx(uint32_t sn);
	Ctx::ptr getAndDelCtx(uint32_t sn);

	template <class T>
	std::shared_ptr<T> getCtxAs(uint32_t sn) {
		auto ctx = getCtx(sn);
		if(ctx) {
			return std::dynamic_pointer_cast<T>(ctx);
		}
		return nullptr;
	}

	template <class T>
	std::shared_ptr<T> getAndDelCtxAs(uint32_t sn) {
		auto ctx = getAndDelCtx(sn);
		if(ctx) {
			return std::dynamic_pointer_cast<T>(ctx);
		}
		return nullptr;
	}

	bool addCtx(Ctx::ptr ctx);
	bool enqueue(SendCtx::ptr ctx);

	bool innerClose();
	bool waitFiber();

  protected:
	azzato::FiberSemaphore				   _sem;
	azzato::FiberSemaphore				   _waitSem;
	RWMutexType							   _queueMutex;
	std::list<SendCtx::ptr>				   _queue;
	RWMutexType							   _mutex;
	std::unordered_map<uint32_t, Ctx::ptr> _ctxs;

	uint32_t		   _sn;
	bool			   _autoConnect;
	azzato::Timer::ptr _timer;
	azzato::IOManager* _iomanager;
	azzato::IOManager* _worker;

	connect_callback	_connectCb;
	disconnect_callback _disconnectCb;

	boost::any _data;
};

class AsyncSocketStreamManager {
  public:
	typedef azzato::RWMutex						   RWMutexType;
	typedef AsyncSocketStream::connect_callback	   connect_callback;
	typedef AsyncSocketStream::disconnect_callback disconnect_callback;

	AsyncSocketStreamManager();

	virtual ~AsyncSocketStreamManager() {}

	void				   add(AsyncSocketStream::ptr stream);
	void				   clear();
	void				   setConnection(const std::vector<AsyncSocketStream::ptr>& streams);
	AsyncSocketStream::ptr get();

	template <class T>
	std::shared_ptr<T> getAs() {
		auto rt = get();
		if(rt) {
			return std::dynamic_pointer_cast<T>(rt);
		}
		return nullptr;
	}

	connect_callback getConnectCb() const { return _connectCb; }

	disconnect_callback getDisconnectCb() const { return _disconnectCb; }

	void setConnectCb(connect_callback v);
	void setDisconnectCb(disconnect_callback v);

  private:
	RWMutexType							_mutex;
	uint32_t							_size;
	uint32_t							_idx;
	std::vector<AsyncSocketStream::ptr> _datas;
	connect_callback					_connectCb;
	disconnect_callback					_disconnectCb;
};

}  // namespace azzato
