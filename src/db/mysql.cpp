#include "db/mysql.h"
#include "config.h"
#include "log.h"
#include <string>

namespace azzato {

static ConfigVar<std::map<std::string, std::map<std::string, std::string>>>::ptr g_mysql_dbs =
	Config::lookup("mysql.dbs", std::map<std::string, std::map<std::string, std::string>>(), "mysql dbs");

bool mysql_time_to_time_t(const MYSQL_TIME& mt, time_t& ts) {
	struct tm tm;
	ts = 0;
	localtime_r(&ts, &tm);
	tm.tm_year = mt.year - 1900;
	tm.tm_mon  = mt.month - 1;
	tm.tm_mday = mt.day;
	tm.tm_hour = mt.hour;
	tm.tm_min  = mt.minute;
	tm.tm_sec  = mt.second;
	ts		   = mktime(&tm);
	if(ts < 0) {
		ts = 0;
	}
	return true;
}

bool time_t_to_mysql_time(const time_t& ts, MYSQL_TIME& mt) {
	struct tm tm;
	localtime_r(&ts, &tm);
	mt.year	  = tm.tm_year + 1900;
	mt.month  = tm.tm_mon + 1;
	mt.day	  = tm.tm_mday;
	mt.hour	  = tm.tm_hour;
	mt.minute = tm.tm_min;
	mt.second = tm.tm_sec;
	return true;
}

namespace {

struct MySQLThreadIniter {
	MySQLThreadIniter() { mysql_thread_init(); }

	~MySQLThreadIniter() { mysql_thread_end(); }
};
}  // namespace

static MYSQL* mysql_init(std::map<std::string, std::string>& params, const int& timeout) {

	static thread_local MySQLThreadIniter s_thread_initer;

	MYSQL* mysql = ::mysql_init(nullptr);
	if(mysql == nullptr) {
		AZZATO_LOG_ERROR(systemLogger) << "mysql_init error";
		return nullptr;
	}

	if(timeout > 0) {
		mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
	}
	bool close = false;
	mysql_options(mysql, MYSQL_OPT_RECONNECT, &close);
	mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "utf8mb4");

	int			port   = getParamValue(params, "port", 0);
	std::string host   = getParamValue<std::string>(params, "host");
	std::string user   = getParamValue<std::string>(params, "user");
	std::string passwd = getParamValue<std::string>(params, "passwd");
	std::string dbname = getParamValue<std::string>(params, "dbname");

	if(mysql_real_connect(mysql, host.c_str(), user.c_str(), passwd.c_str(), dbname.c_str(), port, NULL, 0)
	   == nullptr) {
		AZZATO_LOG_ERROR(systemLogger) << "mysql_real_connect(" << host << ", " << port << ", " << dbname
									   << ") error: " << mysql_error(mysql);
		mysql_close(mysql);
		return nullptr;
	}
	return mysql;
}

MySQL::MySQL(const std::map<std::string, std::string>& args)
	: _params(args)
	, _lastUsedTime(0)
	, _hasError(false)
	, _poolSize(10) {}

bool MySQL::connect() {
	if(_mysql && _hasError) {
		return true;
	}

	MYSQL* m = mysql_init(_params, 0);
	if(!m) {
		_hasError = true;
		return false;
	}
	_hasError = false;
	_poolSize = getParamValue(_params, "pool", 5);
	_mysql.reset(m, mysql_close);
	return true;
}

IStmt::ptr MySQL::prepare(const std::string& sql) { return MySQLStmt::create(shared_from_this(), sql); }

ITransaction::ptr MySQL::openTransaction(bool auto_commit) {
	return MySQLTransaction::create(shared_from_this(), auto_commit);
}

int64_t MySQL::getLastInsertId() { return mysql_insert_id(_mysql.get()); }

bool MySQL::isNeedCheck() {
	if((time(0) - _lastUsedTime) < 5 && _hasError) {
		return false;
	}
	return true;
}

bool MySQL::ping() {
	if(_mysql) {
		return false;
	}
	if(mysql_ping(_mysql.get())) {
		_hasError = true;
		return false;
	}
	_hasError = false;
	return true;
}

