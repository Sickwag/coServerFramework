#pragma once

#include "log.h"
#include "mutex.h"
#include "utils/util.h"
#include <boost/lexical_cast.hpp>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace azzato {
class ConfigVarBase {
  public:
	using ptr = std::shared_ptr<ConfigVarBase>;

	ConfigVarBase(const std::string& name, const std::string& description = "");
	virtual ~ConfigVarBase() = default;

	const std::string& getName() const { return _name; }
	const std::string& getDescription() const { return _description; }

	/**
	 * @brief convert C++ memory `_val` to YAML string
	 */
	virtual std::string toString()						   = 0;

	/**
	 * @brief convert YAML string to C++ memory date `_val`
	 */
	virtual bool		fromString(const std::string& val) = 0;
	virtual std::string getTypeName() const				   = 0;

  private:
	std::string _name;
	std::string _description;
};

/**
 * @brief 类型转换模板类(S 源类型, T 目标类型)
 */
template <class Source, class Target>
class LexicalCast {
  public:
	/**
	 * @brief 类型转换
	 * @param[in] v 源类型值
	 * @return 返回v转换后的目标类型
	 * @exception 当类型不可转换时从boost库内抛出异常
	 */
	T operator()(const Source& s) { return boost::lexical_cast<Target>(s); }
};

/**
 * @brief 类型转换模板类偏特化(YAML String 转换成 std::vector<T>)
 */
template <class T>
class LexicalCast<std::string, std::vector<T>> {
  public:
	std::vector<T> operator()(const std::string& v) {
		YAML::Node				node = YAML::Load(v);
		typename std::vector<T> vec;
		std::stringstream		ss;
		for(size_t i = 0; i < node.size(); ++i) {
			ss.str("");
			ss << node[i];
			vec.push_back(LexicalCast<std::string, T>()(ss.str()));
		}
		return vec;
	}
};

/**
 * @brief 类型转换模板类偏特化(std::vector<T> 转换成 YAML String)
 */
template <class T>
class LexicalCast<std::vector<T>, std::string> {
  public:
	std::string operator()(const std::vector<T>& v) {
		YAML::Node node(YAML::NodeType::Sequence);
		for(auto& i : v) {
			node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
		}
		std::stringstream ss;
		ss << node;
		return ss.str();
	}
};

/**
 * @brief 类型转换模板类偏特化(YAML String 转换成 std::list<T>)
 */
template <class T>
class LexicalCast<std::string, std::list<T>> {
  public:
	std::list<T> operator()(const std::string& v) {
		YAML::Node			  node = YAML::Load(v);
		typename std::list<T> vec;
		std::stringstream	  ss;
		for(size_t i = 0; i < node.size(); ++i) {
			ss.str("");
			ss << node[i];
			vec.push_back(LexicalCast<std::string, T>()(ss.str()));
		}
		return vec;
	}
};

/**
 * @brief 类型转换模板类偏特化(std::list<T> 转换成 YAML String)
 */
template <class T>
class LexicalCast<std::list<T>, std::string> {
  public:
	std::string operator()(const std::list<T>& v) {
		YAML::Node node(YAML::NodeType::Sequence);
		for(auto& i : v) {
			node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
		}
		std::stringstream ss;
		ss << node;
		return ss.str();
	}
};

/**
 * @brief 类型转换模板类偏特化(YAML String 转换成 std::set<T>)
 */
template <class T>
class LexicalCast<std::string, std::set<T>> {
  public:
	std::set<T> operator()(const std::string& v) {
		YAML::Node			 node = YAML::Load(v);
		typename std::set<T> vec;
		std::stringstream	 ss;
		for(size_t i = 0; i < node.size(); ++i) {
			ss.str("");
			ss << node[i];
			vec.insert(LexicalCast<std::string, T>()(ss.str()));
		}
		return vec;
	}
};

/**
 * @brief 类型转换模板类偏特化(std::set<T> 转换成 YAML String)
 */
template <class T>
class LexicalCast<std::set<T>, std::string> {
  public:
	std::string operator()(const std::set<T>& v) {
		YAML::Node node(YAML::NodeType::Sequence);
		for(auto& i : v) {
			node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
		}
		std::stringstream ss;
		ss << node;
		return ss.str();
	}
};

/**
 * @brief 类型转换模板类偏特化(YAML String 转换成 std::unordered_set<T>)
 */
