#pragma once
#include <boost/lexical_cast.hpp>
#include <cstdint>
#include <cxxabi.h>
#include <format>
#include <google/protobuf/message.h>
#include <json/json.h>
#include <memory>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace azzato {
pid_t		getThreadId();
uint32_t	getFiberId();
std::string getHostName();
std::string getIPv4();

void		backtrace(std::vector<std::string>& bt, int size, int skip);
std::string backtraceToString(int size, int skip, const std::string& prefix);

uint64_t	getCurrentMS();
uint64_t	getCurrentUS();
std::string time2Str(time_t ts, const std::string& format = "%Y-%m-%d %H:%M:%S");
time_t		str2Time(const char* str, const char* format = "%Y-%m-%d %H:%M:%S");

std::string toUpper(const std::string& name);
std::string toLower(const std::string& name);
bool		yamlToJson(const YAML::Node& ynode, Json::Value& jnode);
bool		jsonToYaml(const Json::Value& jnode, YAML::Node& ynode);
std::string PBToJsonString(const google::protobuf::Message& message);

template <typename Iter>
std::string join(Iter begin, Iter end, const std::string& tag) {
	std::stringstream ss;
	for(Iter it = begin; it != end; ++it) {
		if(it != begin) {
			ss << tag;
		}
		ss << *it;
	}
	return ss.str();
}

////////// Maintenance implementation
// charconv support only higher than C++17
// template <class V, class Map, class K>
// V getParamValue(const Map& m, const K& k, const V& def = V()) {
// 	auto it = m.find(k);
// 	if(it == m.end()) {
// 		return def;
// 	}
// 	try {
// 		return boost::lexical_cast<V>(it->second);
// 	} catch(...) {
// 	}
// 	return def;
// }

// template <class V, class Map, class K>
// bool checkGetParamValue(const Map& m, const K& k, V& v) {
// 	auto it = m.find(k);
// 	if(it == m.end()) {
// 		return false;
// 	}
// 	try {
// 		v = boost::lexical_cast<V>(it->second);
// 		return true;
// 	} catch(...) {
// 	}
// 	return false;
// }
////////// Maintenance implementation
template <typename V, typename Map, typename K>
V getParamValue(const Map& m, const K& k, const V& def = V()) {
	auto it = m.find(k);
	if(it == m.end()) {
		return def;
	}
	std::ostringstream oss;
	oss << it->second;
	std::string str = oss.str();
	if constexpr(std::is_arithmetic_v<V>) {
		V result{};
		auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);
		if(ec == std::errc()) {
			return result;
		}
		return def;
	} else {
		std::istringstream iss(str);
		V				   result;
		if(iss >> result) {
			return result;
		}
		return def;
	}
}

// try to cast, if return true -> v will change to the result
template <typename V, typename Map, typename K>
bool checkGetParamValue(const Map& m, const K& k, V& v) {
	auto it = m.find(k);
	if(it == m.end()) {
		return false;
	}
	std::ostringstream oss;
	oss << it->second;
	std::string str = oss.str();
	if constexpr(std::is_arithmetic_v<V>) {
		auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), v);
		return ec == std::errc();
	} else {
		std::istringstream iss(str);
		return static_cast<bool>(iss >> v);
	}
}

template <class T>
const char* typeToName() {
	static const char* s_name = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, nullptr);
	return s_name;
}

//////// Concepts
template <typename T>
concept Pod = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;

template <typename T>
concept ContiguousContainer = requires(T& t) {
	{ t.data() } -> std::convertible_to<const std::remove_reference_t<decltype(t[0])>*>;
	{ t.size() } -> std::convertible_to<std::size_t>;
};
//////// Concepts

inline bool readFixFromStream(std::istream& is, char* data, uint64_t const& size) {
	uint64_t pos = 0;
	while(is && (pos < size)) {
		is.read(data + pos, size - pos);
		pos += is.gcount();
	}
	return pos == size;
}

template <Pod T>
bool readFromStream(std::istream& is, T& v) {
	return readFixFromStream(is, (char*)&v, sizeof(v));
}

template <ContiguousContainer T>
bool readFromStream(std::istream& is, T& v) {
	using Elem = std::remove_reference_t<decltype(*v.data())>;
	static_assert(std::is_trivially_copyable_v<Elem>, "element type must be trivially copyable");
	return readFixFromStream(is, (char*)v.data(), sizeof(Elem) * v.size());
}

template <typename T>
bool writeToStream(std::ostream& os, const T& v) {
	if(!os) {
		return false;
	}
	os.write((const char*)&v, sizeof(T));
	return (bool)os;
}

