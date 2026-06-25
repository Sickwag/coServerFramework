#include "log.h"

#include "switch.h"
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <time.h>

namespace azzato {

// ======================================================================
// LogLevel
// ======================================================================

const char* LogLevel::toString(LogLevel::Level level) {
	switch(level) {
#define XX(name)         \
	case LogLevel::name: \
		return #name;    \
		break;

		XX(Level::Debug);
		XX(Level::Info);
		XX(Level::Warn);
		XX(Level::Error);
		XX(Level::Fatal);
#undef XX
	default:
		return "Unknow";
	}
}

LogLevel::Level LogLevel::fromString(const std::string& str) {
#define XX(level, v)            \
	if(str == #v) {             \
		return LogLevel::level; \
	}
	XX(Level::Debug, debug);
	XX(Level::Info, info);
	XX(Level::Warn, warn);
	XX(Level::Error, error);
	XX(Level::Fatal, fatal);

	XX(Level::Debug, DEBUG);
	XX(Level::Info, INFO);
	XX(Level::Warn, WARN);
	XX(Level::Error, ERROR);
	XX(Level::Fatal, FATAL);
#undef XX
	return LogLevel::Level::Unknow;
}

// ======================================================================
// LogEvent
// ======================================================================

LogEvent::LogEvent(std::shared_ptr<Logger> logger,
				   LogLevel::Level		   level,
				   const char*			   filename,
				   int32_t				   line,
				   uint32_t				   elapse,
				   uint32_t				   threadId,
				   uint32_t				   fiberId,
				   uint64_t				   timestamp,
				   const std::string&	   threadName)
	: _filename(filename)
	, _line(line)
	, _elapse(elapse)
	, _threadId(threadId)
	, _fiberId(fiberId)
	, _timestamp(timestamp)
	, _threadName(threadName)
	, _logger(std::move(logger))
	, _level(level) {}

// ======================================================================
// LogEventWrap
// ======================================================================

LogEventWrap::LogEventWrap(LogEvent::ptr e)
	: _event(std::move(e)) {}

LogEventWrap::~LogEventWrap() { _event->getLogger()->log(_event->getLevel(), _event); }

// ======================================================================
// LogFormatter
// ======================================================================

LogFormatter::LogFormatter(const std::string& pattern)
	: _pattern(pattern) {
	init();
}

LogFormatter::LogFormatter(LogFormat fmt)
	: _pattern() {
	init(fmt);
}

std::string LogFormatter::format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
	std::stringstream ss;
	for(const auto& item : _items) {
		item->format(ss, logger, level, event);
	}
	return ss.str();
}

std::ostream& LogFormatter::format(std::ostream&		   ofs,
								   std::shared_ptr<Logger> logger,
								   LogLevel::Level		   level,
								   LogEvent::ptr		   event) {
	for(const auto& item : _items) {
		item->format(ofs, logger, level, event);
	}
	return ofs;
}

// ---- FormatItem subclasses ---------------------------------------------