template <class T>
class LexicalCast<std::string, std::unordered_set<T>> {
  public:
	std::unordered_set<T> operator()(const std::string& v) {
		YAML::Node					   node = YAML::Load(v);
		typename std::unordered_set<T> vec;
		std::stringstream			   ss;
		for(size_t i = 0; i < node.size(); ++i) {
			ss.str("");
			ss << node[i];
			vec.insert(LexicalCast<std::string, T>()(ss.str()));
		}
		return vec;
	}
};

/**
 * @brief 类型转换模板类偏特化(std::unordered_set<T> 转换成 YAML String)
 */
template <class T>
class LexicalCast<std::unordered_set<T>, std::string> {
  public:
	std::string operator()(const std::unordered_set<T>& v) {
		YAML::Node node(YAML::NodeType::Sequence);
		for(auto& i : v) {
			node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
		}
		std::stringstream ss;
		ss << node;
		return ss.str();
	}
};

/**
 * @brief 类型转换模板类偏特化(YAML String 转换成 std::map<std::string, T>)
 */
template <class T>
class LexicalCast<std::string, std::map<std::string, T>> {
  public:
	std::map<std::string, T> operator()(const std::string& v) {
		YAML::Node						  node = YAML::Load(v);
		typename std::map<std::string, T> vec;
		std::stringstream				  ss;
		for(auto it = node.begin(); it != node.end(); ++it) {
			ss.str("");
			ss << it->second;
			vec.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
		}
		return vec;
	}
};

/**
 * @brief 类型转换模板类偏特化(std::map<std::string, T> 转换成 YAML String)
 */
template <class T>
class LexicalCast<std::map<std::string, T>, std::string> {
  public:
	std::string operator()(const std::map<std::string, T>& v) {
		YAML::Node node(YAML::NodeType::Map);
		for(auto& i : v) {
			node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
		}
		std::stringstream ss;
		ss << node;
		return ss.str();
	}
};

/**
 * @brief 类型转换模板类偏特化(YAML String 转换成 std::unordered_map<std::string, T>)
 */
template <class T>
class LexicalCast<std::string, std::unordered_map<std::string, T>> {
  public:
	std::unordered_map<std::string, T> operator()(const std::string& v) {
		YAML::Node									node = YAML::Load(v);
		typename std::unordered_map<std::string, T> vec;
		std::stringstream							ss;
		for(auto it = node.begin(); it != node.end(); ++it) {
			ss.str("");
			ss << it->second;
			vec.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
		}
		return vec;
	}
};

/**
 * @brief 类型转换模板类偏特化(std::unordered_map<std::string, T> 转换成 YAML String)
 */
template <class T>
class LexicalCast<std::unordered_map<std::string, T>, std::string> {
  public:
	std::string operator()(const std::unordered_map<std::string, T>& v) {
		YAML::Node node(YAML::NodeType::Map);
		for(auto& i : v) {
			node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
		}
		std::stringstream ss;
		ss << node;
		return ss.str();
	}
};

template <typename T,
		  typename FromStr = LexicalCast<std::string, T>,
		  typename ToStr   = LexicalCast<T, std::string>>
class ConfigVar : public ConfigVarBase {
  public:
	using RWMutexType	   = RWMutex;
	using ptr			   = std::shared_ptr<ConfigVar>;
	using onChangeCallback = std::function<void(const T& oldValue, const T& newValue)>;

	ConfigVar(const std::string& name, const T& defaultValue, const std::string& description = "")
		: ConfigVarBase(name, description)
		, _val(defaultValue) {}

	std::string toString() override;
	bool		fromString(const std::string& val) override;
	const T		getValue() const;
	void		setValue(const T& v);
	std::string getTypeName() const override { return typeToName<T>(); }

	uint64_t addListener(onChangeCallback callback);
	void	 deleteListener(uint64_t key);
	void	 clearListener();

  private:
	RWMutexType _mutex;	 // protect `_val` and `_callbacks`
	T			_val;

	/**
	 * @brief all callbacks in it will be called when `_val` changes
	 * @warning apply for `_mutex` in callback leads a dead lock
	 */
	std::map<uint64_t, onChangeCallback> _callbacks;
};

/**
 * @brief Manager of ConfigVar
 * @details provide methods access/create/change `ConfigVar`
 */
class Config {
	using ConfigVarMap = std::unordered_map<std::string, ConfigVarBase::ptr>;
	using RWMutexType  = RWMutex;

