#include "http/servlet.h"
#include "utils/macro.h"

#include <fnmatch.h>
#include <map>

namespace azzato {
namespace http {

FunctionServlet::FunctionServlet(callback cb)
	: Servlet("FunctionServlet")
	, _cb(std::move(cb)) {}

int32_t
FunctionServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) {
	return _cb(request, response, session);
}

ServletDispatch::ServletDispatch()
	: Servlet("ServletDispatch") {
	_defaultServlet.reset(new NotFoundServlet("azzato/1.0.0"));
}

int32_t
ServletDispatch::handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) {
	auto servlet = getMatchedServlet(request->getPath());
	if(servlet) {
		servlet->handle(request, response, session);
	}
	return 0;
}

void ServletDispatch::addServlet(const std::string& uri, Servlet::ptr servlet) {
	RWMutexType::WriteLock lock(_mutex);
	_datas[uri] = std::make_shared<HoldServletCreator>(servlet);
}

void ServletDispatch::addServletCreator(const std::string& uri, IServletCreator::ptr creator) {
	RWMutexType::WriteLock lock(_mutex);
	_datas[uri] = creator;
}

void ServletDispatch::addGlobServletCreator(const std::string& uri, IServletCreator::ptr creator) {
	RWMutexType::WriteLock lock(_mutex);
	for(auto it = _globs.begin(); it != _globs.end(); ++it) {
		if(it->first == uri) {
			_globs.erase(it);
			break;
		}
	}
	_globs.push_back(std::make_pair(uri, creator));
}

void ServletDispatch::addServlet(const std::string& uri, FunctionServlet::callback cb) {
	RWMutexType::WriteLock lock(_mutex);
	_datas[uri] = std::make_shared<HoldServletCreator>(std::make_shared<FunctionServlet>(std::move(cb)));
}

void ServletDispatch::addGlobServlet(const std::string& uri, Servlet::ptr servlet) {
	RWMutexType::WriteLock lock(_mutex);
	for(auto it = _globs.begin(); it != _globs.end(); ++it) {
		if(it->first == uri) {
			_globs.erase(it);
			break;
		}
	}
	_globs.push_back(std::make_pair(uri, std::make_shared<HoldServletCreator>(servlet)));
}

void ServletDispatch::addGlobServlet(const std::string& uri, FunctionServlet::callback cb) {
	addGlobServlet(uri, std::make_shared<FunctionServlet>(std::move(cb)));
}

void ServletDispatch::delServlet(const std::string& uri) {
	RWMutexType::WriteLock lock(_mutex);
	_datas.erase(uri);
}

void ServletDispatch::delGlobServlet(const std::string& uri) {
	RWMutexType::WriteLock lock(_mutex);
	for(auto it = _globs.begin(); it != _globs.end(); ++it) {
		if(it->first == uri) {
			_globs.erase(it);
			break;
		}
	}
}

Servlet::ptr ServletDispatch::getServlet(const std::string& uri) {
	RWMutexType::ReadLock lock(_mutex);
	auto				  it = _datas.find(uri);
	return it == _datas.end() ? nullptr : it->second->get();
}

Servlet::ptr ServletDispatch::getGlobServlet(const std::string& uri) {
	RWMutexType::ReadLock lock(_mutex);
	for(auto it = _globs.begin(); it != _globs.end(); ++it) {
		if(it->first == uri) {
			return it->second->get();
		}
	}
	return nullptr;
}

Servlet::ptr ServletDispatch::getMatchedServlet(const std::string& uri) {
	RWMutexType::ReadLock lock(_mutex);
	auto				  mit = _datas.find(uri);
	if(mit != _datas.end()) {
		return mit->second->get();
	}
	for(auto it = _globs.begin(); it != _globs.end(); ++it) {
		if(!fnmatch(it->first.c_str(), uri.c_str(), 0)) {
			return it->second->get();
		}
	}
	return _defaultServlet;
}

void ServletDispatch::listAllServletCreator(std::map<std::string, IServletCreator::ptr>& infos) {
	RWMutexType::ReadLock lock(_mutex);
	for(auto& item : _datas) {
		infos[item.first] = item.second;
	}
}

void ServletDispatch::listAllGlobServletCreator(std::map<std::string, IServletCreator::ptr>& infos) {
	RWMutexType::ReadLock lock(_mutex);
	for(auto& item : _globs) {
		infos[item.first] = item.second;
	}
}

NotFoundServlet::NotFoundServlet(const std::string& name)
	: Servlet("NotFoundServlet")
	, _name(name) {
	_content = "<html><head><title>404 Not Found</title></head><body><center><h1>404 Not Found</h1></center>"
			   "<hr><center>"
			   + name + "</center></body></html>";
}

int32_t
NotFoundServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) {
	response->setStatus(http::HttpStatus::NotFound);
	response->setHeader("Server", "azzato/1.0.0");
	response->setHeader("Content-Type", "text/html");
	response->setBody(_content);
	return 0;
}

}  // namespace http
}  // namespace azzato
