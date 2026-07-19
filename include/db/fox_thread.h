#pragma once
#include "mutex.h"
#include "utils/singleton.h"
#include <event2/buffer.h>
#include <event2/event.h>
#include <map>

namespace azzato {
class FoxThread;

class IFoxThread {
  public:
	using ptr = std::shared_ptr<IFoxThread>;
	using callback = std::function<void()>;
	virtual ~IFoxThread(){};
	virtual bool	 dispatch(callback cb)							 = 0;
	virtual bool	 dispatch(uint32_t id, callback cb)				 = 0;
	virtual bool	 batchDispatch(const std::vector<callback>& cbs) = 0;
	virtual void	 broadcast(callback cb)							 = 0;
	virtual void	 start()										 = 0;
	virtual void	 stop()											 = 0;
	virtual void	 join()											 = 0;
	virtual void	 dump(std::ostream& os)							 = 0;
	virtual uint64_t getTotal()										 = 0;
};

class FoxThread : public IFoxThread {
  public:
	typedef std::shared_ptr<FoxThread>		ptr;
	typedef IFoxThread::callback			callback;
	typedef std::function<void(FoxThread*)> initCallback;
	FoxThread(const std::string& name = "", struct event_base* base = NULL);
	~FoxThread();
	static FoxThread*		  GetThis();
	static const std::string& GetFoxThreadName();
	static void				  GetAllFoxThreadName(std::map<uint64_t, std::string>& names);
	void					  setThis();
	void					  unsetThis();
	void					  start();
	virtual bool			  dispatch(callback cb);
	virtual bool			  dispatch(uint32_t id, callback cb);
	virtual bool			  batchDispatch(const std::vector<callback>& cbs);
	virtual void			  broadcast(callback cb);
	void					  join();
	void					  stop();

	bool isStart() const { return _start; }

	struct event_base* getBase() { return _base; }

	std::thread::id getId() const;
	void*			getData(const std::string& name);

	template <class T>
	T* getData(const std::string& name) {
		return (T*)getData(name);
	}

	void setData(const std::string& name, void* v);

	void setInitCb(initCallback v) { _initCallback = v; }

	void dump(std::ostream& os);

	virtual uint64_t getTotal() { return _total; }

  private:
	void		thread_cb();
	static void read_cb(evutil_socket_t sock, short which, void* args);

  private:
	evutil_socket_t				 _read;
	evutil_socket_t				 _write;
	struct event_base*			 _base;
	struct event*				 _event;
	std::thread*				 _thread;
	RWMutex						 _mutex;
	std::list<callback>			 _callbacks;
	std::string					 _name;
	initCallback				 _initCallback;
	std::map<std::string, void*> _datas;
	bool						 _working;
	bool						 _start;
	uint64_t					 _total;
};

class FoxThreadPool : public IFoxThread {
  public:
	typedef std::shared_ptr<FoxThreadPool> ptr;
	typedef IFoxThread::callback		   callback;
	FoxThreadPool(uint32_t size, const std::string& name = "", bool advance = false);
	~FoxThreadPool();
	void start();
	void stop();
	void join();
	// 随机线程执行
	bool dispatch(callback cb);
	bool batchDispatch(const std::vector<callback>& cb);
	// 指定线程执行
	bool	   dispatch(uint32_t id, callback cb);
	FoxThread* getRandFoxThread();

	void setInitCb(FoxThread::initCallback v) { _initCallback = v; }

	void dump(std::ostream& os);
	void broadcast(callback cb);

	virtual uint64_t getTotal() { return _total; }

  private:
	void releaseFoxThread(FoxThread* t);
	void check();
	void wrapcb(std::shared_ptr<FoxThread>, callback cb);

  private:
	uint32_t				_size;
	uint32_t				_cur;
	std::string				_name;
	bool					_advance;
	bool					_start;
	RWMutex					_mutex;
	std::list<callback>		_callbacks;
	std::vector<FoxThread*> _threads;
	std::list<FoxThread*>	_freeFoxThreads;
	FoxThread::initCallback _initCallback;
	uint64_t				_total;
};

class FoxThreadManager {
  public:
	typedef IFoxThread::callback callback;
	void						 dispatch(const std::string& name, callback cb);
	void						 dispatch(const std::string& name, uint32_t id, callback cb);
	void						 batchDispatch(const std::string& name, const std::vector<callback>& cbs);
	void						 broadcast(const std::string& name, callback cb);
	void						 dumpFoxThreadStatus(std::ostream& os);
	void						 init();
	void						 start();
	void						 stop();
	IFoxThread::ptr				 get(const std::string& name);
	void						 add(const std::string& name, IFoxThread::ptr thr);

  private:
	std::map<std::string, IFoxThread::ptr> _threads;
};

typedef Singleton<FoxThreadManager> FoxThreadMgr;
}  // namespace azzato
