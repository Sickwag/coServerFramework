#pragma once

#include <cstdint>
#include <format>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "mutex.h"
#include "switch.h"
#include "utils/singleton.h"

/**
 * @def AZZATO_LOG_LEVEL(logger, level)
 * @brief Stream log at the given level if the logger's threshold allows it.
 *
 * Usage: AZZATO_LOG_LEVEL(logger, LogLevel::Debug) << "message";
 */
#define AZZATO_LOG_LEVEL(logger, level)                                                            \
	if(logger->getLevel() <= level)                                                                \
	::azzato::LogEventWrap(::azzato::LogEvent::ptr(new ::azzato::LogEvent(logger,                  \
																		  level,                   \
																		  __FILE__,                \
																		  __LINE__,                \
																		  0,                       \
																		  ::azzato::getThreadId(), \
																		  ::azzato::getFiberId(),  \
																		  time(0),                 \
																		  "")))                    \
		.getSS()

#define AZZATO_LOG_DEBUG(logger) AZZATO_LOG_LEVEL(logger, ::azzato::LogLevel::Level::Debug)
#define AZZATO_LOG_INFO(logger) AZZATO_LOG_LEVEL(logger, ::azzato::LogLevel::Level::Info)
#define AZZATO_LOG_WARN(logger) AZZATO_LOG_LEVEL(logger, ::azzato::LogLevel::Level::Warn)
#define AZZATO_LOG_ERROR(logger) AZZATO_LOG_LEVEL(logger, ::azzato::LogLevel::Level::Error)
#define AZZATO_LOG_FATAL(logger) AZZATO_LOG_LEVEL(logger, ::azzato::LogLevel::Level::Fatal)

/**
 * @def AZZATO_LOG_FMT(logger, level, fmt, ...)
 * @brief Format-and-log at the given level using std::format-style placeholders.
 *
 * Usage: AZZATO_LOG_FMT(logger, LogLevel::Info, "value = {}", x);
 */