namespace {

class MessageFormatItem : public LogFormatter::FormatItem {
  public:
	explicit MessageFormatItem(const std::string& /*unused*/) {}
	void format(std::ostream& os,
				Logger::ptr /*logger*/,
				LogLevel::Level /*level*/,
				LogEvent::ptr event) override {
		os << event->getContent();
	}
};

class LevelFormatItem : public LogFormatter::FormatItem {
  public:
	explicit LevelFormatItem(const std::string& /*unused*/) {}
	void format(std::ostream& os,
				Logger::ptr /*logger*/,
				LogLevel::Level level,
				LogEvent::ptr /*event*/) override {
		os << LogLevel::toString(level);
	}
};

class ElapseFormatItem : public LogFormatter::FormatItem {
  public:
	explicit ElapseFormatItem(const std::string& /*unused*/) {}
	void format(std::ostream& os,
				Logger::ptr /*logger*/,
				LogLevel::Level /*level*/,
				LogEvent::ptr event) override {
		os << event->getElapse();
	}
};

class NameFormatItem : public LogFormatter::FormatItem {
  public:
	explicit NameFormatItem(const std::string& /*unused*/) {}
	void
	format(std::ostream& os, Logger::ptr logger, LogLevel::Level /*level*/, LogEvent::ptr event) override {
		os << event->getLogger()->getName();
	}
};

class ThreadIdFormatItem : public LogFormatter::FormatItem {
  public:
	explicit ThreadIdFormatItem(const std::string& /*unused*/) {}
	void format(std::ostream& os,
				Logger::ptr /*logger*/,
				LogLevel::Level /*level*/,
				LogEvent::ptr event) override {
		os << event->getThreadId();
	}
};

class FiberIdFormatItem : public LogFormatter::FormatItem {
  public:
	explicit FiberIdFormatItem(const std::string& /*unused*/) {}
	void format(std::ostream& os,
				Logger::ptr /*logger*/,
				LogLevel::Level /*level*/,
				LogEvent::ptr event) override {
		os << event->getFiberId();
	}
};

class ThreadNameFormatItem : public LogFormatter::FormatItem {
  public:
	explicit ThreadNameFormatItem(const std::string& /*unused*/) {}
	void format(std::ostream& os,
				Logger::ptr /*logger*/,
				LogLevel::Level /*level*/,
				LogEvent::ptr event) override {
		os << event->getThreadName();
	}
};

class DateTimeFormatItem : public LogFormatter::FormatItem {
  public:
	explicit DateTimeFormatItem(const std::string& format = "")
		: _format(format.empty() ? "%Y-%m-%d %H:%M:%S" : format) {}

	void format(std::ostream& os,
				Logger::ptr /*logger*/,
				LogLevel::Level /*level*/,
				LogEvent::ptr event) override {
		struct tm tm;
		time_t	  raw = static_cast<time_t>(event->getTimestamp());
		localtime_r(&raw, &tm);
		char buf[64];
		strftime(buf, sizeof(buf), _format.c_str(), &tm);
		os << buf;
	}

  private:
	std::string _format;
};

class FilenameFormatItem : public LogFormatter::FormatItem {
  public:
	explicit FilenameFormatItem(const std::string& /*unused*/) {}
	void format(std::ostream& os,
				Logger::ptr /*logger*/,
				LogLevel::Level /*level*/,
				LogEvent::ptr event) override {
		os << event->getFile();
	}
};

class LineFormatItem : public LogFormatter::FormatItem {
  public:
	explicit LineFormatItem(const std::string& /*unused*/) {}
	void format(std::ostream& os,
				Logger::ptr /*logger*/,
				LogLevel::Level /*level*/,
				LogEvent::ptr event) override {
		os << event->getLine();
	}
};

class NewLineFormatItem : public LogFormatter::FormatItem {
  public:
	explicit NewLineFormatItem(const std::string& /*unused*/) {}
	void format(std::ostream& os,
				Logger::ptr /*logger*/,
				LogLevel::Level /*level*/,
				LogEvent::ptr /*event*/) override {
		os << std::endl;
	}
};

class StringFormatItem : public LogFormatter::FormatItem {
  public:
	explicit StringFormatItem(const std::string& str)
		: _string(str) {}
	void format(std::ostream& os,
				Logger::ptr /*logger*/,
				LogLevel::Level /*level*/,
				LogEvent::ptr /*event*/) override {
		os << _string;
	}

  private:
	std::string _string;
};

class TabFormatItem : public LogFormatter::FormatItem {
  public:
	explicit TabFormatItem(const std::string& /*unused*/) {}
	void format(std::ostream& os,
				Logger::ptr /*logger*/,
				LogLevel::Level /*level*/,
				LogEvent::ptr /*event*/) override {
		os << '\t';
	}
};

}  // anonymous namespace

// ---- Pattern parsing ----------------------------------------------------