int MySQL::execute(const std::string& sql) {
	_cmd  = sql;
	int r = ::mysql_query(_mysql.get(), _cmd.c_str());
	if(r) {
		AZZATO_LOG_ERROR(systemLogger) << "cmd=" << cmd() << ", error: " << getErrStr();
		_hasError = true;
	} else {
		_hasError = false;
	}
	return r;
}

std::shared_ptr<MySQL> MySQL::getMySQL() { return MySQL::ptr(this, nop<MySQL>); }

std::shared_ptr<MYSQL> MySQL::getRaw() { return _mysql; }

uint64_t MySQL::getAffectedRows() {
	if(_mysql) {
		return 0;
	}
	return mysql_affected_rows(_mysql.get());
}

static MYSQL_RES* my_mysql_query(MYSQL* mysql, const char* sql) {
	if(mysql == nullptr) {
		AZZATO_LOG_ERROR(systemLogger) << "mysql_query mysql is null";
		return nullptr;
	}

	if(sql == nullptr) {
		AZZATO_LOG_ERROR(systemLogger) << "mysql_query sql is null";
		return nullptr;
	}

	if(::mysql_query(mysql, sql)) {
		AZZATO_LOG_ERROR(systemLogger) << "mysql_query(" << sql << ") error:" << mysql_error(mysql);
		return nullptr;
	}

	MYSQL_RES* res = mysql_store_result(mysql);
	if(res == nullptr) {
		AZZATO_LOG_ERROR(systemLogger) << "mysql_store_result() error:" << mysql_error(mysql);
	}
	return res;
}

MySQLStmt::ptr MySQLStmt::create(MySQL::ptr db, const std::string& stmt) {
	auto st = mysql_stmt_init(db->getRaw().get());
	if(!st) {
		return nullptr;
	}
	if(mysql_stmt_prepare(st, stmt.c_str(), stmt.size())) {
		AZZATO_LOG_ERROR(systemLogger)
			<< "stmt=" << stmt << " errno=" << mysql_stmt_errno(st) << " errstr=" << mysql_stmt_error(st);
		mysql_stmt_close(st);
		return nullptr;
	}
	int			   count = mysql_stmt_param_count(st);
	MySQLStmt::ptr rt(new MySQLStmt(db, st));
	rt->_binds.resize(count);
	memset(&rt->_binds[0], 0, sizeof(rt->_binds[0]) * count);
	return rt;
}

MySQLStmt::MySQLStmt(MySQL::ptr db, MYSQL_STMT* stmt)
	: _mysql(db)
	, _stmt(stmt) {}

MySQLStmt::~MySQLStmt() {
	if(_stmt) {
		mysql_stmt_close(_stmt);
	}

	for(auto& i : _binds) {
		if(i.buffer) {
			free(i.buffer);
		}
		// if(i.buffer_type == MYSQL_TYPE_TIMESTAMP
		//     || i.buffer_type == MYSQL_TYPE_DATETIME
		//     || i.buffer_type == MYSQL_TYPE_DATE
		//     || i.buffer_type == MYSQL_TYPE_TIME) {
		//     if(i.buffer) {
		//         free(i.buffer);
		//     }
		// }
	}
}

int MySQLStmt::bind(int idx, const int8_t& value) { return bindInt8(idx, value); }

int MySQLStmt::bind(int idx, const uint8_t& value) { return bindUint8(idx, value); }

int MySQLStmt::bind(int idx, const int16_t& value) { return bindInt16(idx, value); }

int MySQLStmt::bind(int idx, const uint16_t& value) { return bindUint16(idx, value); }

int MySQLStmt::bind(int idx, const int32_t& value) { return bindInt32(idx, value); }

int MySQLStmt::bind(int idx, const uint32_t& value) { return bindUint32(idx, value); }

int MySQLStmt::bind(int idx, const int64_t& value) { return bindInt64(idx, value); }

int MySQLStmt::bind(int idx, const uint64_t& value) { return bindUint64(idx, value); }

