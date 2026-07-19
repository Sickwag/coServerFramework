#include "db/redis.h"
#include "scheduler.h"
#include "utils/config.h"
#include "utils/hash_util.h"
#include "log.h"
#include "utils/macro.h"

namespace azzato {

static ConfigVar<std::map<std::string, std::map<std::string, std::string>>>::ptr g_redis =
	Config::lookup("redis.config",
				   std::map<std::string, std::map<std::string, std::string>>(),
				   "redis config");

static std::string
get_value(const std::map<std::string, std::string>& m, const std::string& key, const std::string& def = "") {
	auto it = m.find(key);
	return it == m.end() ? def : it->second;
}

redisReply* RedisReplyClone(redisReply* r) {
	redisReply* c = (redisReply*)calloc(1, sizeof(*c));
	c->type		  = r->type;

	switch(r->type) {
	case REDIS_REPLY_INTEGER:
		c->integer = r->integer;
		break;
	case REDIS_REPLY_ARRAY:
		if(r->element != NULL && r->elements > 0) {
			c->element	= (redisReply**)calloc(r->elements, sizeof(r));
			c->elements = r->elements;
			for(size_t i = 0; i < r->elements; ++i) {
				c->element[i] = RedisReplyClone(r->element[i]);
			}
		}
		break;
	case REDIS_REPLY_ERROR:
	case REDIS_REPLY_STATUS:
	case REDIS_REPLY_STRING:
		if(r->str == NULL) {
			c->str = NULL;
		} else {
			// c->str = strndup(r->str, r->len);
			c->str = (char*)malloc(r->len + 1);
			memcpy(c->str, r->str, r->len);
			c->str[r->len] = '\0';
		}
		c->len = r->len;
		break;
	}
	return c;
}

Redis::Redis() { _type = IRedis::REDIS; }

Redis::Redis(const std::map<std::string, std::string>& conf) {
	_type	   = IRedis::REDIS;
	auto tmp   = get_value(conf, "host");
	auto pos   = tmp.find(":");
	_host	   = tmp.substr(0, pos);
	_port	   = TypeUtil::atoi(tmp.substr(pos + 1));
	_passwd	   = get_value(conf, "passwd");
	_logEnable = TypeUtil::atoi(get_value(conf, "log_enable", "1"));

	tmp		   = get_value(conf, "timeout_com");
	if(tmp.empty()) {
		tmp = get_value(conf, "timeout");
	}
	uint64_t v			= TypeUtil::atoi(tmp);

	_cmdTimeout.tv_sec	= v / 1000;
	_cmdTimeout.tv_usec = v % 1000 * 1000;
}

bool Redis::reconnect() { return redisReconnect(_context.get()); }

bool Redis::connect() { return connect(_host, _port, 50); }

bool Redis::connect(const std::string& ip, int port, uint64_t ms) {
	_host	   = ip;
	_port	   = port;
	_connectMs = ms;
	if(_context) {
		return true;
	}
	timeval tv = {(int)ms / 1000, (int)ms % 1000 * 1000};
	auto	c  = redisConnectWithTimeout(ip.c_str(), port, tv);
	if(c) {
		if(_cmdTimeout.tv_sec || _cmdTimeout.tv_usec) {
			setTimeout(_cmdTimeout.tv_sec * 1000 + _cmdTimeout.tv_usec / 1000);
		}
		_context.reset(c, redisFree);

		if(!_passwd.empty()) {
			auto r = (redisReply*)redisCommand(c, "auth %s", _passwd.c_str());
			if(!r) {
				AZZATO_LOG_ERROR(systemLogger)
					<< "auth error:(" << _host << ":" << _port << ", " << _name << ")";
				return false;
			}
			if(r->type != REDIS_REPLY_STATUS) {
				AZZATO_LOG_ERROR(systemLogger) << "auth reply type error:" << r->type << "(" << _host << ":"
											   << _port << ", " << _name << ")";
				return false;
			}
			if(!r->str) {
				AZZATO_LOG_ERROR(systemLogger)
					<< "auth reply str error: NULL(" << _host << ":" << _port << ", " << _name << ")";
				return false;
			}
			if(strcmp(r->str, "OK") == 0) {
				return true;
			} else {
				AZZATO_LOG_ERROR(systemLogger)
					<< "auth error: " << r->str << "(" << _host << ":" << _port << ", " << _name << ")";
				return false;
			}
		}
		return true;
	}
	return false;
}

bool Redis::setTimeout(uint64_t v) {
	_cmdTimeout.tv_sec	= v / 1000;
	_cmdTimeout.tv_usec = v % 1000 * 1000;
	redisSetTimeout(_context.get(), _cmdTimeout);
	return true;
}

ReplyPtr Redis::cmd(const std::string& command) {
	auto r = (redisReply*)redisCommand(_context.get(), "%s", command.c_str());
	if(!r) {
		if(_logEnable) {
			AZZATO_LOG_ERROR(systemLogger) << "redisCommand error: (" << command << ")(" << _host << ":"
										   << _port << ")(" << _name << ")";
		}
		return nullptr;
	}
	ReplyPtr rt(r, freeReplyObject);
	if(r->type != REDIS_REPLY_ERROR) {
		return rt;
	}
	if(_logEnable) {
		AZZATO_LOG_ERROR(systemLogger) << "redisCommand error: (" << command << ")(" << _host << ":" << _port
									   << ")(" << _name << ")" << ": " << r->str;
	}
	return nullptr;
}

ReplyPtr Redis::cmd(const std::vector<std::string>& argv) {
	std::vector<const char*> v;
	std::vector<size_t>		 l;
	for(auto& i : argv) {
		v.push_back(i.c_str());
		l.push_back(i.size());
	}

	auto r = (redisReply*)redisCommandArgv(_context.get(), argv.size(), &v[0], &l[0]);
	if(!r) {
		if(_logEnable) {
			AZZATO_LOG_ERROR(systemLogger)
				<< "redisCommandArgv error: (" << _host << ":" << _port << ")(" << _name << ")";
		}
		return nullptr;
	}
	ReplyPtr rt(r, freeReplyObject);
	if(r->type != REDIS_REPLY_ERROR) {
		return rt;
	}
	if(_logEnable) {
		AZZATO_LOG_ERROR(systemLogger)
			<< "redisCommandArgv error: (" << _host << ":" << _port << ")(" << _name << ")" << r->str;
	}
	return nullptr;
}

ReplyPtr Redis::getReply() {
	redisReply* r = nullptr;
	if(redisGetReply(_context.get(), (void**)&r) == REDIS_OK) {
		ReplyPtr rt(r, freeReplyObject);
		return rt;
	}
	if(_logEnable) {
		AZZATO_LOG_ERROR(systemLogger)
			<< "redisGetReply error: (" << _host << ":" << _port << ")(" << _name << ")";
	}
	return nullptr;
}

int Redis::appendCmd(const std::string& command) {
	return redisAppendCommand(_context.get(), "%s", command.c_str());
}

int Redis::appendCmd(const std::vector<std::string>& argv) {
	std::vector<const char*> v;
	std::vector<size_t>		 l;
	for(auto& i : argv) {
		v.push_back(i.c_str());
		l.push_back(i.size());
	}
	return redisAppendCommandArgv(_context.get(), argv.size(), &v[0], &l[0]);
}

RedisCluster::RedisCluster() { _type = IRedis::REDIS_CLUSTER; }

RedisCluster::RedisCluster(const std::map<std::string, std::string>& conf) {
	_type	   = IRedis::REDIS_CLUSTER;
	_host	   = get_value(conf, "host");
	_passwd	   = get_value(conf, "passwd");
	_logEnable = TypeUtil::atoi(get_value(conf, "log_enable", "1"));
	auto tmp   = get_value(conf, "timeout_com");
	if(tmp.empty()) {
		tmp = get_value(conf, "timeout");
	}
	uint64_t v			= TypeUtil::atoi(tmp);

	_cmdTimeout.tv_sec	= v / 1000;
	_cmdTimeout.tv_usec = v % 1000 * 1000;
}

////RedisCluster
bool RedisCluster::reconnect() {
	return true;
	// return redisReconnect(_context.get());
}

bool RedisCluster::connect() { return connect(_host, _port, 50); }

bool RedisCluster::connect(const std::string& ip, int port, uint64_t ms) {
	_host	   = ip;
	_port	   = port;
	_connectMs = ms;
	if(_context) {
		return true;
	}
	timeval tv = {(int)ms / 1000, (int)ms % 1000 * 1000};
	auto	c  = redisClusterConnectWithTimeout(ip.c_str(), tv, 0);
	if(c) {
		_context.reset(c, redisClusterFree);
		if(!_passwd.empty()) {
			auto r = (redisReply*)redisClusterCommand(c, "auth %s", _passwd.c_str());
			if(!r) {
				AZZATO_LOG_ERROR(systemLogger)
					<< "auth error:(" << _host << ":" << _port << ", " << _name << ")";
				return false;
			}
			if(r->type != REDIS_REPLY_STATUS) {
				AZZATO_LOG_ERROR(systemLogger) << "auth reply type error:" << r->type << "(" << _host << ":"
											   << _port << ", " << _name << ")";
				return false;
			}
			if(!r->str) {
				AZZATO_LOG_ERROR(systemLogger)
					<< "auth reply str error: NULL(" << _host << ":" << _port << ", " << _name << ")";
				return false;
			}
			if(strcmp(r->str, "OK") == 0) {
				return true;
			} else {
				AZZATO_LOG_ERROR(systemLogger)
					<< "auth error: " << r->str << "(" << _host << ":" << _port << ", " << _name << ")";
				return false;
			}
		}
		return true;
	}
	return false;
}

bool RedisCluster::setTimeout(uint64_t ms) {
	// timeval tv = {(int)ms / 1000, (int)ms % 1000 * 1000};
	// redisSetTimeout(_context.get(), tv);
	return true;
}

ReplyPtr RedisCluster::cmd(const std::string& command) {
	auto r = (redisReply*)redisClusterCommand(_context.get(), "%s", command.c_str());
	if(!r) {
		if(_logEnable) {
			AZZATO_LOG_ERROR(systemLogger) << "redisCommand error: (" << command << ")(" << _host << ":"
										   << _port << ")(" << _name << ")";
		}
		return nullptr;
	}
	ReplyPtr rt(r, freeReplyObject);
	if(r->type != REDIS_REPLY_ERROR) {
		return rt;
	}
	if(_logEnable) {
		AZZATO_LOG_ERROR(systemLogger) << "redisCommand error: (" << command << ")(" << _host << ":" << _port
									   << ")(" << _name << ")" << ": " << r->str;
	}
	return nullptr;
}

ReplyPtr RedisCluster::cmd(const std::vector<std::string>& argv) {
	std::vector<const char*> v;
	std::vector<size_t>		 l;
	for(auto& i : argv) {
		v.push_back(i.c_str());
		l.push_back(i.size());
	}

	auto r = (redisReply*)redisClusterCommandArgv(_context.get(), argv.size(), &v[0], &l[0]);
	if(!r) {
		if(_logEnable) {
			AZZATO_LOG_ERROR(systemLogger)
				<< "redisCommandArgv error: (" << _host << ":" << _port << ")(" << _name << ")";
		}
		return nullptr;
	}
	ReplyPtr rt(r, freeReplyObject);
	if(r->type != REDIS_REPLY_ERROR) {
		return rt;
	}
	if(_logEnable) {
		AZZATO_LOG_ERROR(systemLogger)
			<< "redisCommandArgv error: (" << _host << ":" << _port << ")(" << _name << ")" << r->str;
	}
	return nullptr;
}

ReplyPtr RedisCluster::getReply() {
	redisReply* r = nullptr;
	if(redisClusterGetReply(_context.get(), (void**)&r) == REDIS_OK) {
		ReplyPtr rt(r, freeReplyObject);
		return rt;
	}
	if(_logEnable) {
		AZZATO_LOG_ERROR(systemLogger)
			<< "redisGetReply error: (" << _host << ":" << _port << ")(" << _name << ")";
	}
	return nullptr;
}

int RedisCluster::appendCmd(const std::string& command) {
	return redisClusterAppendCommand(_context.get(), "%s", command.c_str());
}

int RedisCluster::appendCmd(const std::vector<std::string>& argv) {
	std::vector<const char*> v;
	std::vector<size_t>		 l;
	for(auto& i : argv) {
		v.push_back(i.c_str());
		l.push_back(i.size());
	}
	return redisClusterAppendCommandArgv(_context.get(), argv.size(), &v[0], &l[0]);
}

FoxRedis::FoxRedis(FoxThread* thr, const std::map<std::string, std::string>& conf)
	: _thread(thr)
	, _status(UNCONNECTED)
	, _event(nullptr) {
	_type	   = IRedis::FOX_REDIS;
	auto tmp   = get_value(conf, "host");
	auto pos   = tmp.find(":");
	_host	   = tmp.substr(0, pos);
	_port	   = TypeUtil::atoi(tmp.substr(pos + 1));
	_passwd	   = get_value(conf, "passwd");
	_ctxCount  = 0;
	_logEnable = TypeUtil::atoi(get_value(conf, "log_enable", "1"));

	tmp		   = get_value(conf, "timeout_com");
	if(tmp.empty()) {
		tmp = get_value(conf, "timeout");
	}
	uint64_t v			= TypeUtil::atoi(tmp);

	_cmdTimeout.tv_sec	= v / 1000;
	_cmdTimeout.tv_usec = v % 1000 * 1000;
}

void FoxRedis::OnAuthCb(redisAsyncContext* c, void* rp, void* priv) {
	FoxRedis*	fr = (FoxRedis*)priv;
	redisReply* r  = (redisReply*)rp;
	if(!r) {
		AZZATO_LOG_ERROR(systemLogger)
			<< "auth error:(" << fr->_host << ":" << fr->_port << ", " << fr->_name << ")";
		return;
	}
	if(r->type != REDIS_REPLY_STATUS) {
		AZZATO_LOG_ERROR(systemLogger) << "auth reply type error:" << r->type << "(" << fr->_host << ":"
									   << fr->_port << ", " << fr->_name << ")";
		return;
	}
	if(!r->str) {
		AZZATO_LOG_ERROR(systemLogger)
			<< "auth reply str error: NULL(" << fr->_host << ":" << fr->_port << ", " << fr->_name << ")";
		return;
	}
	if(strcmp(r->str, "OK") == 0) {
		AZZATO_LOG_INFO(systemLogger)
			<< "auth ok: " << r->str << "(" << fr->_host << ":" << fr->_port << ", " << fr->_name << ")";
	} else {
		AZZATO_LOG_ERROR(systemLogger)
			<< "auth error: " << r->str << "(" << fr->_host << ":" << fr->_port << ", " << fr->_name << ")";
	}
}

void FoxRedis::ConnectCb(const redisAsyncContext* c, int status) {
	FoxRedis* ar = static_cast<FoxRedis*>(c->data);
	if(!status) {
		AZZATO_LOG_INFO(systemLogger)
			<< "FoxRedis::ConnectCb " << c->c.tcp.host << ":" << c->c.tcp.port << " success";
		ar->_status = CONNECTED;
		if(!ar->_passwd.empty()) {
			int rt =
				redisAsyncCommand(ar->_context.get(), FoxRedis::OnAuthCb, ar, "auth %s", ar->_passwd.c_str());
			if(rt) {
				AZZATO_LOG_ERROR(systemLogger) << "FoxRedis Auth fail: " << rt;
			}
		}

	} else {
		AZZATO_LOG_ERROR(systemLogger) << "FoxRedis::ConnectCb " << c->c.tcp.host << ":" << c->c.tcp.port
									   << " fail, error:" << c->errstr;
		ar->_status = UNCONNECTED;
	}
}

void FoxRedis::DisconnectCb(const redisAsyncContext* c, int status) {
	AZZATO_LOG_INFO(systemLogger) << "FoxRedis::DisconnectCb " << c->c.tcp.host << ":" << c->c.tcp.port
								  << " status:" << status;
	FoxRedis* ar = static_cast<FoxRedis*>(c->data);
	ar->_status	 = UNCONNECTED;
}

void FoxRedis::CmdCb(redisAsyncContext* ac, void* r, void* privdata) {
	Ctx* ctx = static_cast<Ctx*>(privdata);
	if(!ctx) {
		return;
	}
	if(ctx->timeout) {
		delete ctx;
		// if(ctx && ctx->fiber) {
		//     AZZATO_LOG_ERROR(systemLogger) << "redis cmd: '" << ctx->cmd << "' timeout("
		//                 << (ctx->rds->_cmdTimeout.tv_sec * 1000
		//                         + ctx->rds->_cmdTimeout.tv_usec / 1000)
		//                 << "ms)";
		//     ctx->scheduler->schedule(ctx->fiber);
		//     ctx->cancelEvent();
		// }
		return;
	}

	auto _logEnable	  = ctx->rds->_logEnable;

	redisReply* reply = (redisReply*)r;
	if(ac->err) {
		if(_logEnable) {
			replace(ctx->cmd, "\r\n", "\\r\\n");
			AZZATO_LOG_ERROR(systemLogger)
				<< "redis cmd: '" << ctx->cmd << "' " << "(" << ac->err << ") " << ac->errstr;
		}
		if(ctx->fctx->fiber) {
			ctx->fctx->scheduler->schedule(ctx->fctx->fiber);
		}
	} else if(!reply) {
		if(_logEnable) {
			replace(ctx->cmd, "\r\n", "\\r\\n");
			AZZATO_LOG_ERROR(systemLogger) << "redis cmd: '" << ctx->cmd << "' " << "reply: NULL";
		}
		if(ctx->fctx->fiber) {
			ctx->fctx->scheduler->schedule(ctx->fctx->fiber);
		}
	} else if(reply->type == REDIS_REPLY_ERROR) {
		if(_logEnable) {
			replace(ctx->cmd, "\r\n", "\\r\\n");
			AZZATO_LOG_ERROR(systemLogger) << "redis cmd: '" << ctx->cmd << "' " << "reply: " << reply->str;
		}
		if(ctx->fctx->fiber) {
			ctx->fctx->scheduler->schedule(ctx->fctx->fiber);
		}
	} else {
		if(ctx->fctx->fiber) {
			ctx->fctx->rpy.reset(RedisReplyClone(reply), freeReplyObject);
			ctx->fctx->scheduler->schedule(ctx->fctx->fiber);
		}
	}
	ctx->cancelEvent();
	delete ctx;
}

void FoxRedis::TimeCb(int fd, short event, void* d) {
	FoxRedis* ar = static_cast<FoxRedis*>(d);
	redisAsyncCommand(ar->_context.get(), CmdCb, nullptr, "ping");
}

struct Res {
	redisAsyncContext* ctx;
	struct event*	   event;
};

// void DelayTimeCb(int fd, short event, void* d) {
//     AZZATO_LOG_INFO(systemLogger) << "DelayTimeCb";
//     Res* res = static_cast<Res*>(d);
//     redisAsyncFree(res->ctx);
//     evtimer_del(res->event);
//     event_free(res->event);
//     delete res;
// }

bool FoxRedis::init() {
	if(_thread == FoxThread::GetThis()) {
		return pinit();
	} else {
		_thread->dispatch(std::bind(&FoxRedis::pinit, this));
	}
	return true;
}

void FoxRedis::delayDelete(redisAsyncContext* c) {
	// if(!c) {
	//     return;
	// }

	// Res* res = new Res();
	// res->ctx = c;
	// struct event* event = event_new(_thread->getBase(), -1, EV_TIMEOUT, DelayTimeCb, res);
	// res->event = event;
	//
	// struct timeval tv = {60, 0};
	// evtimer_add(event, &tv);
}

bool FoxRedis::pinit() {
	// AZZATO_LOG_INFO(systemLogger) << "pinit _status=" << _status;
	if(_status != UNCONNECTED) {
		return true;
	}
	auto ctx = redisAsyncConnect(_host.c_str(), _port);
	if(!ctx) {
		AZZATO_LOG_ERROR(systemLogger) << "redisAsyncConnect (" << _host << ":" << _port << ") null";
		return false;
	}
	if(ctx->err) {
		AZZATO_LOG_ERROR(systemLogger) << "Error:(" << ctx->err << ")" << ctx->errstr;
		return false;
	}
	ctx->data = this;
	redisLibeventAttach(ctx, _thread->getBase());
	redisAsyncSetConnectCallback(ctx, ConnectCb);
	redisAsyncSetDisconnectCallback(ctx, DisconnectCb);
	_status = CONNECTING;
	// _context.reset(ctx, redisAsyncFree);
	_context.reset(ctx, nop<redisAsyncContext>);
	// _context.reset(ctx, std::bind(&FoxRedis::delayDelete, this, std::placeholders::_1));
	if(_event == nullptr) {
		_event			  = event_new(_thread->getBase(), -1, EV_TIMEOUT | EV_PERSIST, TimeCb, this);
		struct timeval tv = {120, 0};
		evtimer_add(_event, &tv);
	}
	TimeCb(0, 0, this);
	return true;
}

ReplyPtr FoxRedis::cmd(const std::string& command) {
	FCtx fctx;
	fctx.cmd	   = command;
	fctx.scheduler = Scheduler::getThis();
	fctx.fiber	   = Fiber::getThis();

	_thread->dispatch(std::bind(&FoxRedis::pcmd, this, &fctx));
	Fiber::yieldToHold();
	return fctx.rpy;
}

ReplyPtr FoxRedis::cmd(const std::vector<std::string>& argv) {
	// Ctx::ptr ctx(new Ctx(this));
	// ctx->parts = argv;
	FCtx fctx;
	do {
		std::vector<const char*> args;
		std::vector<size_t>		 args_len;
		for(auto& i : argv) {
			args.push_back(i.c_str());
			args_len.push_back(i.size());
		}
		char* buf = nullptr;
		int	  len = redisFormatCommandArgv(&buf, argv.size(), &(args[0]), &(args_len[0]));
		if(len == -1 || !buf) {
			AZZATO_LOG_ERROR(systemLogger) << "redis fmt error";
			return nullptr;
		}
		fctx.cmd.append(buf, len);
		free(buf);
	} while(0);

	// ctx->scheduler = Scheduler::getThis();
	// ctx->fiber = Fiber::getThis();
	// ctx->thread = _thread;

	fctx.scheduler = Scheduler::getThis();
	fctx.fiber	   = Fiber::getThis();

	_thread->dispatch(std::bind(&FoxRedis::pcmd, this, &fctx));
	Fiber::yieldToHold();
	return fctx.rpy;
}

void FoxRedis::pcmd(FCtx* fctx) {
	if(_status == UNCONNECTED) {
		AZZATO_LOG_INFO(systemLogger) << "redis (" << _host << ":" << _port << ") unconnected " << fctx->cmd;
		init();
		if(fctx->fiber) {
			fctx->scheduler->schedule(fctx->fiber);
		}
		return;
	}
	Ctx* ctx(new Ctx(this));
	ctx->thread = _thread;
	ctx->init();
	ctx->fctx = fctx;
	ctx->cmd  = fctx->cmd;

	if(!ctx->cmd.empty()) {
		// redisAsyncCommand(_context.get(), CmdCb, ctx.get(), ctx->cmd.c_str());
		redisAsyncFormattedCommand(_context.get(), CmdCb, ctx, ctx->cmd.c_str(), ctx->cmd.size());
		//} else if(!ctx->parts.empty()) {
		//    std::vector<const char*> argv;
		//    std::vector<size_t> argv_len;
		//    for(auto& i : ctx->parts) {
		//        argv.push_back(i.c_str());
		//        argv_len.push_back(i.size());
		//    }
		//    redisAsyncCommandArgv(_context.get(), CmdCb, ctx.get(), argv.size(),
		//            &(argv[0]), &(argv_len[0]));
	}
}

FoxRedis::~FoxRedis() {
	if(_event) {
		evtimer_del(_event);
		event_free(_event);
	}
}

FoxRedis::Ctx::Ctx(FoxRedis* r)
	: ev(nullptr)
	, timeout(false)
	, rds(r)
	//,scheduler(nullptr)
	, thread(nullptr) {
	Atomic::addFetch(rds->_ctxCount, 1);
}

FoxRedis::Ctx::~Ctx() {
	// cancelEvent();
	AZZATO_ASSERT(thread == FoxThread::GetThis());
	// AZZATO_ASSERT(destory == 0);
	Atomic::subFetch(rds->_ctxCount, 1);
	//++destory;
	// cancelEvent();
	if(ev) {
		evtimer_del(ev);
		event_free(ev);
		ev = nullptr;
	}
}

void FoxRedis::Ctx::cancelEvent() {
	// if(ev) {
	//     if(thread == FoxThread::GetThis()) {
	//         evtimer_del(ev);
	//         event_free(ev);
	//     } else {
	//         auto e = ev;
	//         thread->dispatch([e](){
	//             evtimer_del(e);
	//             event_free(e);
	//         });
	//     }
	//     ev = nullptr;
	// }
	// ref = nullptr;
}

bool FoxRedis::Ctx::init() {
	ev = evtimer_new(rds->_thread->getBase(), EventCb, this);
	evtimer_add(ev, &rds->_cmdTimeout);
	return true;
}

void FoxRedis::Ctx::EventCb(int fd, short event, void* d) {
	Ctx* ctx	 = static_cast<Ctx*>(d);
	ctx->timeout = 1;
	if(ctx->rds->_logEnable) {
		replace(ctx->cmd, "\r\n", "\\r\\n");
		AZZATO_LOG_INFO(systemLogger)
			<< "redis cmd: '" << ctx->cmd << "' reach timeout "
			<< (ctx->rds->_cmdTimeout.tv_sec * 1000 + ctx->rds->_cmdTimeout.tv_usec / 1000) << "ms";
	}
	if(ctx->fctx->fiber) {
		ctx->fctx->scheduler->schedule(ctx->fctx->fiber);
	}
	ctx->cancelEvent();
	// ctx->ref = nullptr;
}

FoxRedisCluster::FoxRedisCluster(FoxThread* thr, const std::map<std::string, std::string>& conf)
	: _thread(thr)
	, _status(UNCONNECTED)
	, _event(nullptr) {
	_ctxCount  = 0;

	_type	   = IRedis::FOX_REDIS_CLUSTER;
	_host	   = get_value(conf, "host");
	_passwd	   = get_value(conf, "passwd");
	_logEnable = TypeUtil::atoi(get_value(conf, "log_enable", "1"));
	auto tmp   = get_value(conf, "timeout_com");
	if(tmp.empty()) {
		tmp = get_value(conf, "timeout");
	}
	uint64_t v			= TypeUtil::atoi(tmp);

	_cmdTimeout.tv_sec	= v / 1000;
	_cmdTimeout.tv_usec = v % 1000 * 1000;
}

void FoxRedisCluster::OnAuthCb(redisClusterAsyncContext* c, void* rp, void* priv) {
	FoxRedisCluster* fr = (FoxRedisCluster*)priv;
	redisReply*		 r	= (redisReply*)rp;
	if(!r) {
		AZZATO_LOG_ERROR(systemLogger) << "auth error:(" << fr->_host << ", " << fr->_name << ")";
		return;
	}
	if(r->type != REDIS_REPLY_STATUS) {
		AZZATO_LOG_ERROR(systemLogger)
			<< "auth reply type error:" << r->type << "(" << fr->_host << ", " << fr->_name << ")";
		return;
	}
	if(!r->str) {
		AZZATO_LOG_ERROR(systemLogger)
			<< "auth reply str error: NULL(" << fr->_host << ", " << fr->_name << ")";
		return;
	}
	if(strcmp(r->str, "OK") == 0) {
		AZZATO_LOG_INFO(systemLogger)
			<< "auth ok: " << r->str << "(" << fr->_host << ", " << fr->_name << ")";
	} else {
		AZZATO_LOG_ERROR(systemLogger)
			<< "auth error: " << r->str << "(" << fr->_host << ", " << fr->_name << ")";
	}
}

void FoxRedisCluster::ConnectCb(const redisAsyncContext* c, int status) {
	FoxRedisCluster* ar = static_cast<FoxRedisCluster*>(c->data);
	if(!status) {
		AZZATO_LOG_INFO(systemLogger)
			<< "FoxRedisCluster::ConnectCb " << c->c.tcp.host << ":" << c->c.tcp.port << " success";
		if(!ar->_passwd.empty()) {
			int rt = redisClusterAsyncCommand(
				ar->_context.get(), FoxRedisCluster::OnAuthCb, ar, "auth %s", ar->_passwd.c_str());
			if(rt) {
				AZZATO_LOG_ERROR(systemLogger) << "FoxRedisCluster Auth fail: " << rt;
			}
		}
	} else {
		AZZATO_LOG_ERROR(systemLogger) << "FoxRedisCluster::ConnectCb " << c->c.tcp.host << ":"
									   << c->c.tcp.port << " fail, error:" << c->errstr;
	}
}

void FoxRedisCluster::DisconnectCb(const redisAsyncContext* c, int status) {
	AZZATO_LOG_INFO(systemLogger) << "FoxRedisCluster::DisconnectCb " << c->c.tcp.host << ":" << c->c.tcp.port
								  << " status:" << status;
}

void FoxRedisCluster::CmdCb(redisClusterAsyncContext* ac, void* r, void* privdata) {
	Ctx* ctx = static_cast<Ctx*>(privdata);
	if(ctx->timeout) {
		delete ctx;
		// if(ctx && ctx->fiber) {
		//     AZZATO_LOG_ERROR(systemLogger) << "redis cmd: '" << ctx->cmd << "' timeout("
		//                 << (ctx->rds->_cmdTimeout.tv_sec * 1000
		//                         + ctx->rds->_cmdTimeout.tv_usec / 1000)
		//                 << "ms)";
		//     ctx->scheduler->schedule(ctx->fiber);
		//     ctx->cancelEvent();
		// }
		return;
	}
	auto _logEnable	  = ctx->rds->_logEnable;
	// ctx->cancelEvent();
	redisReply* reply = (redisReply*)r;
	//++ctx->callback_count;
	if(ac->err) {
		if(_logEnable) {
			replace(ctx->cmd, "\r\n", "\\r\\n");
			AZZATO_LOG_ERROR(systemLogger)
				<< "redis cmd: '" << ctx->cmd << "' " << "(" << ac->err << ") " << ac->errstr;
		}
		if(ctx->fctx->fiber) {
			ctx->fctx->scheduler->schedule(ctx->fctx->fiber);
		}
	} else if(!reply) {
		if(_logEnable) {
			replace(ctx->cmd, "\r\n", "\\r\\n");
			AZZATO_LOG_ERROR(systemLogger) << "redis cmd: '" << ctx->cmd << "' " << "reply: NULL";
		}
		if(ctx->fctx->fiber) {
			ctx->fctx->scheduler->schedule(ctx->fctx->fiber);
		}
	} else if(reply->type == REDIS_REPLY_ERROR) {
		if(_logEnable) {
			replace(ctx->cmd, "\r\n", "\\r\\n");
			AZZATO_LOG_ERROR(systemLogger) << "redis cmd: '" << ctx->cmd << "' " << "reply: " << reply->str;
		}
		if(ctx->fctx->fiber) {
			ctx->fctx->scheduler->schedule(ctx->fctx->fiber);
		}
	} else {
		if(ctx->fctx->fiber) {
			ctx->fctx->rpy.reset(RedisReplyClone(reply), freeReplyObject);
			ctx->fctx->scheduler->schedule(ctx->fctx->fiber);
		}
	}
	// ctx->ref = nullptr;
	delete ctx;
	// ctx->tref = nullptr;
}

void FoxRedisCluster::TimeCb(int fd, short event, void* d) {
	// FoxRedisCluster* ar = static_cast<FoxRedisCluster*>(d);
	// redisAsyncCommand(ar->_context.get(), CmdCb, nullptr, "ping");
}

bool FoxRedisCluster::init() {
	if(_thread == FoxThread::GetThis()) {
		return pinit();
	} else {
		_thread->dispatch(std::bind(&FoxRedisCluster::pinit, this));
	}
	return true;
}

void FoxRedisCluster::delayDelete(redisAsyncContext* c) {
	// if(!c) {
	//     return;
	// }

	// Res* res = new Res();
	// res->ctx = c;
	// struct event* event = event_new(_thread->getBase(), -1, EV_TIMEOUT, DelayTimeCb, res);
	// res->event = event;
	//
	// struct timeval tv = {60, 0};
	// evtimer_add(event, &tv);
}

bool FoxRedisCluster::pinit() {
	if(_status != UNCONNECTED) {
		return true;
	}
	AZZATO_LOG_INFO(systemLogger) << "FoxRedisCluster pinit:" << _host;
	auto ctx  = redisClusterAsyncConnect(_host.c_str(), 0);
	ctx->data = this;
	redisClusterLibeventAttach(ctx, _thread->getBase());
	redisClusterAsyncSetConnectCallback(ctx, ConnectCb);
	redisClusterAsyncSetDisconnectCallback(ctx, DisconnectCb);
	if(!ctx) {
		AZZATO_LOG_ERROR(systemLogger) << "redisClusterAsyncConnect (" << _host << ") null";
		return false;
	}
	if(ctx->err) {
		AZZATO_LOG_ERROR(systemLogger)
			<< "Error:(" << ctx->err << ")" << ctx->errstr << " passwd=" << _passwd;
		return false;
	}
	_status = CONNECTED;
	// _context.reset(ctx, redisAsyncFree);
	_context.reset(ctx, nop<redisClusterAsyncContext>);
	// _context.reset(ctx, std::bind(&FoxRedisCluster::delayDelete, this, std::placeholders::_1));
	if(_event == nullptr) {
		_event			  = event_new(_thread->getBase(), -1, EV_TIMEOUT | EV_PERSIST, TimeCb, this);
		struct timeval tv = {120, 0};
		evtimer_add(_event, &tv);
		TimeCb(0, 0, this);
	}
	return true;
}

ReplyPtr FoxRedisCluster::cmd(const std::string& command) {
	FCtx fctx;
	fctx.cmd	   = command;
	fctx.scheduler = Scheduler::getThis();
	fctx.fiber	   = Fiber::getThis();

	_thread->dispatch(std::bind(&FoxRedisCluster::pcmd, this, &fctx));
	Fiber::yieldToHold();
	return fctx.rpy;
}

ReplyPtr FoxRedisCluster::cmd(const std::vector<std::string>& argv) {
	// Ctx::ptr ctx(new Ctx(this));
	// ctx->parts = argv;
	FCtx fctx;
	do {
		std::vector<const char*> args;
		std::vector<size_t>		 args_len;
		for(auto& i : argv) {
			args.push_back(i.c_str());
			args_len.push_back(i.size());
		}
		char* buf = nullptr;
		int	  len = redisFormatCommandArgv(&buf, argv.size(), &(args[0]), &(args_len[0]));
		if(len == -1 || !buf) {
			AZZATO_LOG_ERROR(systemLogger) << "redis fmt error";
			return nullptr;
		}
		fctx.cmd.append(buf, len);
		free(buf);
	} while(0);

	fctx.scheduler = Scheduler::getThis();
	fctx.fiber	   = Fiber::getThis();

	_thread->dispatch(std::bind(&FoxRedisCluster::pcmd, this, &fctx));
	Fiber::yieldToHold();
	return fctx.rpy;
}

void FoxRedisCluster::pcmd(FCtx* fctx) {
	if(_status != CONNECTED) {
		AZZATO_LOG_INFO(systemLogger) << "redis (" << _host << ") unconnected " << fctx->cmd;
		init();
		if(fctx->fiber) {
			fctx->scheduler->schedule(fctx->fiber);
		}
		return;
	}
	Ctx* ctx(new Ctx(this));
	ctx->thread = _thread;
	ctx->init();
	ctx->fctx = fctx;
	ctx->cmd  = fctx->cmd;
	// ctx->ref = ctx;
	// ctx->tref = ctx;
	if(!ctx->cmd.empty()) {
		// redisClusterAsyncCommand(_context.get(), CmdCb, ctx.get(), ctx->cmd.c_str());
		redisClusterAsyncFormattedCommand(_context.get(), CmdCb, ctx, &ctx->cmd[0], ctx->cmd.size());
		//} else if(!ctx->parts.empty()) {
		//    std::vector<const char*> argv;
		//    std::vector<size_t> argv_len;
		//    for(auto& i : ctx->parts) {
		//        argv.push_back(i.c_str());
		//        argv_len.push_back(i.size());
		//    }
		//    redisClusterAsyncCommandArgv(_context.get(), CmdCb, ctx.get(), argv.size(),
		//            &(argv[0]), &(argv_len[0]));
	}
}

FoxRedisCluster::~FoxRedisCluster() {
	if(_event) {
		evtimer_del(_event);
		event_free(_event);
	}
}

FoxRedisCluster::Ctx::Ctx(FoxRedisCluster* r)
	: ev(nullptr)
	, timeout(false)
	, rds(r)
	//,scheduler(nullptr)
	, thread(nullptr) {
	//,cancel_count(0)
	//,destory(0)
	//,callback_count(0) {
	fctx = nullptr;
	Atomic::addFetch(rds->_ctxCount, 1);
}

FoxRedisCluster::Ctx::~Ctx() {
	AZZATO_ASSERT(thread == FoxThread::GetThis());
	// AZZATO_ASSERT(destory == 0);
	Atomic::subFetch(rds->_ctxCount, 1);
	//++destory;
	// cancelEvent();

	if(ev) {
		evtimer_del(ev);
		event_free(ev);
		ev = nullptr;
	}
}

void FoxRedisCluster::Ctx::cancelEvent() {
	// AZZATO_LOG_INFO(systemLogger) << "cancelEvent " << FoxThread::GetThis()
	//            << " - " << thread
	//            << " - " << IOManager::GetThis()
	//            << " - " << cancel_count;
	// if(thread != FoxThread::GetThis()) {
	//     AZZATO_LOG_INFO(systemLogger) << "cancelEvent " << FoxThread::GetThis()
	//                << " - " << thread
	//                << " - " << IOManager::GetThis()
	//                << " - " << cancel_count;

	//    //AZZATO_LOG_INFO(systemLogger) << "cancelEvent thread=" << thread << " " << thread->getId()
	//    //           << " this=" << FoxThread::GetThis();
	//    //AZZATO_ASSERT(thread == FoxThread::GetThis());
	//}
	// AZZATO_ASSERT(!IOManager::GetThis());
	////if(Atomic::addFetch(cancel_count) > 1) {
	////    return;
	////}
	////AZZATO_ASSERT(!Fiber::getThis());
	////RWMutex::WriteLock lock(mutex);
	// if(++cancel_count > 1) {
	//     return;
	// }
	// if(ev) {
	//     auto e = ev;
	//     ev = nullptr;
	//     //lock.unlock();
	//     //evtimer_del(e);
	//     //event_free(e);
	//     if(thread == FoxThread::GetThis()) {
	//         evtimer_del(e);
	//         event_free(e);
	//     } else {
	//         thread->dispatch([e](){
	//             evtimer_del(e);
	//             event_free(e);
	//         });
	//     }
	// }
	// ref = nullptr;
}

bool FoxRedisCluster::Ctx::init() {
	AZZATO_ASSERT(thread == FoxThread::GetThis());
	ev = evtimer_new(rds->_thread->getBase(), EventCb, this);
	evtimer_add(ev, &rds->_cmdTimeout);
	return true;
}

void FoxRedisCluster::Ctx::EventCb(int fd, short event, void* d) {
	Ctx* ctx = static_cast<Ctx*>(d);
	if(!ctx->ev) {
		return;
	}
	ctx->timeout = 1;
	if(ctx->rds->_logEnable) {
		replace(ctx->cmd, "\r\n", "\\r\\n");
		AZZATO_LOG_INFO(systemLogger)
			<< "redis cmd: '" << ctx->cmd << "' reach timeout "
			<< (ctx->rds->_cmdTimeout.tv_sec * 1000 + ctx->rds->_cmdTimeout.tv_usec / 1000) << "ms";
	}
	ctx->cancelEvent();
	if(ctx->fctx->fiber) {
		ctx->fctx->scheduler->schedule(ctx->fctx->fiber);
	}
	// ctx->ref = nullptr;
	// delete ctx;
	// ctx->tref = nullptr;
}

IRedis::ptr RedisManager::get(const std::string& name) {
	RWMutex::WriteLock lock(_mutex);
	auto			   it = _datas.find(name);
	if(it == _datas.end()) {
		return nullptr;
	}
	if(it->second.empty()) {
		return nullptr;
	}
	auto r = it->second.front();
	it->second.pop_front();
	if(r->getType() == IRedis::FOX_REDIS || r->getType() == IRedis::FOX_REDIS_CLUSTER) {
		it->second.push_back(r);
		return std::shared_ptr<IRedis>(r, nop<IRedis>);
	}
	lock.unlock();
	auto rr = dynamic_cast<ISyncRedis*>(r);
	if((time(0) - rr->getLastActiveTime()) > 30) {
		if(!rr->cmd("ping")) {
			if(!rr->reconnect()) {
				RWMutex::WriteLock lock(_mutex);
				_datas[name].push_back(r);
				return nullptr;
			}
		}
	}
	rr->setLastActiveTime(time(0));
	return std::shared_ptr<IRedis>(r, std::bind(&RedisManager::freeRedis, this, std::placeholders::_1));
}

void RedisManager::freeRedis(IRedis* r) {
	RWMutex::WriteLock lock(_mutex);
	_datas[r->getName()].push_back(r);
}

RedisManager::RedisManager() { init(); }

void RedisManager::init() {
	_config		 = g_redis->getValue();
	size_t done	 = 0;
	size_t total = 0;
	for(auto& i : _config) {
		auto type	= get_value(i.second, "type");
		auto pool	= TypeUtil::atoi(get_value(i.second, "pool"));
		auto passwd = get_value(i.second, "passwd");
		total += pool;
		for(int n = 0; n < pool; ++n) {
			if(type == "redis") {
				Redis* rds(new Redis(i.second));
				rds->connect();
				rds->setLastActiveTime(time(0));
				RWMutex::WriteLock lock(_mutex);
				_datas[i.first].push_back(rds);
				Atomic::addFetch(done, 1);
			} else if(type == "redis_cluster") {
				RedisCluster* rds(new RedisCluster(i.second));
				rds->connect();
				rds->setLastActiveTime(time(0));
				RWMutex::WriteLock lock(_mutex);
				_datas[i.first].push_back(rds);
				Atomic::addFetch(done, 1);
			} else if(type == "fox_redis") {
				auto conf = i.second;
				auto name = i.first;
				FoxThreadMgr::getInstance()->dispatch("redis", [this, conf, name, &done]() {
					FoxRedis* rds(new FoxRedis(FoxThread::GetThis(), conf));
					rds->init();
					rds->setName(name);

					RWMutex::WriteLock lock(_mutex);
					_datas[name].push_back(rds);
					Atomic::addFetch(done, 1);
				});
			} else if(type == "fox_redis_cluster") {
				auto conf = i.second;
				auto name = i.first;
				FoxThreadMgr::getInstance()->dispatch("redis", [this, conf, name, &done]() {
					FoxRedisCluster* rds(new FoxRedisCluster(FoxThread::GetThis(), conf));
					rds->init();
					rds->setName(name);

					RWMutex::WriteLock lock(_mutex);
					_datas[name].push_back(rds);
					Atomic::addFetch(done, 1);
				});
			} else {
				Atomic::addFetch(done, 1);
			}
		}
	}

	while(done != total) {
		usleep(5000);
	}
}

std::ostream& RedisManager::dump(std::ostream& os) {
	os << "[RedisManager total=" << _config.size() << "]" << std::endl;
	for(auto& i : _config) {
		os << "    " << i.first << " :[";
		for(auto& n : i.second) {
			os << "{" << n.first << ":" << n.second << "}";
		}
		os << "]" << std::endl;
	}
	return os;
}

ReplyPtr RedisUtil::Cmd(const std::string& name, const std::vector<std::string>& args) {
	auto rds = RedisMgr::getInstance()->get(name);
	if(!rds) {
		return nullptr;
	}
	return rds->cmd(args);
}

ReplyPtr RedisUtil::TryCmd(const std::string& name, uint32_t count, const std::vector<std::string>& args) {
	for(uint32_t i = 0; i < count; ++i) {
		ReplyPtr rt = Cmd(name, args);
		if(rt) {
			return rt;
		}
	}
	return nullptr;
}

}  // namespace azzato
