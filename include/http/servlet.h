#pragma once

#include "http/http.h"
#include "http/http_session.h"
#include "mutex.h"
#include "utils/util.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace azzato {
namespace http {

class Servlet {
  public:
	using ptr = std::shared_ptr<Servlet>;

	explicit Servlet(const std::string& name)
		: _name(name) {}

	virtual ~Servlet() = default;

	virtual int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) = 0;

	const std::string& getName() const { return _name; }

  protected:
	std::string _name;
};

class FunctionServlet : public Servlet {
  public:
	using ptr	   = std::shared_ptr<FunctionServlet>;
	using callback = std::function<int32_t(HttpRequest::ptr, HttpResponse::ptr, HttpSession::ptr)>;

	explicit FunctionServlet(callback cb);

	int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;

  private:
	callback _cb;
};

class IServletCreator {
  public:
	using ptr = std::shared_ptr<IServletCreator>;

	virtual ~IServletCreator() = default;

	virtual Servlet::ptr get() const = 0;

	virtual std::string getName() const = 0;
};

class HoldServletCreator : public IServletCreator {
  public:
	using ptr = std::shared_ptr<HoldServletCreator>;

	explicit HoldServletCreator(Servlet::ptr servlet)
		: _servlet(std::move(servlet)) {}

	Servlet::ptr get() const override { return _servlet; }

	std::string getName() const override { return _servlet->getName(); }

  private:
	Servlet::ptr _servlet;
};

template <typename T>
class ServletCreator : public IServletCreator {
  public:
	using ptr = std::shared_ptr<ServletCreator>;

	Servlet::ptr get() const override { return Servlet::ptr(new T); }

	std::string getName() const override { return typeToName<T>(); }
};

class ServletDispatch : public Servlet {
  public:
	using ptr		   = std::shared_ptr<ServletDispatch>;
	using RWMutexType = RWMutex;

	ServletDispatch();

	int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;

	void addServlet(const std::string& uri, Servlet::ptr servlet);

	void addServlet(const std::string& uri, FunctionServlet::callback cb);

	void addGlobServlet(const std::string& uri, Servlet::ptr servlet);

	void addGlobServlet(const std::string& uri, FunctionServlet::callback cb);

	void addServletCreator(const std::string& uri, IServletCreator::ptr creator);

	void addGlobServletCreator(const std::string& uri, IServletCreator::ptr creator);

	template <typename T>
	void addServletCreator(const std::string& uri) {
		addServletCreator(uri, std::make_shared<ServletCreator<T>>());
	}

	template <typename T>
	void addGlobServletCreator(const std::string& uri) {
		addGlobServletCreator(uri, std::make_shared<ServletCreator<T>>());
	}

	void delServlet(const std::string& uri);

	void delGlobServlet(const std::string& uri);

	Servlet::ptr getDefault() const { return _defaultServlet; }

	void setDefault(Servlet::ptr value) { _defaultServlet = std::move(value); }

	Servlet::ptr getServlet(const std::string& uri);

	Servlet::ptr getGlobServlet(const std::string& uri);

	Servlet::ptr getMatchedServlet(const std::string& uri);

	void listAllServletCreator(std::map<std::string, IServletCreator::ptr>& infos);

	void listAllGlobServletCreator(std::map<std::string, IServletCreator::ptr>& infos);

  private:
	RWMutexType												 _mutex;
	std::unordered_map<std::string, IServletCreator::ptr>	 _datas;
	std::vector<std::pair<std::string, IServletCreator::ptr>> _globs;
	Servlet::ptr											 _defaultServlet;
};

class NotFoundServlet : public Servlet {
  public:
	using ptr = std::shared_ptr<NotFoundServlet>;

	explicit NotFoundServlet(const std::string& name);

	int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;

  private:
	std::string _name;
	std::string _content;
};

}  // namespace http
}  // namespace azzato