template <ContiguousContainer T>
bool writeToStream(std::ostream& os, const T& v) {
	if(!os) {
		return false;
	}
	using Elem = std::remove_reference_t<decltype(*v.data())>;
	os.write((const char*)v.data(), sizeof(Elem) * v.size());
	return (bool)os;
}

template <typename T>
bool writeToStreamWithSpeed(std::ostream& os, const T& v, const uint64_t& speed = -1) {
	if(os) {
		return writeFixToStreamWithSpeed(os, (const char*)&v, sizeof(T), speed);
	}
	return false;
}

template <ContiguousContainer T>
bool writeToStreamWithSpeed(std::ostream&	os,
							const T&		v,
							const uint64_t& speed			= -1,
							const uint64_t& min_duration_ms = 10) {
	if(os) {
		using Elem = std::remove_reference_t<decltype(*v.data())>;
		return writeFixToStreamWithSpeed(os, (const char*)v.data(), sizeof(Elem) * v.size(), speed);
	}
	return false;
}

bool readFixFromStreamWithSpeed(std::ifstream&	is,
								char*			data,
								uint64_t const& size,
								uint64_t const& speed = -1);

bool writeFixToStreamWithSpeed(std::ofstream&  os,
							   char const*	   data,
							   uint64_t const& size,
							   uint64_t const& speed = -1);

template <typename T>
bool readFromStreamWithSpeed(std::istream& is, const T& v, const uint64_t& speed = -1) {
	if(is) {
		return readFixFromStreamWithSpeed(is, (char*)&v, sizeof(T), speed);
	}
	return false;
}

template <ContiguousContainer T>
bool readFromStreamWithSpeed(std::istream& is, const T& v, const uint64_t& speed = -1) {
	if(is) {
		using Elem = std::remove_reference_t<decltype(*v.data())>;
		return readFixFromStreamWithSpeed(is, (char*)v.data(), sizeof(Elem) * v.size(), speed);
	}
	return false;
}

template <typename T>
int binarySearch(const T* arr, int length, const T& v) {
	int begin = 0, end = length - 1;
	while(begin <= end) {
		int m = (begin + end) / 2;
		if(v < arr[m])
			end = m - 1;
		else if(arr[m] < v)
			begin = m + 1;
		else
			return m;
	}
	return -begin - 1;
}

template <typename T>
void nop(T*) {}

template <typename T>
void deleteArray(T* v) {
	if(v) {
		delete[] v;
	}
}

class FSUtil {
  public:
	static void
	listAllFile(std::vector<std::string>& files, const std::string& path, const std::string& subfix);
	static bool		   mkdir(const std::string& dirname);
	static bool		   isRunningPidfile(const std::string& pidfile);
	static bool		   rm(const std::string& path);
	static bool		   mv(const std::string& from, const std::string& to);
	static bool		   realpath(const std::string& path, std::string& rpath);
	static bool		   symlink(const std::string& frm, const std::string& to);
	static bool		   unlink(const std::string& filename, bool exist = false);
	static std::string dirname(const std::string& filename);
	static std::string basename(const std::string& filename);
	static bool openForRead(std::ifstream& ifs, const std::string& filename, std::ios_base::openmode mode);
	static bool openForWrite(std::ofstream& ofs, const std::string& filename, std::ios_base::openmode mode);
};

class TypeUtil {
  public:
	static int8_t  toChar(const std::string& str);
	static int64_t atoi(const std::string& str);
	static double  atof(const std::string& str);
	static int8_t  toChar(const char* str);
	static int64_t atoi(const char* str);
	static double  atof(const char* str);
};

class StringUtil {
  public:
	template <typename... Args>
	static std::string format(std::format_string<Args...> fmt, const Args&... args);

	static std::string urlEncode(const std::string& str, bool space_as_plus = true);
	static std::string urlDecode(const std::string& str, bool space_as_plus = true);

	static std::string trim(const std::string& str, const std::string& delimit = " \t\r\n");
	static std::string trimLeft(const std::string& str, const std::string& delimit = " \t\r\n");
	static std::string trimRight(const std::string& str, const std::string& delimit = " \t\r\n");

	static std::string	wstringToString(const std::wstring& ws);
	static std::wstring stringToWString(const std::string& s);
};

// should use "{}" instead of "%"
template <typename... Args>
inline std::string StringUtil::format(std::format_string<Args...> fmt, Args const&... args) {
	return std::format(fmt, args...);
}

