#pragma once

#include "http/ws_session.h"
#include "mutex.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace azzato {
namespace http {

class WSServlet {
  public:
	using ptr = std::shared_ptr<WSServlet>;

	explicit WSServlet(const std::string& name)
		: _name(name) {}

	virtual ~WSServlet() = default;

	virtual int32_t handle(HttpRequest::ptr request, WSFrameMessage::ptr msg, WSSession::ptr session) = 0;

	virtual int32_t onConnect(HttpRequest::ptr header, WSSession::ptr session) { return 0; }

	virtual int32_t onClose(HttpRequest::ptr header, WSSession::ptr session) { return 0; }

	const std::string& getName() const { return _name; }

  protected:
	std::string _name;
};

class FunctionWSServlet : public WSServlet {
  public:
	using ptr		  = std::shared_ptr<FunctionWSServlet>;
	using callback	  = std::function<int32_t(HttpRequest::ptr, WSFrameMessage::ptr, WSSession::ptr)>;
	using onConnectCb = std::function<int32_t(HttpRequest::ptr, WSSession::ptr)>;
	using onCloseCb	  = std::function<int32_t(HttpRequest::ptr, WSSession::ptr)>;

	FunctionWSServlet(callback cb, onConnectCb connectCb = nullptr, onCloseCb closeCb = nullptr);

	int32_t handle(HttpRequest::ptr request, WSFrameMessage::ptr msg, WSSession::ptr session) override;

	int32_t onConnect(HttpRequest::ptr header, WSSession::ptr session) override;

	int32_t onClose(HttpRequest::ptr header, WSSession::ptr session) override;

  private:
	callback	_cb;
	onConnectCb _connectCb;
	onCloseCb	_closeCb;
};

class WSServletDispatch {
  public:
	using ptr		  = std::shared_ptr<WSServletDispatch>;
	using RWMutexType = RWMutex;

	WSServletDispatch();

	int32_t handle(HttpRequest::ptr request, WSFrameMessage::ptr msg, WSSession::ptr session);

	void addServlet(const std::string& uri, WSServlet::ptr servlet);

	void addServlet(const std::string&			   uri,
					FunctionWSServlet::callback	   cb,
					FunctionWSServlet::onConnectCb connectCb = nullptr,
					FunctionWSServlet::onCloseCb   closeCb	 = nullptr);

	void addGlobServlet(const std::string& uri, WSServlet::ptr servlet);

	void addGlobServlet(const std::string&			   uri,
						FunctionWSServlet::callback	   cb,
						FunctionWSServlet::onConnectCb connectCb = nullptr,
						FunctionWSServlet::onCloseCb   closeCb	 = nullptr);

	void delServlet(const std::string& uri);

	void delGlobServlet(const std::string& uri);

	WSServlet::ptr getWSServlet(const std::string& uri);

	WSServlet::ptr getGlobWSServlet(const std::string& uri);

	WSServlet::ptr getMatchedWSServlet(const std::string& uri);

  private:
	RWMutexType											_mutex;
	std::unordered_map<std::string, WSServlet::ptr>		_datas;
	std::vector<std::pair<std::string, WSServlet::ptr>> _globs;
	WSServlet::ptr										_defaultServlet;
};

}  // namespace http
}  // namespace azzato