void LogFormatter::init() {
	// Each tuple: (content, format, type)  type=0 raw string, type=1 format item
	std::vector<std::tuple<std::string, std::string, int>> vec;
	std::string											   nstr;

	for(size_t i = 0; i < _pattern.size(); ++i) {
		if(_pattern[i] != '%') {
			nstr.append(1, _pattern[i]);
			continue;
		}

		// Literal %% -> %
		if((i + 1) < _pattern.size() && _pattern[i + 1] == '%') {
			nstr.append(1, '%');
			++i;  // skip the second '%'
			continue;
		}

		size_t n		 = i + 1;
		int	   fmtStatus = 0;  // 0 = no brace, 1 = inside braces
		size_t fmtBegin	 = 0;

		std::string str;
		std::string fmt;

		while(n < _pattern.size()) {
			if(fmtStatus == 0 && !std::isalpha(_pattern[n]) && _pattern[n] != '{' && _pattern[n] != '}') {
				str = _pattern.substr(i + 1, n - i - 1);
				break;
			}
			if(fmtStatus == 0) {
				if(_pattern[n] == '{') {
					str		  = _pattern.substr(i + 1, n - i - 1);
					fmtStatus = 1;
					fmtBegin  = n;
					++n;
					continue;
				}
			} else {  // fmtStatus == 1
				if(_pattern[n] == '}') {
					fmt		  = _pattern.substr(fmtBegin + 1, n - fmtBegin - 1);
					fmtStatus = 0;
					++n;
					break;
				}
			}
			++n;
			if(n == _pattern.size() && str.empty()) {
				str = _pattern.substr(i + 1);
			}
		}

		if(fmtStatus == 0) {
			if(!nstr.empty()) {
				vec.emplace_back(nstr, std::string{}, 0);
				nstr.clear();
			}
			vec.emplace_back(str, fmt, 1);
			i = n - 1;
		} else {
			std::cerr << "pattern parse error: " << _pattern << " - " << _pattern.substr(i) << std::endl;
			_hasError = true;
			vec.emplace_back("<<pattern_error>>", fmt, 0);
		}
	}

	if(!nstr.empty()) {
		vec.emplace_back(nstr, std::string{}, 0);
	}

	static const std::map<std::string, std::function<LogFormatter::FormatItem::ptr(const std::string&)>>
		s_formatItems = {
#define XX(str, C)                                                        \
	{                                                                     \
		#str, [](const std::string& f) { return std::make_shared<C>(f); } \
	}

			XX(m, MessageFormatItem),
			XX(p, LevelFormatItem),
			XX(r, ElapseFormatItem),
			XX(c, NameFormatItem),
			XX(t, ThreadIdFormatItem),
			XX(n, NewLineFormatItem),
			XX(d, DateTimeFormatItem),
			XX(f, FilenameFormatItem),
			XX(l, LineFormatItem),
			XX(T, TabFormatItem),
			XX(F, FiberIdFormatItem),
			XX(N, ThreadNameFormatItem),
#undef XX
		};

	for(const auto& tup : vec) {
		if(std::get<2>(tup) == 0) {
			_items.push_back(std::make_shared<StringFormatItem>(std::get<0>(tup)));
		} else {
			auto it = s_formatItems.find(std::get<0>(tup));
			if(it == s_formatItems.end()) {
				_items.push_back(
					std::make_shared<StringFormatItem>("<<error_format %" + std::get<0>(tup) + ">>"));
				_hasError = true;
			} else {
				_items.push_back(it->second(std::get<1>(tup)));
			}
		}
	}
}