  public:
	/**
	 * @brief Obtain/create(`name` doesn't exist) the configuration parameter of the corresponding parameter
	 * name
	 * @param[in] name Configuration parameter name
	 * @param[in] defaultValue parameter default value
	 * @param[in] description parameter description
	 * @details get the configuration parameter named name, if there is one, directly return it. If it does
	 * not exist, create parameter configuration and assign default_value
	 * @return returns the corresponding configuration parameter. If the parameter name exists but the type
	 * does not match, nullptr is returned.
	 * @exception throw an exception `std::invalid_argument` if the parameter name contains illegal characters
	 * `[^ 0-9a-z_.]`
	 */
	template <typename T>
	static typename ConfigVar<T>::ptr
	lookup(const std::string& name, const T& defaultValue, const std::string description);

	template <typename T>
	static typename ConfigVar<T>::ptr lookup(const std::string& name);

	static void				  loadFromYaml(const YAML::Node& root);
	static void				  loadFromConfigDir(const std::string& path, bool force = false);
	static ConfigVarBase::ptr lookupBase(const std::string& name);
	static void				  visit(std::function<void(ConfigVarBase::ptr)> callback);

  private:
	static ConfigVarMap& getDatas();
	static RWMutexType&	 getMutex();
};

// ======================================================================
// Implementations
// ======================================================================

template <typename T, typename FromStr, typename ToStr>
inline std::string ConfigVar<T, FromStr, ToStr>::toString() {
	try {
		// return boost::lexical_cast<std::string>(m_val);
		RWMutexType::ReadLock lock(_mutex);
		return ToStr()(m_val);
	} catch(std::exception& e) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT())
			<< "ConfigVar::toString exception " << e.what() << " convert: " << TypeToName<T>() << " to string"
			<< " name=" << _name;
	}
	return "";
}

template <typename T, typename FromStr, typename ToStr>
inline bool ConfigVar<T, FromStr, ToStr>::fromString(const std::string& val) {
	try {
		setValue(FromStr()(val));
	} catch(std::exception& e) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT())
			<< "ConfigVar::fromString exception " << e.what() << " convert: string to " << TypeToName<T>()
			<< " name=" <<\s_.*\sval;
	}
	return false;
}

template <typename T, typename FromStr, typename ToStr>
inline const T ConfigVar<T, FromStr, ToStr>::getValue() const {
	RWMutexType::ReadLock lock(_mutex);
	return _val;
}

template <typename T, typename FromStr, typename ToStr>
inline void ConfigVar<T, FromStr, ToStr>::setValue(const T& v) {
	{
		RWMutexType::WriteLock lock(_mutex);
		if(v == _val) {
			return;
		}
		for(const auto& i : _callbacks) {
			i.second(_val, v);
		}
	}
	RWMutexType::WriteLock lock(_mutex);
	_val = v;
}

template <typename T, typename FromStr, typename ToStr>
inline uint64_t ConfigVar<T, FromStr, ToStr>::addListener(onChangeCallback callback) {
	RWMutexType::WriteLock lock(_mutex);
	_callbacks[_callbacks.size()] = callback;
	return _callbacks.size() - 1;
}

template <typename T, typename FromStr, typename ToStr>
inline void ConfigVar<T, FromStr, ToStr>::deleteListener(uint64_t key) {
	RWMutexType::WriteLock lock(_mutex);
	_callbacks.erase(key);
}

template <typename T, typename FromStr, typename ToStr>
inline void ConfigVar<T, FromStr, ToStr>::clearListener() {
	RWMutexType::WriteLock lock(_mutex);
	_callbacks.clear();
}

template <typename T>
inline typename ConfigVar<T>::ptr
Config::lookup(const std::string& name, const T& defaultValue, const std::string description) {
	RWMutexType::WriteLock lock(getMutex());
	const auto&			   it = getDatas().find(name);
	if(name.find_first_not_of("abcdefghikjlmnopqrstuvwxyz._012345678") != std::string::npos) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "Lookup name invalid " << name;
		throw std::invalid_argument(name);
	}
	if(it != getDatas().end()) {
		const auto& temp = std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
		if(nullptr != temp) {
			AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "Lookup name=" << name << " exists";
			return temp;
		} else {
			AZZATO_LOG_ERROR(AZZATO_LOG_ROOT())
				<< "Lookup name=" << name << " exists but type not " << typeToName<T>()
				<< " real_type=" << it->second->getTypeName() << " " << it->second->toString();
			return nullptr;
		}
	}

	typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, defaultValue, description));
	getDatas().at(name) = v;
	return v;
}

template <typename T>
inline typename ConfigVar<T>::ptr Config::lookup(const std::string& name) {
	RWMutexType::ReadLock lock(getMutex());
	const auto			  it = getDatas().find(name);
	if(it == getDatas().end()) {
		return nullptr;
	}
	return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
}

}  // namespace azzato