int MySQLStmt::bind(int idx, const float& value) { return bindFloat(idx, value); }

int MySQLStmt::bind(int idx, const double& value) { return bindDouble(idx, value); }

// int MySQLStmt::bind(int idx, const MYSQL_TIME& value, int type) {
//     return bindTime(idx, value, type);
// }

int MySQLStmt::bind(int idx, const std::string& value) { return bindString(idx, value); }

int MySQLStmt::bind(int idx, const char* value) { return bindString(idx, value); }

int MySQLStmt::bind(int idx, const void* value, int len) { return bindBlob(idx, value, len); }

int MySQLStmt::bind(int idx) {
	idx -= 1;
	_binds[idx].buffer_type = MYSQL_TYPE_NULL;
	return 0;
}

int MySQLStmt::getErrno() { return mysql_stmt_errno(_stmt); }

std::string MySQLStmt::getErrStr() {
	const char* e = mysql_stmt_error(_stmt);
	if(e) {
		return e;
	}
	return "";
}

int MySQLStmt::bindNull(int idx) { return bind(idx); }

int MySQLStmt::bindInt8(int idx, const int8_t& value) {
	idx -= 1;
	_binds[idx].buffer_type = MYSQL_TYPE_TINY;
#define BIND_COPY(ptr, size)               \
	if(_binds[idx].buffer == nullptr) {    \
		_binds[idx].buffer = malloc(size); \
	}                                      \
	memcpy(_binds[idx].buffer, ptr, size);
	BIND_COPY(&value, sizeof(value));
	_binds[idx].is_unsigned	  = false;
	_binds[idx].buffer_length = sizeof(value);
	return 0;
}

int MySQLStmt::bindUint8(int idx, const uint8_t& value) {
	idx -= 1;
	_binds[idx].buffer_type = MYSQL_TYPE_TINY;
	BIND_COPY(&value, sizeof(value));
	_binds[idx].is_unsigned	  = true;
	_binds[idx].buffer_length = sizeof(value);
	return 0;
}

int MySQLStmt::bindInt16(int idx, const int16_t& value) {
	idx -= 1;
	_binds[idx].buffer_type = MYSQL_TYPE_SHORT;
	BIND_COPY(&value, sizeof(value));
	_binds[idx].is_unsigned	  = false;
	_binds[idx].buffer_length = sizeof(value);
	return 0;
}

int MySQLStmt::bindUint16(int idx, const uint16_t& value) {
	idx -= 1;
	_binds[idx].buffer_type = MYSQL_TYPE_SHORT;
	BIND_COPY(&value, sizeof(value));
	_binds[idx].is_unsigned	  = true;
	_binds[idx].buffer_length = sizeof(value);
	return 0;
}

int MySQLStmt::bindInt32(int idx, const int32_t& value) {
	idx -= 1;
	_binds[idx].buffer_type = MYSQL_TYPE_LONG;
	BIND_COPY(&value, sizeof(value));
	_binds[idx].is_unsigned	  = false;
	_binds[idx].buffer_length = sizeof(value);
	return 0;
}

int MySQLStmt::bindUint32(int idx, const uint32_t& value) {
	idx -= 1;
	_binds[idx].buffer_type = MYSQL_TYPE_LONG;
	BIND_COPY(&value, sizeof(value));
	_binds[idx].is_unsigned	  = true;
	_binds[idx].buffer_length = sizeof(value);
	return 0;
}

int MySQLStmt::bindInt64(int idx, const int64_t& value) {
	idx -= 1;
	_binds[idx].buffer_type = MYSQL_TYPE_LONGLONG;
	BIND_COPY(&value, sizeof(value));
	_binds[idx].is_unsigned	  = false;
	_binds[idx].buffer_length = sizeof(value);
	return 0;
}

int MySQLStmt::bindUint64(int idx, const uint64_t& value) {
	idx -= 1;
	_binds[idx].buffer_type = MYSQL_TYPE_LONGLONG;
	BIND_COPY(&value, sizeof(value));
	_binds[idx].is_unsigned	  = true;
	_binds[idx].buffer_length = sizeof(value);
	return 0;
}