void LogFormatter::init(LogFormat fmt) {
	auto add = [&](auto&& item) { _items.push_back(std::move(item)); };

	if((fmt & LogFormat::DateTime) != LogFormat::None) {
		add(std::make_shared<DateTimeFormatItem>());
		add(std::make_shared<TabFormatItem>(""));
	}
	if((fmt & LogFormat::ThreadId) != LogFormat::None) {
		add(std::make_shared<ThreadIdFormatItem>(""));
		add(std::make_shared<TabFormatItem>(""));
	}
	if((fmt & LogFormat::ThreadName) != LogFormat::None) {
		add(std::make_shared<ThreadNameFormatItem>(""));
		add(std::make_shared<TabFormatItem>(""));
	}
	if((fmt & LogFormat::FiberId) != LogFormat::None) {
		add(std::make_shared<FiberIdFormatItem>(""));
		add(std::make_shared<TabFormatItem>(""));
	}
	if((fmt & LogFormat::Level) != LogFormat::None) {
		add(std::make_shared<StringFormatItem>("["));
		add(std::make_shared<LevelFormatItem>(""));
		add(std::make_shared<StringFormatItem>("]"));
		add(std::make_shared<TabFormatItem>(""));
	}
	if((fmt & LogFormat::Name) != LogFormat::None) {
		add(std::make_shared<StringFormatItem>("["));
		add(std::make_shared<NameFormatItem>(""));
		add(std::make_shared<StringFormatItem>("]"));
		add(std::make_shared<TabFormatItem>(""));
	}
	if((fmt & LogFormat::File) != LogFormat::None) {
		add(std::make_shared<FilenameFormatItem>(""));
		add(std::make_shared<StringFormatItem>(":"));
		add(std::make_shared<LineFormatItem>(""));
		add(std::make_shared<TabFormatItem>(""));
	}
	if((fmt & LogFormat::Message) != LogFormat::None) {
		add(std::make_shared<MessageFormatItem>(""));
	}
	add(std::make_shared<NewLineFormatItem>(""));
}

// ======================================================================
// LogAppender
// ======================================================================

void LogAppender::setFormatter(LogFormatter::ptr val) {
	MutexType::Lock lock(_mutex);
	_formatter	  = std::move(val);
	_hasFormatter = static_cast<bool>(_formatter);
}

LogFormatter::ptr LogAppender::getFormatter() {
	MutexType::Lock lock(_mutex);
	return _formatter;
}

// ======================================================================
// Logger
// ======================================================================

Logger::Logger(const std::string& name)
	: _name(name)
	, _level(LogLevel::Level::Debug) {
	_formatter = std::make_shared<LogFormatter>(
		LogFormat::DateTime | LogFormat::ThreadId | LogFormat::ThreadName | LogFormat::FiberId
		| LogFormat::Level | LogFormat::Name | LogFormat::File | LogFormat::Line | LogFormat::Message);
}

void Logger::setFormatter(LogFormatter::ptr val) {
	MutexType::Lock lock(_mutex);
	_formatter = std::move(val);

	for(auto& appender : _appenders) {
		MutexType::Lock al(appender->_mutex);
		if(!appender->_hasFormatter) {
			appender->_formatter = _formatter;
		}
	}
}

void Logger::setFormatter(const std::string& val) {
	auto fmt = std::make_shared<LogFormatter>(val);
	if(fmt->hasError()) {
		std::cerr << "Logger setFormatter name=" << _name << " value=" << val << " invalid formatter"
				  << std::endl;
		return;
	}
	setFormatter(std::move(fmt));
}

std::string Logger::toYamlString() {
	MutexType::Lock lock(_mutex);
	// Placeholder – YAML dependency not required yet.
	std::stringstream ss;
	ss << "name: " << _name << "\n";
	if(_level != LogLevel::Level::Unknow) {
		ss << "level: " << LogLevel::toString(_level) << "\n";
	}
	if(_formatter) {
		ss << "formatter: " << _formatter->getPattern() << "\n";
	}
	ss << "appenders: " << _appenders.size() << "\n";
	return ss.str();
}

LogFormatter::ptr Logger::getFormatter() {
	MutexType::Lock lock(_mutex);
	return _formatter;
}

void Logger::addAppender(LogAppender::ptr appender) {
	MutexType::Lock lock(_mutex);
	if(!appender->getFormatter()) {
		MutexType::Lock al(appender->_mutex);
		appender->_formatter = _formatter;
	}
	_appenders.push_back(std::move(appender));
}

void Logger::delAppender(LogAppender::ptr appender) {
	MutexType::Lock lock(_mutex);
	for(auto it = _appenders.begin(); it != _appenders.end(); ++it) {
		if(*it == appender) {
			_appenders.erase(it);
			break;
		}
	}
}

void Logger::clearAppenders() {
	MutexType::Lock lock(_mutex);
	_appenders.clear();
}