// GCC implement only
class Atomic {
  public:
	template <class T, class S = T>
	static T addFetch(volatile T& t, S v = 1) {
		return __sync_add_and_fetch(&t, (T)v);
	}

	template <class T, class S = T>
	static T subFetch(volatile T& t, S v = 1) {
		return __sync_sub_and_fetch(&t, (T)v);
	}

	template <class T, class S>
	static T orFetch(volatile T& t, S v) {
		return __sync_or_and_fetch(&t, (T)v);
	}

	template <class T, class S>
	static T andFetch(volatile T& t, S v) {
		return __sync_and_and_fetch(&t, (T)v);
	}

	template <class T, class S>
	static T xorFetch(volatile T& t, S v) {
		return __sync_xor_and_fetch(&t, (T)v);
	}

	template <class T, class S>
	static T nandFetch(volatile T& t, S v) {
		return __sync_nand_and_fetch(&t, (T)v);
	}

	template <class T, class S>
	static T fetchAdd(volatile T& t, S v = 1) {
		return __sync_fetch_and_add(&t, (T)v);
	}

	template <class T, class S>
	static T fetchSub(volatile T& t, S v = 1) {
		return __sync_fetch_and_sub(&t, (T)v);
	}

	template <class T, class S>
	static T fetchOr(volatile T& t, S v) {
		return __sync_fetch_and_or(&t, (T)v);
	}

	template <class T, class S>
	static T fetchAnd(volatile T& t, S v) {
		return __sync_fetch_and_and(&t, (T)v);
	}

	template <class T, class S>
	static T fetchXor(volatile T& t, S v) {
		return __sync_fetch_and_xor(&t, (T)v);
	}

	template <class T, class S>
	static T fetchNand(volatile T& t, S v) {
		return __sync_fetch_and_nand(&t, (T)v);
	}

	template <class T, class S>
	static T compareAndSwap(volatile T& t, S old_val, S new_val) {
		return __sync_val_compare_and_swap(&t, (T)old_val, (T)new_val);
	}

	template <class T, class S>
	static bool compareAndSwapBool(volatile T& t, S old_val, S new_val) {
		return __sync_bool_compare_and_swap(&t, (T)old_val, (T)new_val);
	}
};

class SpeedLimit {
  public:
	using ptr = std::shared_ptr<SpeedLimit>;
	SpeedLimit(uint32_t speed);
	void add(uint32_t v);

  private:
	uint32_t _speedLimit;  // 0 is coerced to UINT32_MAX (unlimited)
	float	 _byteCountPerMS;

	uint32_t _curCount;
	uint32_t _curSec;
};

template <typename T>
class SharedArray {
  public:
	explicit SharedArray(const uint64_t& size = 0, T* p = 0)
		: _size(size)
		, _ptr(p, deleteArray<T>) {}
	template <typename Deleter>
	SharedArray(const uint64_t& size, T* p, Deleter d)
		: _size(size)
		, _ptr(p, d){};

	SharedArray(const SharedArray& r)
		: _size(r._size)
		, _ptr(r._ptr) {}

	SharedArray& operator=(const SharedArray& r) {
		_size = r._size;
		_ptr  = r._ptr;
		return *this;
	}
	T&	 operator[](std::ptrdiff_t i) const { return _ptr.get()[i]; }
	T*	 get() const { return _ptr.get(); }
	bool unique() const { return _ptr.unique(); }
	long useCount() const { return _ptr.use_count(); }
	void swap(SharedArray& b) {
		std::swap(_size, b._size);
		_ptr.swap(b._ptr);
	}
	bool operator!() const { return !_ptr; }
	operator bool() const { return !!_ptr; }
	uint64_t size() const { return _size; }

  private:
	uint64_t		   _size;
	std::shared_ptr<T> _ptr;
};

template <class T>
void Slice(std::vector<std::vector<T>>& dst, const std::vector<T>& src, size_t size) {
	size_t left = src.size();
	size_t pos	= 0;
	while(left > size) {
		std::vector<T> tmp;
		tmp.reserve(size);
		for(size_t i = 0; i < size; ++i) {
			tmp.push_back(src[pos + i]);
		}
		pos += size;
		left -= size;
		dst.push_back(tmp);
	}

	if(left > 0) {
		std::vector<T> tmp;
		tmp.reserve(left);
		for(size_t i = 0; i < left; ++i) {
			tmp.push_back(src[pos + i]);
		}
		dst.push_back(tmp);
	}
}

}  // namespace azzato
