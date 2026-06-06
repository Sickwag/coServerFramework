#include "utils/util.h"
#include "utils/json_util.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <cxxabi.h>
#include <dirent.h>
#include <execinfo.h>
#include <filesystem>
#include <fstream>
#include <ifaddrs.h>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>

namespace {

in_addr_t getIPv4Inet() {
	struct ifaddrs* ifas = nullptr;
	struct ifaddrs* ifa	 = nullptr;

	in_addr_t localhost	 = inet_addr("127.0.0.1");
	if(getifaddrs(&ifas)) {
		// TODO: SYLAR_LOG_ERROR(g_logger) << "getifaddrs errno=" << errno << " errstr=" << strerror(errno);
		return localhost;
	}

	in_addr_t ipv4 = localhost;

	for(ifa = ifas; ifa && ifa->ifa_addr; ifa = ifa->ifa_next) {
		if(ifa->ifa_addr->sa_family != AF_INET) {
			continue;
		}
		if(!strncasecmp(ifa->ifa_name, "lo", 2)) {
			continue;
		}
		ipv4 = ((struct sockaddr_in*)ifa->ifa_addr)->sin_addr.s_addr;
		if(ipv4 == localhost) {
			continue;
		}
	}
	if(ifas != nullptr) {
		freeifaddrs(ifas);
	}
	return ipv4;  // return a ip address not local loop
}

std::string _getIPv4() {
	// std::shared_ptr<char> ipv4(new char[INET_ADDRSTRLEN], azzato::deleteArray<char>);
	auto ipv4 = std::make_shared<char[]>(INET_ADDRSTRLEN);
	memset(ipv4.get(), 0, INET_ADDRSTRLEN);
	auto ia = getIPv4Inet();
	inet_ntop(AF_INET, &ia, ipv4.get(), INET_ADDRSTRLEN);
	return ipv4.get();
}

////////// Maintenance implementation
int __lstat(const char* file, struct stat* st = nullptr) {
	struct stat lst;
	int			ret = lstat(file, &lst);
	if(st) {
		*st = lst;
	}
	return ret;
}

int __mkdir(const char* dirname) {
	if(access(dirname, F_OK) == 0) {
		return 0;
	}
	return mkdir(dirname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}
////////// Maintenance implementation

////////// for url encode/decode
// clang-format off
constexpr char uriChars[256] = {
	/* 0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0,
	/* 64 */
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0,
	/* 128 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 192 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

constexpr char xdigitChars[256] = {
	0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,
	0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
// clang-format on
////////// for url encode/decode
void serializeUnknowFieldSet(const google::protobuf::UnknownFieldSet& ufs, Json::Value& jnode) {
	std::map<int, std::vector<Json::Value>> kvs;
	for(int i = 0; i < ufs.field_count(); i++) {
		auto const& uf = ufs.field(i);
		using UF	   = google::protobuf::UnknownField;
		switch(uf.type()) {
		case UF::TYPE_VARINT:
			kvs[uf.number()].push_back((Json::Int64)uf.varint());
			break;
		case UF::TYPE_FIXED32:
			kvs[uf.number()].push_back((Json::UInt)uf.fixed32());
			break;
		case UF::TYPE_FIXED64:
			kvs[uf.number()].push_back((Json::UInt64)uf.fixed64());
			break;
		case UF::TYPE_LENGTH_DELIMITED:
			google::protobuf::UnknownFieldSet temp;
			std::string						  v(uf.length_delimited());
			if(!v.empty() && temp.ParseFromString(v)) {
				Json::Value vv;
				serializeUnknowFieldSet(temp, vv);
				kvs[uf.number()].push_back(
					vv);  // Binary can also be parsed, indicating that it is a nested data structure.
			} else {
				kvs[uf.number()].push_back(v);	// Add to the message as a normal string
			}
			break;
		}
	}

	// transfrom parsed data to json
	for(auto& i : kvs) {
		if(i.second.size() > 1) {
			for(auto& n : i.second) {
				jnode[std::to_string(i.first)].append(n);  // make a json array obj
			}
		} else {
			jnode[std::to_string(i.first)] = i.second[0];  // simple single json element
		}
	}
}

static void serializeMessage(const google::protobuf::Message& message, Json::Value& jnode) {
	const google::protobuf::Descriptor* descriptor = message.GetDescriptor();
	const google::protobuf::Reflection* reflection = message.GetReflection();

	for(int i = 0; i < descriptor->field_count(); ++i) {
		const google::protobuf::FieldDescriptor* field = descriptor->field(i);

		if(field->is_repeated()) {
			if(!reflection->FieldSize(message, field)) {
				continue;
			}
		} else {
			if(!reflection->HasField(message, field)) {
				continue;
			}
		}

		if(field->is_repeated()) {
			switch(field->cpp_type()) {
#define XX(cpptype, method, valuetype, jsontype)                               \
	case google::protobuf::FieldDescriptor::CPPTYPE_##cpptype: {               \
		int size = reflection->FieldSize(message, field);                      \
		for(int n = 0; n < size; ++n) {                                        \
			jnode[std::string(field->name())].append(                          \
				(jsontype)reflection->GetRepeated##method(message, field, n)); \
		}                                                                      \
		break;                                                                 \
	}
				XX(INT32, Int32, int32_t, Json::Int);
				XX(UINT32, UInt32, uint32_t, Json::UInt);
				XX(FLOAT, Float, float, double);
				XX(DOUBLE, Double, double, double);
				XX(BOOL, Bool, bool, bool);
				XX(INT64, Int64, int64_t, Json::Int64);
				XX(UINT64, UInt64, uint64_t, Json::UInt64);
#undef XX
			case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
				int size = reflection->FieldSize(message, field);
				for(int n = 0; n < size; ++n) {
					jnode[std::string(field->name())].append(
						reflection->GetRepeatedEnum(message, field, n)->number());
				}
				break;
			}
			case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
				int size = reflection->FieldSize(message, field);
				for(int n = 0; n < size; ++n) {
					jnode[std::string(field->name())].append(
						reflection->GetRepeatedString(message, field, n));
				}
				break;
			}
			case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
				int size = reflection->FieldSize(message, field);
				for(int n = 0; n < size; ++n) {
					Json::Value vv;
					serializeMessage(reflection->GetRepeatedMessage(message, field, n), vv);
					jnode[std::string(field->name())].append(vv);
				}
				break;
			}
			}
			continue;
		}

		switch(field->cpp_type()) {
#define XX(cpptype, method, valuetype, jsontype)                                               \
	case google::protobuf::FieldDescriptor::CPPTYPE_##cpptype: {                               \
		jnode[std::string(field->name())] = (jsontype)reflection->Get##method(message, field); \
		break;                                                                                 \
	}
			XX(INT32, Int32, int32_t, Json::Int);
			XX(UINT32, UInt32, uint32_t, Json::UInt);
			XX(FLOAT, Float, float, double);
			XX(DOUBLE, Double, double, double);
			XX(BOOL, Bool, bool, bool);
			XX(INT64, Int64, int64_t, Json::Int64);
			XX(UINT64, UInt64, uint64_t, Json::UInt64);
#undef XX
		case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
			jnode[std::string(field->name())] = reflection->GetEnum(message, field)->number();
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
			jnode[std::string(field->name())] = reflection->GetString(message, field);
			break;
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
			serializeMessage(reflection->GetMessage(message, field), jnode[std::string(field->name())]);
			break;
		}
		}
	}

	const auto& ufs = reflection->GetUnknownFields(message);
	serializeUnknowFieldSet(ufs, jnode);
}

}  // namespace

namespace azzato {
namespace fs = std::filesystem;

pid_t getThreadId() { return syscall(SYS_gettid); }

uint32_t getFiberId() {
	// TODO: get fiber id to return
	return 0;
}

std::string getHostName() {
	// std::shared_ptr<char> host(new char[512], azzato::deleteArray<char>);
	auto host = std::make_shared<char[]>(512);	// use automatic delete for array type
	memset(host.get(), 0, 512);
	gethostname(host.get(), 511);
	return host.get();
}

std::string getIPv4() {
	static std::string const ip = _getIPv4();  // local IP will not change generally
	return ip;
}

std::string demangle(char const* str) {
	size_t		size   = 0;
	int			status = 0;
	std::string rt;
	rt.resize(256);
	if(1 == sscanf(str, "%*[^(]%*[^_]%255[^)+]", &rt[0])) {
		char* v = abi::__cxa_demangle(&rt[0], nullptr, &size, &status);
		if(v) {
			std::string result(v);
			free(v);
			return result;
		}
	}
	if(1 == sscanf(str, "%255s", &rt[0])) {
		return rt;
	}
	return str;
}

void backtrace(std::vector<std::string>& bt, int size, int skip) {
	void** array   = (void**)malloc(sizeof(void*) * size);
	size_t s	   = ::backtrace(array, size);
	char** strings = backtrace_symbols(array, s);
	if(strings == nullptr) {
		// TODO: AZZATO_LOG
		return;
	}
	// for(size_t i = skip; i < s; i++) {
	// 	bt.push_back(demangle(strings[i]));
	// }
	std::for_each(strings + skip, strings + s, [&bt](char* name) { bt.push_back(name); });
	free(strings);
	free(array);
}

std::string backtraceToString(int size, int skip, const std::string& prefix) {
	std::vector<std::string> bt;
	backtrace(bt, size, skip);
	std::stringstream ss;
	std::for_each(
		bt.begin(), bt.end(), [&ss, &prefix](std::string const& str) { ss << prefix << str << std::endl; });
	return ss.str();
}

uint64_t getCurrentMS() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000ul + tv.tv_usec / 1000;
}

uint64_t getCurrentUS() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 * 1000ul + tv.tv_usec;
}

std::string time2Str(time_t ts, const std::string& format) {
	struct tm tm;
	localtime_r(&ts, &tm);
	char buf[64];
	strftime(buf, sizeof(buf), format.c_str(), &tm);
	return buf;
}

time_t str2Time(const char* str, const char* format) {
	struct tm t;
	memset(&t, 0, sizeof(t));
	if(!strptime(str, format, &t)) {
		return 0;
	}
	return mktime(&t);
}

std::string toUpper(const std::string& name) {
	std::string rt = name;
	std::transform(rt.begin(), rt.end(), rt.begin(), ::toupper);
	return rt;
}

std::string toLower(const std::string& name) {
	std::string rt = name;
	std::transform(rt.begin(), rt.end(), rt.begin(), ::tolower);
	return rt;
}

bool yamlToJson(YAML::Node const& ynode, Json::Value& jnode) {
	try {
		if(ynode.IsScalar()) {
			Json::Value v(ynode.Scalar());
			jnode.swap(v);
			return true;
		}
		if(ynode.IsSequence()) {
			for(size_t i = 0; i < ynode.size(); ++i) {
				Json::Value v;
				if(yamlToJson(ynode[i], v)) {
					jnode.append(v);
				} else {
					return false;
				}
			}
		} else if(ynode.IsMap()) {
			for(auto it = ynode.begin(); it != ynode.end(); ++it) {
				Json::Value v;
				if(yamlToJson(it->second, v)) {
					jnode[it->first.Scalar()] = v;
				} else {
					return false;
				}
			}
		}
	} catch(...) {
		return false;
	}
	return true;
}

bool jsonToYaml(Json::Value const& jnode, YAML::Node& ynode) {
	try {
		if(jnode.isArray()) {
			for(int i = 0; i < (int)jnode.size(); ++i) {
				YAML::Node n;
				if(jsonToYaml(jnode[i], n)) {
					ynode.push_back(n);
				} else {
					return false;
				}
			}
		} else if(jnode.isObject()) {
			for(auto it = jnode.begin(); it != jnode.end(); ++it) {
				YAML::Node n;
				if(jsonToYaml(*it, n)) {
					ynode[it.name()] = n;
				} else {
					return false;
				}
			}
		} else {
			ynode = jnode.asString();
		}
	} catch(...) {
		return false;
	}
	return true;
}

std::string PBToJsonString(google::protobuf::Message const& message) {
	Json::Value jnode;
	serializeMessage(message, jnode);
	return JsonUtil::toString(jnode);
}

bool readFixFromStreamWithSpeed(std::ifstream& is, char* data, uint64_t const& size, uint64_t const& speed) {
	SpeedLimit::ptr limit;
	if(dynamic_cast<std::ifstream*>(&is)) {
		limit.reset(new SpeedLimit(speed));
	}

	uint64_t offset = 0;
	uint64_t per	= std::max((uint64_t)ceil(speed / 100.0), (uint64_t)1024 * 64);
	while(is && (offset < size)) {
		uint64_t s = size - offset > per ? per : size - offset;
		is.read(data + offset, s);
		offset += is.gcount();

		if(limit) {
			limit->add(is.gcount());
		}
	}
	return offset == size;
}

bool writeFixToStreamWithSpeed(std::ofstream&  os,
							   char const*	   data,
							   uint64_t const& size,
							   uint64_t const& speed) {
	SpeedLimit::ptr limit;
	if(dynamic_cast<std::ofstream*>(&os)) {
		limit.reset(new SpeedLimit(speed));
	}

	uint64_t offset = 0;
	uint64_t per	= std::max((uint64_t)ceil(speed / 100.0), (uint64_t)1024 * 64);
	while(os && (offset < size)) {
		uint64_t s = size - offset > per ? per : size - offset;
		os.write(data + offset, s);
		offset += s;

		if(limit) {
			limit->add(s);
		}
	}

	return offset == size;
}

void FSUtil::listAllFile(std::vector<std::string>& files,
						 std::string const&		   path,
						 std::string const&		   subfix) {

	////////// Maintenance implementation
	// if(access(path.c_str(), F_OK) != 0) {
	// 	return;
	// }
	// DIR *dir = opendir(path.c_str()); // #include <dirent.h>
	// if (dir != nullptr) {
	// 	return;
	// }
	// struct dirent *dp = nullptr;
	// while((dp=readdir(dir)) != nullptr){
	// 	if(dp->d_type == DT_DIR){
	// 		if(!strcmp(dp->d_name, ".") || strcmp(dp->d_name, "..")){
	// 			continue;
	// 		}
	//         listAllFile(files, path, subfix);
	//     }else if(dp->d_type == DT_REG){
	//         std::string filename(dp->d_name);
	// 		if(subfix.empty()){
	//             files.push_back(path + "/" + filename);
	//         }else{
	// 			if(filename.size() < subfix.size()){
	// 				continue;
	//             }
	// 			if(fs::path(filename).extension() == subfix){
	//                 files.push_back(path + "/" + filename);
	//             }
	//         }

	//     }
	// }
	// closedir(dir);
	////////// Maintenance implementation
	try {
		for(const auto& entry :
			fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied)) {
			if(fs::is_regular_file(entry)) {
				const auto& p = entry.path();
				if(subfix.empty()) {
					files.emplace_back(entry.path().generic_string());
				} else {
					std::string ext = p.extension().string();
					if(subfix == ext || (!ext.empty() && ext.substr(1) == subfix)) {
						files.emplace_back(entry.path().generic_string());
					}
				}
			}
		}

	} catch(const fs::filesystem_error& e) {
		// TODO: AZZATO_LOG_ROOT
	}
}

bool FSUtil::mkdir(std::string const& dirname) {
	////////// Maintenance implementation
	// if(__lstat(dirname.c_str()) == 0) {
	// 	return true;
	// }
	// char* path = strdup(dirname.c_str());
	// char* ptr  = strchr(path + 1, '/');
	// do {
	// 	for(; ptr; *ptr = '/', ptr = strchr(ptr + 1, '/')) {
	// 		*ptr = '\0';
	// 		if(__mkdir(path) != 0) {
	// 			break;
	// 		}
	// 	}
	// 	if(ptr != nullptr) {
	// 		break;
	// 	} else if(__mkdir(path) != 0) {
	// 		break;
	// 	}
	// 	free(path);
	// 	return true;
	// } while(0);
	// free(path);
	// return false;
	////////// Maintenance implementation
	return fs::create_directories(dirname);
}

bool FSUtil::isRunningPidfile(const std::string& pidfile) {
	////////// Maintenance implementation
	// if(__lstat(pidfile.c_str()) != 0) {
	// 	return false;
	// }
	// std::ifstream ifs(pidfile);
	// std::string	  line;
	// if(!ifs || !std::getline(ifs, line)) {
	// 	return false;
	// }
	// if(line.empty()) {
	// 	return false;
	// }
	// pid_t pid = atoi(line.c_str());
	// if(pid <= 1) {
	// 	return false;
	// }
	// if(kill(pid, 0) != 0) {
	// 	return false;
	// }
	// return true;
	////////// Maintenance implementation

	try {
		if(!fs::exists(pidfile)) {
			return false;
		}

		std::ifstream ifs(pidfile);
		std::string	  line;
		if(!std::getline(ifs, line)) {
			return false;
		}
		if(line.empty()) {
			return false;
		}

		pid_t pid = std::stoi(line);
		if(pid <= 1) {
			return false;
		}

		return (kill(pid, 0) == 0);
	} catch(...) {
		return false;
	}
}

bool FSUtil::unlink(const std::string& filename, bool exist) {
	////////// Maintenance implementation
	// if(!exist && __lstat(filename.c_str())) {
	// 	return true;
	// }
	// return ::unlink(filename.c_str()) == 0;
	////////// Maintenance implementation

	std::error_code ec;

	if(!exist && !fs::exists(filename, ec)) {
		return true;
	}

	return fs::remove(filename, ec);
}

bool FSUtil::rm(const std::string& path) {
	////////// Maintenance implementation
	// struct stat st;
	// if(lstat(path.c_str(), &st)) {
	// 	return true;
	// }
	// if(!(st.st_mode & S_IFDIR)) {
	// 	return Unlink(path);
	// }

	// DIR* dir = opendir(path.c_str());
	// if(!dir) {
	// 	return false;
	// }

	// bool		   ret = true;
	// struct dirent* dp  = nullptr;
	// while((dp = readdir(dir))) {
	// 	if(!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
	// 		continue;
	// 	}
	// 	std::string dirname = path + "/" + dp->d_name;
	// 	ret					= Rm(dirname);
	// }
	// closedir(dir);
	// if(::rmdir(path.c_str())) {
	// 	ret = false;
	// }
	// return ret;
	////////// Maintenance implementation

	std::error_code ec;

	try {
		if(!fs::exists(path, ec)) {
			return true;
		}

		return fs::remove_all(path, ec) > 0;
	} catch(...) {
		return false;
	}
}

bool FSUtil::mv(const std::string& from, const std::string& to) {
	////////// Maintenance implementation
	// if(!Rm(to)) {
	// 	return false;
	// }
	// return rename(from.c_str(), to.c_str()) == 0;
	////////// Maintenance implementation

	std::error_code ec;

	try {
		if(fs::exists(to, ec)) {
			fs::remove(to, ec);
		}

		fs::rename(from, to, ec);
		return !ec;
	} catch(...) {
		return false;
	}
}

bool FSUtil::realpath(const std::string& path, std::string& rpath) {
	////////// Maintenance implementation
	// if(__lstat(path.c_str())) {
	// 	return false;
	// }
	// char* ptr = ::realpath(path.c_str(), nullptr);
	// if(nullptr == ptr) {
	// 	return false;
	// }
	// std::string(ptr).swap(rpath);
	// free(ptr);
	// return true;
	////////// Maintenance implementation

	std::error_code ec;

	try {
		if(!fs::exists(path, ec)) {
			return false;
		}

		fs::path canonical_path = fs::canonical(path, ec);
		if(ec) {
			return false;
		}

		rpath = canonical_path.string();
		return true;
	} catch(...) {
		return false;
	}
}

bool FSUtil::symlink(const std::string& from, const std::string& to) {
	////////// Maintenance implementation
	// if(!Rm(to)) {
	// 	return false;
	// }
	// return ::symlink(from.c_str(), to.c_str()) == 0;
	////////// Maintenance implementation

	std::error_code ec;

	try {
		if(fs::exists(to, ec)) {
			fs::remove(to, ec);
		}

		fs::create_symlink(from, to, ec);
		return !ec;
	} catch(...) {
		return false;
	}
}

std::string FSUtil::dirname(const std::string& filename) {
	////////// Maintenance implementation
	// if(filename.empty()) {
	// 	return ".";
	// }
	// auto pos = filename.rfind('/');
	// if(pos == 0) {
	// 	return "/";
	// } else if(pos == std::string::npos) {
	// 	return ".";
	// } else {
	// 	return filename.substr(0, pos);
	// }
	////////// Maintenance implementation

	if(filename.empty()) {
		return ".";
	}

	fs::path p(filename);
	if(p.has_parent_path()) {
		return p.parent_path().string();
	}

	return ".";
}

std::string FSUtil::basename(const std::string& filename) {
	////////// Maintenance implementation
	// if(filename.empty()) {
	// 	return filename;
	// }
	// auto pos = filename.rfind('/');
	// if(pos == std::string::npos) {
	// 	return filename;
	// } else {
	// 	return filename.substr(pos + 1);
	// }
	////////// Maintenance implementation

	if(filename.empty()) {
		return filename;
	}

	fs::path p(filename);
	return p.filename().string();
}

bool FSUtil::openForRead(std::ifstream& ifs, const std::string& filename, std::ios_base::openmode mode) {
	////////// Maintenance implementation
	// ifs.open(filename.c_str(), mode);
	// return ifs.is_open();
	////////// Maintenance implementation

	std::error_code ec;

	if(!fs::exists(filename, ec)) {
		return false;
	}

	ifs.open(filename, mode);
	return ifs.is_open();
}

bool FSUtil::openForWrite(std::ofstream& ofs, const std::string& filename, std::ios_base::openmode mode) {
	////////// Maintenance implementation
	// ofs.open(filename.c_str(), mode);
	// if(!ofs.is_open()) {
	// 	std::string dir = Dirname(filename);
	// 	Mkdir(dir);
	// 	ofs.open(filename.c_str(), mode);
	// }
	// return ofs.is_open();
	////////// Maintenance implementation

	std::error_code ec;
	fs::path		file_path(filename);
	fs::path		dir_path = file_path.parent_path();

	if(!dir_path.empty() && !fs::exists(dir_path, ec)) {
		fs::create_directories(dir_path, ec);
	}

	ofs.open(filename, mode);
	return ofs.is_open();
}

int8_t TypeUtil::toChar(const std::string& str) {
	if(str.empty()) {
		return 0;
	}
	return *str.begin();
}

int64_t TypeUtil::atoi(const std::string& str) {
	if(str.empty()) {
		return 0;
	}
	return strtoull(str.c_str(), nullptr, 10);
}

double TypeUtil::atof(const std::string& str) {
	if(str.empty()) {
		return 0;
	}
	return atof(str.c_str());
}

int8_t TypeUtil::toChar(const char* str) {
	if(str == nullptr) {
		return 0;
	}
	return str[0];
}

int64_t TypeUtil::atoi(const char* str) {
	if(str == nullptr) {
		return 0;
	}
	return strtoull(str, nullptr, 10);
}

double TypeUtil::atof(const char* str) {
	if(str == nullptr) {
		return 0;
	}
	return atof(str);
}

#define CHAR_IS_UNRESERVED(c) (uriChars[(unsigned char)(c)])

std::string StringUtil::urlEncode(std::string const& str, bool space_as_plus) {
	static const char* hexdigits = "0123456789ABCDEF";
	std::string*	   ss		 = nullptr;
	const char*		   end		 = str.c_str() + str.length();
	for(const char* c = str.c_str(); c < end; ++c) {
		if(!CHAR_IS_UNRESERVED(*c)) {
			if(!ss) {
				ss = new std::string;
				ss->reserve(str.size() * 1.2);
				ss->append(str.c_str(), c - str.c_str());
			}
			if(*c == ' ' && space_as_plus) {
				ss->append(1, '+');
			} else {
				ss->append(1, '%');
				ss->append(1, hexdigits[(uint8_t)*c >> 4]);
				ss->append(1, hexdigits[*c & 0xf]);
			}
		} else if(ss) {
			ss->append(1, *c);
		}
	}
	if(!ss) {
		return str;
	} else {
		std::string rt = *ss;
		delete ss;
		return rt;
	}
}

std::string StringUtil::urlDecode(std::string const& str, bool space_as_plus) {
	std::string* ss	 = nullptr;
	const char*	 end = str.c_str() + str.length();
	for(const char* c = str.c_str(); c < end; ++c) {
		if(*c == '+' && space_as_plus) {
			if(!ss) {
				ss = new std::string;
				ss->append(str.c_str(), c - str.c_str());
			}
			ss->append(1, ' ');
		} else if(*c == '%' && (c + 2) < end && isxdigit(*(c + 1)) && isxdigit(*(c + 2))) {
			if(!ss) {
				ss = new std::string;
				ss->append(str.c_str(), c - str.c_str());
			}
			ss->append(1, (char)(xdigitChars[(int)*(c + 1)] << 4 | xdigitChars[(int)*(c + 2)]));
			c += 2;
		} else if(ss) {
			ss->append(1, *c);
		}
	}
	if(!ss) {
		return str;
	} else {
		std::string rt = *ss;
		delete ss;
		return rt;
	}
}

std::string StringUtil::trim(std::string const& str, std::string const& delimit) {
	auto begin = str.find_first_not_of(delimit);
	if(begin == std::string::npos) {
		return "";
	}
	auto end = str.find_last_not_of(delimit);
	return str.substr(begin, end - begin + 1);
}

std::string StringUtil::trimLeft(std::string const& str, std::string const& delimit) {
	auto begin = str.find_first_not_of(delimit);
	if(begin == std::string::npos) {
		return "";
	}
	return str.substr(begin);
}

std::string StringUtil::trimRight(std::string const& str, std::string const& delimit) {
	auto end = str.find_last_not_of(delimit);
	if(end == std::string::npos) {
		return "";
	}
	return str.substr(0, end);
}

std::string StringUtil::wstringToString(std::wstring const& ws) {
	std::string	   str_locale  = setlocale(LC_ALL, "");
	const wchar_t* wch_src	   = ws.c_str();
	size_t		   n_dest_size = wcstombs(NULL, wch_src, 0) + 1;
	char*		   ch_dest	   = new char[n_dest_size];
	memset(ch_dest, 0, n_dest_size);
	wcstombs(ch_dest, wch_src, n_dest_size);
	std::string str_result = ch_dest;
	delete[] ch_dest;
	setlocale(LC_ALL, str_locale.c_str());
	return str_result;
}

std::wstring StringUtil::stringToWString(std::string const& s) {
	std::string str_locale	= setlocale(LC_ALL, "");
	const char* chSrc		= s.c_str();
	size_t		n_dest_size = mbstowcs(NULL, chSrc, 0) + 1;
	wchar_t*	wch_dest	= new wchar_t[n_dest_size];
	wmemset(wch_dest, 0, n_dest_size);
	mbstowcs(wch_dest, chSrc, n_dest_size);
	std::wstring wstr_result = wch_dest;
	delete[] wch_dest;
	setlocale(LC_ALL, str_locale.c_str());
	return wstr_result;
}

SpeedLimit::SpeedLimit(uint32_t speed)
	: _speedLimit(speed)
	, _byteCountPerMS(0)
	, _curCount(0)
	, _curSec(0) {
	if(speed == 0) {
		_speedLimit = (uint32_t)-1;
	}
	_byteCountPerMS = _speedLimit / 1000.0;

	// avoiding divided by zero
	_byteCountPerMS = (_byteCountPerMS == 0) ? (float)-1 : _byteCountPerMS;
}

void SpeedLimit::add(uint32_t v) {
	uint64_t curMS = getCurrentMS();
	if(curMS / 1000 != _curSec) {
		_curSec	  = curMS / 1000;
		_curCount = v;
		return;
	}
	_curCount += v;
	int usedMS	= curMS % 1000;
	int limitMS = _curCount / _byteCountPerMS;

	if(usedMS < limitMS) {
		usleep(1000 * (limitMS - usedMS));
	}
}
}  // namespace azzato
