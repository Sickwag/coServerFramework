#pragma once

#include "db/fox_thread.h"
#include "mutex.h"
#include "utils/singleton.h"
#include <adapters/libevent.h>
#include <format>
#include <hircluster.h>
#include <hiredis.h>
#include <memory>
#include <stdlib.h>
#include <string>
#include <sys/time.h>

namespace azzato {

using ReplyPtr = std::shared_ptr<redisReply>;

class IRedis {
  public:
	enum Type {
		REDIS			  = 1,
		REDIS_CLUSTER	  = 2,
		FOX_REDIS		  = 3,
		FOX_REDIS_CLUSTER = 4
	};

	using ptr = std::shared_ptr<IRedis>;

	IRedis()
		: _logEnable(true) {}

	virtual ~IRedis() {}

	virtual ReplyPtr cmd(const std::string& command)		   = 0;
	virtual ReplyPtr cmd(const std::vector<std::string>& argv) = 0;

	const std::string& getName() const { return _name; }

	void setName(const std::string& v) { _name = v; }

	const std::string& getPasswd() const { return _passwd; }

	void setPasswd(const std::string& v) { _passwd = v; }

	Type getType() const { return _type; }

  protected:
	std::string _name;
	std::string _passwd;
	Type		_type;
	bool		_logEnable;
};

class ISyncRedis : public IRedis {
  public:
	using ptr = std::shared_ptr<ISyncRedis>;

	virtual ~ISyncRedis() {}

	virtual bool reconnect()											   = 0;
	virtual bool connect(const std::string& ip, int port, uint64_t ms = 0) = 0;
	virtual bool connect()												   = 0;
	virtual bool setTimeout(uint64_t ms)								   = 0;

	virtual int appendCmd(const std::string& command)					   = 0;
	virtual int appendCmd(const std::vector<std::string>& argv)			   = 0;

	virtual ReplyPtr getReply()											   = 0;

	uint64_t getLastActiveTime() const { return _lastActiveTime; }

	void setLastActiveTime(uint64_t v) { _lastActiveTime = v; }

  protected:
	uint64_t _lastActiveTime;
};

class Redis : public ISyncRedis {
  public:
	using ptr = std::shared_ptr<Redis>;
	Redis();
	Redis(const std::map<std::string, std::string>& conf);

	virtual bool reconnect();
	virtual bool connect(const std::string& ip, int port, uint64_t ms = 0);
	virtual bool connect();
	virtual bool setTimeout(uint64_t ms);

	template <typename... Args>
	ReplyPtr cmd(std::format_string<Args...> fmt, Args&&... args) {
		return cmd(std::vformat(fmt.get(), std::make_format_args(std::forward<Args>(args)...)));
	}

	ReplyPtr cmd(const std::string& command);
	ReplyPtr cmd(const std::vector<std::string>& argv);

	template <typename... Args>
	int appendCmd(std::format_string<Args...> fmt, Args&&... args) {
		return appendCmd(std::vformat(fmt.get(), std::make_format_args(std::forward<Args>(args)...)));
	}

	int appendCmd(const std::string& command);
	int appendCmd(const std::vector<std::string>& argv);

	virtual ReplyPtr getReply();

  private:
	std::string					  _host;
	uint32_t					  _port;
	uint32_t					  _connectMs;
	struct timeval				  _cmdTimeout;
	std::shared_ptr<redisContext> _context;
};

class RedisCluster : public ISyncRedis {
  public:
	using ptr = std::shared_ptr<RedisCluster>;
	RedisCluster();
	RedisCluster(const std::map<std::string, std::string>& conf);

	virtual bool reconnect();
	virtual bool connect(const std::string& ip, int port, uint64_t ms = 0);
	virtual bool connect();
	virtual bool setTimeout(uint64_t ms);

	template <typename... Args>
	ReplyPtr cmd(std::format_string<Args...> fmt, Args&&... args) {
		return cmd(std::vformat(fmt.get(), std::make_format_args(std::forward<Args>(args)...)));
	}

	ReplyPtr cmd(const std::string& command);
	ReplyPtr cmd(const std::vector<std::string>& argv);

	template <typename... Args>
	int appendCmd(std::format_string<Args...> fmt, Args&&... args) {
		return appendCmd(std::vformat(fmt.get(), std::make_format_args(std::forward<Args>(args)...)));
	}

	int appendCmd(const std::string& command);
	int appendCmd(const std::vector<std::string>& argv);

	virtual ReplyPtr getReply();

  private:
	std::string							 _host;
	uint32_t							 _port;
	uint32_t							 _connectMs;
	struct timeval						 _cmdTimeout;
	std::shared_ptr<redisClusterContext> _context;
};

class FoxRedis : public IRedis {
  public:
	using ptr = std::shared_ptr<FoxRedis>;

	enum STATUS {
		UNCONNECTED = 0,
		CONNECTING	= 1,
		CONNECTED	= 2
	};

	enum RESULT {
		OK			= 0,
		TIME_OUT	= 1,
		CONNECT_ERR = 2,
		CMD_ERR		= 3,
		REPLY_NULL	= 4,
		REPLY_ERR	= 5,
		INIT_ERR	= 6
	};

	FoxRedis(azzato::FoxThread* thr, const std::map<std::string, std::string>& conf);
	~FoxRedis();

	template <typename... Args>
	ReplyPtr cmd(std::format_string<Args...> fmt, Args&&... args) {
		return cmd(std::vformat(fmt.get(), std::make_format_args(std::forward<Args>(args)...)));
	}

	ReplyPtr cmd(const std::string& command);
	ReplyPtr cmd(const std::vector<std::string>& argv);

	bool init();

	int getCtxCount() const { return _ctxCount; }