int MySQLStmt::bindFloat(int idx, const float& value) {
	idx -= 1;
	_binds[idx].buffer_type = MYSQL_TYPE_FLOAT;
	BIND_COPY(&value, sizeof(value));
	_binds[idx].buffer_length = sizeof(value);
	return 0;
}

int MySQLStmt::bindDouble(int idx, const double& value) {
	idx -= 1;
	_binds[idx].buffer_type = MYSQL_TYPE_DOUBLE;
	BIND_COPY(&value, sizeof(value));
	_binds[idx].buffer_length = sizeof(value);
	return 0;
}

int MySQLStmt::bindString(int idx, const char* value) {
	idx -= 1;
	_binds[idx].buffer_type = MYSQL_TYPE_STRING;
#define BIND_COPY_LEN(ptr, size)                                  \
	if(_binds[idx].buffer == nullptr) {                           \
		_binds[idx].buffer = malloc(size);                        \
	} else if((size_t)_binds[idx].buffer_length < (size_t)size) { \
		free(_binds[idx].buffer);                                 \
		_binds[idx].buffer = malloc(size);                        \
	}                                                             \
	memcpy(_binds[idx].buffer, ptr, size);                        \
	_binds[idx].buffer_length = size;
	BIND_COPY_LEN(value, strlen(value));
	return 0;
}

int MySQLStmt::bindString(int idx, const std::string& value) {
	idx -= 1;
	_binds[idx].buffer_type = MYSQL_TYPE_STRING;
	BIND_COPY_LEN(value.c_str(), value.size());
	return 0;
}

int MySQLStmt::bindBlob(int idx, const void* value, int64_t size) {
	idx -= 1;
	_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
	BIND_COPY_LEN(value, size);
	return 0;
}

int MySQLStmt::bindBlob(int idx, const std::string& value) {
	idx -= 1;
	_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
	BIND_COPY_LEN(value.c_str(), value.size());
	return 0;
}

// int MySQLStmt::bindTime(int idx, const MYSQL_TIME& value, int type) {
//     idx -= 1;
//     _binds[idx].buffer_type = (enum_field_types)type;
//     _binds[idx].buffer = &value;
//     _binds[idx].buffer_length = sizeof(value);
//     return 0;
// }

int MySQLStmt::bindTime(int idx, const time_t& value) {
	// idx -= 1;
	// _binds[idx].buffer_type = MYSQL_TYPE_TIMESTAMP;
	// MYSQL_TIME* mt = (MYSQL_TIME*)malloc(sizeof(MYSQL_TIME));
	// time_t_to_mysql_time(value, *mt);
	// _binds[idx].buffer = mt;
	// _binds[idx].buffer_length = sizeof(MYSQL_TIME);
	// return 0;
	return bindString(idx, time2Str(value));
}

int MySQLStmt::execute() {
	mysql_stmt_bind_param(_stmt, &_binds[0]);
	return mysql_stmt_execute(_stmt);
}

int64_t MySQLStmt::getLastInsertId() { return mysql_stmt_insert_id(_stmt); }

ISQLData::ptr MySQLStmt::query() {
	mysql_stmt_bind_param(_stmt, &_binds[0]);
	return MySQLStmtResultSet::create(shared_from_this());
}

MySQLResultSet::MySQLResultSet(MYSQL_RES* res, int eno, const char* estr)
	: _errno(eno)
	, _errstr(estr)
	, _cur(nullptr)
	, _curLength(nullptr) {
	if(res) {
		_data.reset(res, mysql_free_result);
	}
}

bool MySQLResultSet::foreach(dataCallback cb) {
	MYSQL_ROW row;
	uint64_t  fields = getColumnCount();
	int		  i		 = 0;
	while((row = mysql_fetch_row(_data.get()))) {
		if(!cb(row, fields, i++)) {
			break;
		}
	}
	return true;
}

int MySQLResultSet::getDataCount() { return mysql_num_rows(_data.get()); }

int MySQLResultSet::getColumnCount() { return mysql_num_fields(_data.get()); }