#define AZZATO_LOG_FMT(logger, level, fmt, ...)                                                    \
	if(logger->getLevel() <= level)                                                                \
	::azzato::LogEventWrap(::azzato::LogEvent::ptr(new ::azzato::LogEvent(logger,                  \
																		  level,                   \
																		  __FILE__,                \
																		  __LINE__,                \
																		  0,                       \
																		  ::azzato::getThreadId(), \
																		  ::azzato::getFiberId(),  \
																		  time(0),                 \
																		  "")))                    \
		.getEvent()                                                                                \
		->format(fmt, ##__VA_ARGS__)

#define AZZATO_LOG_FMT_DEBUG(logger, ...) \
	AZZATO_LOG_FMT(logger, ::azzato::LogLevel::Level::Debug, ##__VA_ARGS__)
#define AZZATO_LOG_FMT_INFO(logger, ...) \
	AZZATO_LOG_FMT(logger, ::azzato::LogLevel::Level::Info, ##__VA_ARGS__)
#define AZZATO_LOG_FMT_WARN(logger, ...) \
	AZZATO_LOG_FMT(logger, ::azzato::LogLevel::Level::Warn, ##__VA_ARGS__)
#define AZZATO_LOG_FMT_ERROR(logger, ...) \
	AZZATO_LOG_FMT(logger, ::azzato::LogLevel::Level::Error, ##__VA_ARGS__)
#define AZZATO_LOG_FMT_FATAL(logger, ...) \
	AZZATO_LOG_FMT(logger, ::azzato::LogLevel::Level::Fatal, ##__VA_ARGS__)

#define AZZATO_LOG_ROOT() ::azzato::LoggerMgr::getInstance()->getRoot()
#define AZZATO_LOG_NAME(name) ::azzato::LoggerMgr::getInstance()->getLogger(name)

namespace azzato {

class Logger;
class LoggerManager;

pid_t	 getThreadId();
uint32_t getFiberId();

// -----------------------------------------------------------------------
// LogLevel
// -----------------------------------------------------------------------

/**
 * @brief Defines log severity levels and their string conversions.
 */
class LogLevel {
  public:
	enum class Level : int { Unknow = 0, Debug, Info, Warn, Error, Fatal };

	/**
	 * @brief Convert a Level to its uppercase string representation.
	 * @param level The severity level.
	 * @return Pointer to a statically allocated string, e.g. "DEBUG".
	 */
	static const char* toString(LogLevel::Level level);

	/**
	 * @brief Parse a case-insensitive string into a Level.
	 * @param str The input string ("debug", "INFO", etc.).
	 * @return The corresponding Level, or Level::Unknow on mismatch.
	 */
	static LogLevel::Level fromString(const std::string& str);
};

// -----------------------------------------------------------------------
// LogFormat — bitmask to control which fields appear in log output
// -----------------------------------------------------------------------

/**
 * @brief Bitmask flags selecting which fields appear in formatted log output.
 *
 * Usage: LogFormat::DateTime | LogFormat::Level | LogFormat::Message
 * Order and separators are defined by LogFormatter::init(LogFormat).
 */
enum class LogFormat : uint32_t {
	None       = 0,
	Message    = 1 << 0,
	Level      = 1 << 1,
	Elapse     = 1 << 2,
	Name       = 1 << 3,
	ThreadId   = 1 << 4,
	FiberId    = 1 << 5,
	ThreadName = 1 << 6,
	DateTime   = 1 << 7,
	File       = 1 << 8,
	Line       = 1 << 9,
};

// -----------------------------------------------------------------------
// LogEvent
// -----------------------------------------------------------------------

/**
 * @brief Holds all contextual data for a single log invocation.
 *
 * Created by the logging macros and consumed by LogFormatter.
 */
class LogEvent {
  public:
	using ptr = std::shared_ptr<LogEvent>;

	/**
	 * @param logger   The logger that originated this event.
	 * @param level    Severity level.
	 * @param filename Source file name (__FILE__).
	 * @param line     Source line number (__LINE__).
	 * @param elapse   Milliseconds since program start.
	 * @param threadId Thread ID.
	 * @param fiberId  Fiber / coroutine ID.
	 * @param timestamp Unix timestamp (seconds).
	 * @param threadName Human-readable thread name.
	 */
	LogEvent(std::shared_ptr<Logger> logger,
			 LogLevel::Level		 level,
			 const char*			 filename,
			 int32_t				 line,
			 uint32_t				 elapse,
			 uint32_t				 threadId,
			 uint32_t				 fiberId,
			 uint64_t				 timestamp,
			 const std::string&		 threadName);

	/** @return Source file name. */
	const char* getFile() const { return _filename; }
	/** @return Source line number. */
	int32_t getLine() const { return _line; }
	/** @return Elapsed milliseconds since program start. */
	uint32_t getElapse() const { return _elapse; }
	/** @return Thread ID. */
	uint32_t getThreadId() const { return _threadId; }
	/** @return Fiber / coroutine ID. */
	uint32_t getFiberId() const { return _fiberId; }
	/** @return Unix timestamp (seconds). */
	uint64_t getTimestamp() const { return _timestamp; }
	/** @return Thread name. */
	const std::string& getThreadName() const { return _threadName; }
	/** @return Accumulated log message content. */
	std::string getContent() const { return _ss.str(); }
	/** @return The logger that created this event. */
	std::shared_ptr<Logger> getLogger() const { return _logger; }
	/** @return Severity level. */
	LogLevel::Level getLevel() const { return _level; }
	/** @return The underlying stringstream for streaming-style logging. */
	std::stringstream& getSS() { return _ss; }

	/**
	 * @brief Append a formatted message using std::format-style placeholders.
	 *
	 * Example: event->format("value = {}, name = {}", x, name);
	 *
	 * @tparam Args Deduced argument types.
	 * @param fmt   std::format format string.
	 * @param args  Values to format.
	 */
	template <typename... Args>
	void format(std::format_string<Args...> fmt, Args&&... args) {
		_ss << std::vformat(fmt.get(), std::make_format_args(std::forward<Args>(args)...));
	}

  private:
	const char*				_filename  = nullptr;
	int32_t					_line	   = 0;
	uint32_t				_elapse	   = 0;
	uint32_t				_threadId  = 0;
	uint32_t				_fiberId   = 0;
	uint64_t				_timestamp = 0;
	std::string				_threadName;
	std::stringstream		_ss;
	std::shared_ptr<Logger> _logger;
	LogLevel::Level			_level;
};

// -----------------------------------------------------------------------
// LogEventWrap
// -----------------------------------------------------------------------

/**
 * @brief RAII wrapper that dispatches the LogEvent to the logger on destruction.
 *
 * Used by the logging macros so that the log call is made automatically
 * when the temporary goes out of scope (end of the full expression).
 */
class LogEventWrap {
  public:
	explicit LogEventWrap(LogEvent::ptr e);
	~LogEventWrap();

	LogEvent::ptr	   getEvent() const { return _event; }
	std::stringstream& getSS() const { return _event->getSS(); }

  private:
	LogEvent::ptr _event;
};

// -----------------------------------------------------------------------
// LogFormatter
// -----------------------------------------------------------------------

/**
 * @brief Parses a pattern string and formats LogEvents into text.
 *
 * Pattern syntax (inspired by printf-style):
 *   %m  – message
 *   %p  – level
 *   %r  – elapsed ms
 *   %c  – logger name
 *   %t  – thread id
 *   %n  – newline
 *   %d  – datetime (optional {format} for strftime)
 *   %f  – filename
 *   %l  – line number
 *   %T  – tab
 *   %F  – fiber id
 *   %N  – thread name
 *
 * Default: "%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"
 */
class LogFormatter {
  public:
	using ptr = std::shared_ptr<LogFormatter>;

	explicit LogFormatter(const std::string& pattern);

	/**
	 * @brief Construct a formatter from a LogFormat bitmask.
	 * @param fmt Bitmask of fields to include.
	 */
	explicit LogFormatter(LogFormat fmt);

	/**
	 * @brief Format into a string.
	 */
	std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);

	/**
	 * @brief Format into an output stream.
	 * @return The same ostream reference (for chaining).
	 */
	std::ostream&
	format(std::ostream& ofs, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);

	// -----------------------------------------------------------------------
	// FormatItem
	// -----------------------------------------------------------------------

	/**
	 * @brief Abstract base for a single token in the parsed pattern.
	 */
	class FormatItem {
	  public:
		using ptr								 = std::shared_ptr<FormatItem>;
		virtual ~FormatItem()					 = default;

		/**
		 * @brief Write this token's contribution into the stream.
		 */
		virtual void format(std::ostream&			os,
							std::shared_ptr<Logger> logger,
							LogLevel::Level			level,
							LogEvent::ptr			event) = 0;
	};

	/** @brief Parse the pattern and build the internal FormatItem list. */
	void init();

	/**
	 * @brief Build the FormatItem list from a LogFormat bitmask.
	 * @param fmt Bitmask of fields to include.
	 */
	void init(LogFormat fmt);

	/** @brief Whether the pattern contained parse errors. */
	bool hasError() const { return _hasError; }

	/** @brief The original pattern string. */
	const std::string& getPattern() const { return _pattern; }

  private:
	std::string					 _pattern;
	std::vector<FormatItem::ptr> _items;
	bool						 _hasError = false;
};

// -----------------------------------------------------------------------
// LogAppender
// -----------------------------------------------------------------------

/**
 * @brief Abstract output destination for log events.
 *
 * Concrete subclasses write to stdout or files.
 */
class LogAppender {
	friend class Logger;

  public:
	using ptr			   = std::shared_ptr<LogAppender>;
	using MutexType		   = Spinlock;

	virtual ~LogAppender() = default;

	/**
	 * @brief Write a log event to this destination.
	 */
	virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;

	/**
	 * @brief Serialize this appender's configuration to a YAML string.
	 */
	virtual std::string toYamlString()															 = 0;

	void			  setFormatter(LogFormatter::ptr val);
	LogFormatter::ptr getFormatter();

	LogLevel::Level getLevel() const { return _level; }
	void			setLevel(LogLevel::Level val) { _level = val; }

  protected:
	LogLevel::Level	  _level		= LogLevel::Level::Debug;
	bool			  _hasFormatter = false;
	MutexType		  _mutex;
	LogFormatter::ptr _formatter;
};

// -----------------------------------------------------------------------
// Logger
// -----------------------------------------------------------------------

/**
 * @brief Named logger that filters by level and fans out to attached appenders.
 *
 * If no appender is attached, events are forwarded to the root logger.
 */
class Logger : public std::enable_shared_from_this<Logger> {
	friend class LoggerManager;

  public:
	using ptr		= std::shared_ptr<Logger>;
	using MutexType = Spinlock;

	explicit Logger(const std::string& name = "root");

	void log(LogLevel::Level level, LogEvent::ptr event);
	void debug(LogEvent::ptr event);
	void info(LogEvent::ptr event);
	void warn(LogEvent::ptr event);
	void error(LogEvent::ptr event);
	void fatal(LogEvent::ptr event);

	void addAppender(LogAppender::ptr appender);
	void delAppender(LogAppender::ptr appender);
	void clearAppenders();

	LogLevel::Level getLevel() const { return _level; }
	void			setLevel(LogLevel::Level val) { _level = val; }

	const std::string& getName() const { return _name; }

	void			  setFormatter(LogFormatter::ptr val);
	void			  setFormatter(const std::string& val);
	LogFormatter::ptr getFormatter();

	std::string toYamlString();

  private:
	std::string					_name;
	LogLevel::Level				_level;
	MutexType					_mutex;	 // protect `_appenders` and `_root`
	std::list<LogAppender::ptr> _appenders;
	LogFormatter::ptr			_formatter;
	Logger::ptr					_root;
};

// -----------------------------------------------------------------------
// StdoutLogAppender
// -----------------------------------------------------------------------

/**
 * @brief Appender that writes log events to stdout.
 */
class StdoutLogAppender : public LogAppender {
  public:
	using ptr = std::shared_ptr<StdoutLogAppender>;

	void		log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;
	std::string toYamlString() override;
};

// -----------------------------------------------------------------------
// FileLogAppender
// -----------------------------------------------------------------------

/**
 * @brief Appender that writes log events to a file.
 *
 * Automatically re-opens the file if more than 3 seconds have elapsed
 * since the last write.
 */
class FileLogAppender : public LogAppender {
  public:
	using ptr = std::shared_ptr<FileLogAppender>;

	explicit FileLogAppender(const std::string& filename);

	void		log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;
	std::string toYamlString() override;

	/**
	 * @brief (Re-)open the log file.
	 * @return true on success.
	 */
	bool reopen();

  private:
	std::string	  _filename;
	std::ofstream _filestream;
	uint64_t	  _lastTime = 0;
};

// -----------------------------------------------------------------------
// LoggerManager
// -----------------------------------------------------------------------

/**
 * @brief Manages all named loggers and provides the root logger.
 *
 * Accessed through the LoggerMgr singleton.
 */
class LoggerManager {
  public:
	using MutexType = Spinlock;

	LoggerManager();

	/**
	 * @brief Get or create a named logger.
	 *
	 * New loggers inherit the root logger as fallback.
	 */
	Logger::ptr getLogger(const std::string& name);

	/** @brief Called once during startup to apply config. */
	void init();

	/** @brief The root logger (name = "root"). */
	Logger::ptr getRoot() const { return _root; }

	/** @brief Serialize all managed loggers to a YAML string. */
	std::string toYamlString();

  private:
	MutexType						   _mutex;
	std::map<std::string, Logger::ptr> _loggers;
	Logger::ptr						   _root;
};

/// @brief Singleton alias for LoggerManager.
using LoggerMgr = Singleton<LoggerManager>;

}  // namespace azzato

// Enable bitmask operations (|) for LogFormat
template <>
struct EnableBitMask<azzato::LogFormat> {
	static constexpr bool LogModule = true;
};