void Logger::log(LogLevel::Level level, LogEvent::ptr event) {
	if(level < _level) {
		return;
	}
	auto			self = shared_from_this();
	MutexType::Lock lock(_mutex);
	if(!_appenders.empty()) {
		for(auto& appender : _appenders) {
			appender->log(self, level, event);
		}
	} else if(_root) {
		_root->log(level, std::move(event));
	}
}

void Logger::debug(LogEvent::ptr event) { log(LogLevel::Level::Debug, std::move(event)); }
void Logger::info(LogEvent::ptr event) { log(LogLevel::Level::Info, std::move(event)); }
void Logger::warn(LogEvent::ptr event) { log(LogLevel::Level::Warn, std::move(event)); }
void Logger::error(LogEvent::ptr event) { log(LogLevel::Level::Error, std::move(event)); }
void Logger::fatal(LogEvent::ptr event) { log(LogLevel::Level::Fatal, std::move(event)); }

// ======================================================================
// FileLogAppender
// ======================================================================

FileLogAppender::FileLogAppender(const std::string& filename)
	: _filename(filename) {
	reopen();
}

void FileLogAppender::log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
	if(level < _level) {
		return;
	}
	uint64_t now = event->getTimestamp();
	if(now >= (_lastTime + 3)) {
		reopen();
		_lastTime = now;
	}
	MutexType::Lock lock(_mutex);
	if(!_formatter->format(_filestream, std::move(logger), level, std::move(event))) {
		std::cerr << "FileLogAppender::log write failed" << std::endl;
	}
}

std::string FileLogAppender::toYamlString() {
	MutexType::Lock lock(_mutex);
	// Placeholder
	std::stringstream ss;
	ss << "type: FileLogAppender\n";
	ss << "file: " << _filename << "\n";
	if(_level != LogLevel::Level::Unknow) {
		ss << "level: " << LogLevel::toString(_level) << "\n";
	}
	if(_hasFormatter && _formatter) {
		ss << "formatter: " << _formatter->getPattern() << "\n";
	}
	return ss.str();
}

bool FileLogAppender::reopen() {
	MutexType::Lock lock(_mutex);
	if(_filestream.is_open()) {
		_filestream.close();
	}
	_filestream.open(_filename, std::ios::app);
	return _filestream.is_open();
}

// ======================================================================
// StdoutLogAppender
// ======================================================================

void StdoutLogAppender::log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
	if(level < _level) {
		return;
	}
	MutexType::Lock lock(_mutex);
	_formatter->format(std::cout, std::move(logger), level, std::move(event));
}

std::string StdoutLogAppender::toYamlString() {
	MutexType::Lock	  lock(_mutex);
	std::stringstream ss;
	ss << "type: StdoutLogAppender\n";
	if(_level != LogLevel::Level::Unknow) {
		ss << "level: " << LogLevel::toString(_level) << "\n";
	}
	if(_hasFormatter && _formatter) {
		ss << "formatter: " << _formatter->getPattern() << "\n";
	}
	return ss.str();
}

// ======================================================================
// LoggerManager
// ======================================================================

LoggerManager::LoggerManager() {
	_root = std::make_shared<Logger>();
	_root->addAppender(std::make_shared<StdoutLogAppender>());
	_loggers[_root->getName()] = _root;
	init();
}

Logger::ptr LoggerManager::getLogger(const std::string& name) {
	MutexType::Lock lock(_mutex);
	auto			it = _loggers.find(name);
	if(it != _loggers.end()) {
		return it->second;
	}
	auto logger	   = std::make_shared<Logger>(name);
	logger->_root  = _root;
	_loggers[name] = logger;
	return logger;
}

std::string LoggerManager::toYamlString() {
	MutexType::Lock	  lock(_mutex);
	std::stringstream ss;
	for(const auto& [name, logger] : _loggers) {
		ss << logger->toYamlString() << "\n";
	}
	return ss.str();
}

void LoggerManager::init() {
	// Reserved for configuration-based logger setup.
}

}  // namespace azzato