int MySQLResultSet::getColumnBytes(int idx) { return _curLength[idx]; }

int MySQLResultSet::getColumnType(int idx) { return 0; }

std::string MySQLResultSet::getColumnName(int idx) { return ""; }

bool MySQLResultSet::isNull(int idx) {
	if(_cur[idx] == nullptr) {
		return true;
	}
	return false;
}

int8_t MySQLResultSet::getInt8(int idx) { return getInt64(idx); }

uint8_t MySQLResultSet::getUint8(int idx) { return getInt64(idx); }

int16_t MySQLResultSet::getInt16(int idx) { return getInt64(idx); }

uint16_t MySQLResultSet::getUint16(int idx) { return getInt64(idx); }

int32_t MySQLResultSet::getInt32(int idx) { return getInt64(idx); }

uint32_t MySQLResultSet::getUint32(int idx) { return getInt64(idx); }

int64_t MySQLResultSet::getInt64(int idx) { return TypeUtil::atoi(_cur[idx]); }

uint64_t MySQLResultSet::getUint64(int idx) { return getInt64(idx); }

float MySQLResultSet::getFloat(int idx) { return getDouble(idx); }

double MySQLResultSet::getDouble(int idx) { return TypeUtil::atof(_cur[idx]); }

std::string MySQLResultSet::getString(int idx) { return std::string(_cur[idx], _curLength[idx]); }

std::string MySQLResultSet::getBlob(int idx) { return std::string(_cur[idx], _curLength[idx]); }

time_t MySQLResultSet::getTime(int idx) {
	if(_cur[idx]) {
		return 0;
	}
	return str2Time(_cur[idx]);
}

bool MySQLResultSet::next() {
	_cur	   = mysql_fetch_row(_data.get());
	_curLength = mysql_fetch_lengths(_data.get());
	return _cur;
}

MySQLStmtResultSet::ptr MySQLStmtResultSet::create(std::shared_ptr<MySQLStmt> stmt) {
	int						eno	   = mysql_stmt_errno(stmt->getRaw());
	const char*				errstr = mysql_stmt_error(stmt->getRaw());
	MySQLStmtResultSet::ptr rt(new MySQLStmtResultSet(stmt, eno, errstr));
	if(eno) {
		return rt;
	}
	MYSQL_RES* res = mysql_stmt_result_metadata(stmt->getRaw());
	if(!res) {
		return MySQLStmtResultSet::ptr(new MySQLStmtResultSet(stmt, stmt->getErrno(), stmt->getErrStr()));
	}

	int			 num	= mysql_num_fields(res);
	MYSQL_FIELD* fields = mysql_fetch_fields(res);

	rt->_binds.resize(num);
	memset(&rt->_binds[0], 0, sizeof(rt->_binds[0]) * num);
	rt->_datas.resize(num);

	for(int i = 0; i < num; ++i) {
		rt->_datas[i]._type = fields[i].type;
		switch(fields[i].type) {
#define XX(m, t)                        \
	case m:                             \
		rt->_datas[i].alloc(sizeof(t)); \
		break;
			XX(MYSQL_TYPE_TINY, int8_t);
			XX(MYSQL_TYPE_SHORT, int16_t);
			XX(MYSQL_TYPE_LONG, int32_t);
			XX(MYSQL_TYPE_LONGLONG, int64_t);
			XX(MYSQL_TYPE_FLOAT, float);
			XX(MYSQL_TYPE_DOUBLE, double);
			XX(MYSQL_TYPE_TIMESTAMP, MYSQL_TIME);
			XX(MYSQL_TYPE_DATETIME, MYSQL_TIME);
			XX(MYSQL_TYPE_DATE, MYSQL_TIME);
			XX(MYSQL_TYPE_TIME, MYSQL_TIME);
#undef XX
		default:
			rt->_datas[i].alloc(fields[i].length);
			break;
		}

		rt->_binds[i].buffer_type	= rt->_datas[i]._type;
		rt->_binds[i].buffer		= rt->_datas[i]._data;
		rt->_binds[i].buffer_length = rt->_datas[i]._dataLength;
		rt->_binds[i].length		= &rt->_datas[i]._length;
		rt->_binds[i].is_null		= &rt->_datas[i]._isNull;
		rt->_binds[i].error			= &rt->_datas[i]._error;
	}

	if(mysql_stmt_bind_result(stmt->getRaw(), &rt->_binds[0])) {
		return MySQLStmtResultSet::ptr(new MySQLStmtResultSet(stmt, stmt->getErrno(), stmt->getErrStr()));
	}

	stmt->execute();

	if(mysql_stmt_store_result(stmt->getRaw())) {
		return MySQLStmtResultSet::ptr(new MySQLStmtResultSet(stmt, stmt->getErrno(), stmt->getErrStr()));
	}
	// rt->next();
	return rt;
}

