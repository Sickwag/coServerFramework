#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace azzato {

/*
   foo://user@sylar.com:8042/over/there?name=ferret#nose
     \_/   \______________/\_________/ \_________/ \__/
      |           |            |            |        |
   scheme     authority       path        query   fragment
*/

class Uri {
  public:
	using ptr = std::shared_ptr<Uri>;

	static ptr create(const std::string& uri);

	Uri();

	const std::string& getScheme() const { return _scheme; }

	const std::string& getUserinfo() const { return _userinfo; }

	const std::string& getHost() const { return _host; }

	const std::string& getPath() const { return _path; }

	const std::string& getQuery() const { return _query; }

	const std::string& getFragment() const { return _fragment; }

	int32_t getPort() const;

	void setScheme(const std::string& v) { _scheme = v; }

	void setUserinfo(const std::string& v) { _userinfo = v; }

	void setHost(const std::string& v) { _host = v; }

	void setPath(const std::string& v) { _path = v; }

	void setQuery(const std::string& v) { _query = v; }

	void setFragment(const std::string& v) { _fragment = v; }

	void setPort(int32_t v) { _port = v; }

	std::ostream& dump(std::ostream& os) const;

	std::string toString() const;

  private:
	std::string _scheme;
	std::string _userinfo;
	std::string _host;
	std::string _path;
	std::string _query;
	std::string _fragment;
	int32_t		_port = 0;
};

}  // namespace azzato