  private:
	static void OnAuthCb(redisAsyncContext* c, void* rp, void* priv);

  private:
	struct FCtx {
		std::string		   cmd;
		azzato::Scheduler* scheduler;
		azzato::Fiber::ptr fiber;
		ReplyPtr		   rpy;
	};

	struct Ctx {
		using ptr = std::shared_ptr<Ctx>;

		event*		ev;
		bool		timeout;
		FoxRedis*	rds;
		std::string cmd;
		FCtx*		fctx;
		// std::vector<std::string> parts;
		// azzato::Scheduler* scheduler;
		// azzato::Fiber::ptr fiber;
		// ReplyPtr rpy;
		FoxThread* thread;

		// Ctx::ptr ref;

		Ctx(FoxRedis* rds);
		~Ctx();
		bool		init();
		void		cancelEvent();
		static void EventCb(int fd, short event, void* d);
	};

  private:
	virtual void pcmd(FCtx* ctx);
	bool		 pinit();
	void		 delayDelete(redisAsyncContext* c);

  private:
	static void ConnectCb(const redisAsyncContext* c, int status);
	static void DisconnectCb(const redisAsyncContext* c, int status);
	static void CmdCb(redisAsyncContext* c, void* r, void* privdata);
	static void TimeCb(int fd, short event, void* d);

  private:
	azzato::FoxThread*				   _thread;
	std::shared_ptr<redisAsyncContext> _context;
	std::string						   _host;
	uint16_t						   _port;
	STATUS							   _status;
	int								   _ctxCount;

	struct timeval _cmdTimeout;
	std::string	   _err;
	struct event*  _event;
};

class FoxRedisCluster : public IRedis {
  public:
	using ptr = std::shared_ptr<FoxRedisCluster>;

	enum STATUS {
		UNCONNECTED = 0,
		CONNECTING	= 1,
		CONNECTED	= 2
	};

	enum RESULT {
		OK			= 0,
		TIME_OUT	= 1,
		CONNECT_ERR = 2,
		CMD_ERR		= 3,
		REPLY_NULL	= 4,
		REPLY_ERR	= 5,
		INIT_ERR	= 6
	};

	FoxRedisCluster(azzato::FoxThread* thr, const std::map<std::string, std::string>& conf);
	~FoxRedisCluster();

	template <typename... Args>
	ReplyPtr cmd(std::format_string<Args...> fmt, Args&&... args) {
		return cmd(std::vformat(fmt.get(), std::make_format_args(std::forward<Args>(args)...)));
	}

	ReplyPtr cmd(const std::string& command);
	ReplyPtr cmd(const std::vector<std::string>& argv);

	int getCtxCount() const { return _ctxCount; }

	bool init();

  private:
	struct FCtx {
		std::string		   cmd;
		azzato::Scheduler* scheduler;
		azzato::Fiber::ptr fiber;
		ReplyPtr		   rpy;
	};

	struct Ctx {
		using ptr = std::shared_ptr<Ctx>;

		event*			 ev;
		bool			 timeout;
		FoxRedisCluster* rds;
		FCtx*			 fctx;
		std::string		 cmd;
		// std::vector<std::string> parts;
		FoxThread* thread;
		// int cancel_count;
		// int destroy;
		// int callback_count;
		// azzato::RWMutex mutex;

		// Ctx::ptr ref;
		// Ctx::ptr tref;
		void cancelEvent();

		Ctx(FoxRedisCluster* rds);
		~Ctx();
		bool		init();
		static void EventCb(int fd, short event, void* d);
	};

  private:
	virtual void pcmd(FCtx* ctx);
	bool		 pinit();
	void		 delayDelete(redisAsyncContext* c);
	static void	 OnAuthCb(redisClusterAsyncContext* c, void* rp, void* priv);

  private:
	static void ConnectCb(const redisAsyncContext* c, int status);
	static void DisconnectCb(const redisAsyncContext* c, int status);
	static void CmdCb(redisClusterAsyncContext* c, void* r, void* privdata);
	static void TimeCb(int fd, short event, void* d);

  private:
	azzato::FoxThread*						  _thread;
	std::shared_ptr<redisClusterAsyncContext> _context;
	std::string								  _host;
	STATUS									  _status;
	int										  _ctxCount;

	struct timeval _cmdTimeout;
	std::string	   _err;
	struct event*  _event;
};

class RedisManager {
  public:
	RedisManager();
	IRedis::ptr get(const std::string& name);

	std::ostream& dump(std::ostream& os);

  private:
	void freeRedis(IRedis* r);
	void init();

  private:
	azzato::RWMutex											  _mutex;
	std::map<std::string, std::list<IRedis*>>				  _datas;
	std::map<std::string, std::map<std::string, std::string>> _config;
};

using RedisMgr = azzato::Singleton<RedisManager>;

class RedisUtil {
  public:
	template <typename... Args>
	static ReplyPtr Cmd(const std::string& name, std::format_string<Args...> fmt, Args&&... args) {
		auto rds = RedisMgr::getInstance()->get(name);
		if(!rds)
			return nullptr;
		return rds->cmd(std::vformat(fmt.get(), std::make_format_args(std::forward<Args>(args)...)));
	}

	static ReplyPtr Cmd(const std::string& name, const std::vector<std::string>& args);

	template <typename... Args>
	static ReplyPtr
	TryCmd(const std::string& name, uint32_t count, std::format_string<Args...> fmt, Args&&... args) {
		for(uint32_t i = 0; i < count; ++i) {
			auto rt = Cmd(name, std::vformat(fmt.get(), std::make_format_args(args...)));
			if(rt)
				return rt;
		}
		return nullptr;
	}

	static ReplyPtr TryCmd(const std::string& name, uint32_t count, const std::vector<std::string>& args);
};

}  // namespace azzato