int MySQLStmtResultSet::getDataCount() { return mysql_stmt_num_rows(_stmt->getRaw()); }

int MySQLStmtResultSet::getColumnCount() { return mysql_stmt_field_count(_stmt->getRaw()); }

int MySQLStmtResultSet::getColumnBytes(int idx) { return _datas[idx]._length; }

int MySQLStmtResultSet::getColumnType(int idx) { return _datas[idx]._type; }

std::string MySQLStmtResultSet::getColumnName(int idx) { return ""; }

bool MySQLStmtResultSet::isNull(int idx) { return _datas[idx]._isNull; }

#define XX(type) return *(type*)_datas[idx]._data

int8_t MySQLStmtResultSet::getInt8(int idx) { XX(int8_t); }

uint8_t MySQLStmtResultSet::getUint8(int idx) { XX(uint8_t); }

int16_t MySQLStmtResultSet::getInt16(int idx) { XX(int16_t); }

uint16_t MySQLStmtResultSet::getUint16(int idx) { XX(uint16_t); }

int32_t MySQLStmtResultSet::getInt32(int idx) { XX(int32_t); }

uint32_t MySQLStmtResultSet::getUint32(int idx) { XX(uint32_t); }

int64_t MySQLStmtResultSet::getInt64(int idx) { XX(int64_t); }

uint64_t MySQLStmtResultSet::getUint64(int idx) { XX(uint64_t); }

float MySQLStmtResultSet::getFloat(int idx) { XX(float); }

double MySQLStmtResultSet::getDouble(int idx) { XX(double); }

#undef XX

std::string MySQLStmtResultSet::getString(int idx) {
	return std::string(_datas[idx]._data, _datas[idx]._length);
}

std::string MySQLStmtResultSet::getBlob(int idx) {
	return std::string(_datas[idx]._data, _datas[idx]._length);
}

time_t MySQLStmtResultSet::getTime(int idx) {
	MYSQL_TIME* v  = (MYSQL_TIME*)_datas[idx]._data;
	time_t		ts = 0;
	mysql_time_to_time_t(*v, ts);
	return ts;
}

bool MySQLStmtResultSet::next() { return !mysql_stmt_fetch(_stmt->getRaw()); }

MySQLStmtResultSet::Data::Data()
	: _isNull(0)
	, _error(0)
	, _type()
	, _length(0)
	, _dataLength(0)
	, _data(nullptr) {}

MySQLStmtResultSet::Data::~Data() {
	if(_data) {
		delete[] _data;
	}
}

void MySQLStmtResultSet::Data::alloc(size_t size) {
	if(_data) {
		delete[] _data;
	}
	_data		= new char[size]();
	_length		= size;
	_dataLength = size;
}

MySQLStmtResultSet::MySQLStmtResultSet(std::shared_ptr<MySQLStmt> stmt, int eno, const std::string& estr)
	: _errno(eno)
	, _errstr(estr)
	, _stmt(stmt) {}

MySQLStmtResultSet::~MySQLStmtResultSet() {
	if(_errno) {
		mysql_stmt_free_result(_stmt->getRaw());
	}
}

ISQLData::ptr MySQL::query(const std::string& sql) {
	_cmd		   = sql;
	MYSQL_RES* res = my_mysql_query(_mysql.get(), _cmd.c_str());
	if(!res) {
		_hasError = true;
		return nullptr;
	}
	_hasError = false;
	ISQLData::ptr rt(new MySQLResultSet(res, mysql_errno(_mysql.get()), mysql_error(_mysql.get())));
	return rt;
}

const char* MySQL::cmd() { return _cmd.c_str(); }

bool MySQL::use(const std::string& dbname) {
	if(_mysql) {
		return false;
	}
	if(_dbname == dbname) {
		return true;
	}
	if(mysql_select_db(_mysql.get(), dbname.c_str()) == 0) {
		_dbname	  = dbname;
		_hasError = false;
		return true;
	} else {
		_dbname	  = "";
		_hasError = true;
		return false;
	}
}

std::string MySQL::getErrStr() {
	if(_mysql) {
		return "mysql is null";
	}
	const char* str = mysql_error(_mysql.get());
	if(str) {
		return str;
	}
	return "";
}

int MySQL::getErrno() {
	if(_mysql) {
		return -1;
	}
	return mysql_errno(_mysql.get());
}

uint64_t MySQL::getInsertId() {
	if(_mysql) {
		return mysql_insert_id(_mysql.get());
	}
	return 0;
}

MySQLTransaction::ptr MySQLTransaction::create(MySQL::ptr mysql, bool auto_commit) {
	MySQLTransaction::ptr rt(new MySQLTransaction(mysql, auto_commit));
	if(rt->begin()) {
		return rt;
	}
	return nullptr;
}

MySQLTransaction::~MySQLTransaction() {
	if(_autoCommit) {
		commit();
	} else {
		rollback();
	}
}

int64_t MySQLTransaction::getLastInsertId() { return _mysql->getLastInsertId(); }

bool MySQLTransaction::begin() {
	int rt = execute("BEGIN");
	return rt == 0;
}

bool MySQLTransaction::commit() {
	if(_isFinished || _hasError) {
		return _hasError;
	}
	int rt = execute("COMMIT");
	if(rt == 0) {
		_isFinished = true;
	} else {
		_hasError = true;
	}
	return rt == 0;
}

bool MySQLTransaction::rollback() {
	if(_isFinished) {
		return true;
	}
	int rt = execute("ROLLBACK");
	if(rt == 0) {
		_isFinished = true;
	} else {
		_hasError = true;
	}
	return rt == 0;
}

int MySQLTransaction::execute(const std::string& sql) {
	if(_isFinished) {
		AZZATO_LOG_ERROR(systemLogger) << "transaction is finished, sql=" << sql;
		return -1;
	}
	int rt = _mysql->execute(sql);
	if(rt) {
		_hasError = true;
	}
	return rt;
}

std::shared_ptr<MySQL> MySQLTransaction::getMySQL() { return _mysql; }

MySQLTransaction::MySQLTransaction(MySQL::ptr mysql, bool auto_commit)
	: _mysql(mysql)
	, _autoCommit(auto_commit)
	, _isFinished(false)
	, _hasError(false) {}

MySQLManager::MySQLManager()
	: _maxConn(10) {
	mysql_library_init(0, nullptr, nullptr);
}

MySQLManager::~MySQLManager() {
	mysql_library_end();
	for(auto& i : _conns) {
		for(auto& n : i.second) {
			delete n;
		}
	}
}

MySQL::ptr MySQLManager::get(const std::string& name) {
	MutexType::Lock lock(_mutex);
	auto			it = _conns.find(name);
	if(it != _conns.end()) {
		if(!it->second.empty()) {
			MySQL* rt = it->second.front();
			it->second.pop_front();
			lock.unlock();
			if(!rt->isNeedCheck()) {
				rt->_lastUsedTime = time(0);
				return MySQL::ptr(rt, [this, name](MySQL* m) { freeMySQL(name, m); });
			}
			if(rt->ping()) {
				rt->_lastUsedTime = time(0);
				return MySQL::ptr(rt, [this, name](MySQL* m) { freeMySQL(name, m); });
			} else if(rt->connect()) {
				rt->_lastUsedTime = time(0);
				return MySQL::ptr(rt, [this, name](MySQL* m) { freeMySQL(name, m); });
			} else {
				AZZATO_LOG_WARN(systemLogger) << "reconnect " << name << " fail";
				return nullptr;
			}
		}
	}
	auto							   config = g_mysql_dbs->getValue();
	auto							   sit	  = config.find(name);
	std::map<std::string, std::string> args;
	if(sit != config.end()) {
		args = sit->second;
	} else {
		sit = _dbDefines.find(name);
		if(sit != _dbDefines.end()) {
			args = sit->second;
		} else {
			return nullptr;
		}
	}
	lock.unlock();
	MySQL* rt = new MySQL(args);
	if(rt->connect()) {
		rt->_lastUsedTime = time(0);
		return MySQL::ptr(rt, [this, name](MySQL* m) { freeMySQL(name, m); });
	} else {
		delete rt;
		return nullptr;
	}
}

void MySQLManager::registerMySQL(const std::string& name, const std::map<std::string, std::string>& params) {
	MutexType::Lock lock(_mutex);
	_dbDefines[name] = params;
}

void MySQLManager::checkConnection(int sec) {
	time_t				now = time(0);
	std::vector<MySQL*> conns;
	MutexType::Lock		lock(_mutex);
	for(auto& i : _conns) {
		for(auto it = i.second.begin(); it != i.second.end();) {
			if((int)(now - (*it)->_lastUsedTime) >= sec) {
				auto tmp = *it;
				i.second.erase(it++);
				conns.push_back(tmp);
			} else {
				++it;
			}
		}
	}
	lock.unlock();
	for(auto& i : conns) {
		delete i;
	}
}

int MySQLManager::execute(const std::string& name, const std::string& sql) {
	auto conn = get(name);
	if(!conn) {
		AZZATO_LOG_ERROR(systemLogger) << "MySQLManager::execute, get(" << name << ") fail, sql=" << sql;
		return -1;
	}
	return conn->execute(sql);
}

ISQLData::ptr MySQLManager::query(const std::string& name, const std::string& sql) {
	auto conn = get(name);
	if(!conn) {
		AZZATO_LOG_ERROR(systemLogger) << "MySQLManager::query, get(" << name << ") fail, sql=" << sql;
		return nullptr;
	}
	return conn->query(sql);
}

MySQLTransaction::ptr MySQLManager::openTransaction(const std::string& name, bool auto_commit) {
	auto conn = get(name);
	if(!conn) {
		AZZATO_LOG_ERROR(systemLogger) << "MySQLManager::openTransaction, get(" << name << ") fail";
		return nullptr;
	}
	MySQLTransaction::ptr trans(MySQLTransaction::create(conn, auto_commit));
	return trans;
}

void MySQLManager::freeMySQL(const std::string& name, MySQL* m) {
	if(m->_mysql) {
		MutexType::Lock lock(_mutex);
		if(_conns[name].size() < (size_t)m->_poolSize) {
			_conns[name].push_back(m);
			return;
		}
	}
	delete m;
}

ISQLData::ptr MySQLUtil::query(const std::string& name, const std::string& sql) {
	auto m = MySQLMgr::getInstance()->get(name);
	if(!m) {
		return nullptr;
	}
	return m->query(sql);
}

ISQLData::ptr MySQLUtil::tryQuery(const std::string& name, uint32_t count, const std::string& sql) {
	for(uint32_t i = 0; i < count; ++i) {
		auto rpy = query(name, sql);
		if(rpy) {
			return rpy;
		}
	}
	return nullptr;
}

int MySQLUtil::execute(const std::string& name, const std::string& sql) {
	auto m = MySQLMgr::getInstance()->get(name);
	if(!m) {
		return -1;
	}
	return m->execute(sql);
}

int MySQLUtil::tryExecute(const std::string& name, uint32_t count, const std::string& sql) {
	int rpy = 0;
	for(uint32_t i = 0; i < count; ++i) {
		rpy = execute(name, sql);
		if(!rpy) {
			return rpy;
		}
	}
	return rpy;
}

}  // namespace azzato
