#include "http/ws_servlet.h"
#include "utils/macro.h"

#include <fnmatch.h>

namespace azzato {
namespace http {

FunctionWSServlet::FunctionWSServlet(callback cb, const std::string& name)
	: WSServlet(name)
	, _cb(std::move(cb)) {}

int32_t FunctionWSServlet::handle(HttpRequest::ptr request, WSFrameMessage::ptr msg, WSSession::ptr session) {
	return _cb(request, std::move(msg), std::move(session));
}

WSServletDispatch::WSServletDispatch()
	: _defaultServlet(std::make_shared<FunctionWSServlet>(
		  [](HttpRequest::ptr, WSFrameMessage::ptr, WSSession::ptr) -> int32_t { return 0; },
		  "default")) {}

int32_t WSServletDispatch::handle(HttpRequest::ptr request, WSFrameMessage::ptr msg, WSSession::ptr session) {
	auto servlet = getMatchedWSServlet(request->getPath());
	if(servlet) {
		servlet->handle(request, std::move(msg), std::move(session));
	}
	return 0;
}

void WSServletDispatch::addServlet(const std::string& uri, WSServlet::ptr servlet) {
	RWMutexType::WriteLock lock(_mutex);
	_datas[uri] = servlet;
}

void WSServletDispatch::addServlet(const std::string& uri, FunctionWSServlet::callback cb) {
	RWMutexType::WriteLock lock(_mutex);
	_datas[uri] = std::make_shared<FunctionWSServlet>(std::move(cb));
}

void WSServletDispatch::addGlobServlet(const std::string& uri, WSServlet::ptr servlet) {
	RWMutexType::WriteLock lock(_mutex);
	for(auto it = _globs.begin(); it != _globs.end(); ++it) {
		if(it->first == uri) {
			_globs.erase(it);
			break;
		}
	}
	_globs.push_back(std::make_pair(uri, servlet));
}

void WSServletDispatch::addGlobServlet(const std::string& uri, FunctionWSServlet::callback cb) {
	addGlobServlet(uri, std::make_shared<FunctionWSServlet>(std::move(cb)));
}

void WSServletDispatch::delServlet(const std::string& uri) {
	RWMutexType::WriteLock lock(_mutex);
	_datas.erase(uri);
}

void WSServletDispatch::delGlobServlet(const std::string& uri) {
	RWMutexType::WriteLock lock(_mutex);
	for(auto it = _globs.begin(); it != _globs.end(); ++it) {
		if(it->first == uri) {
			_globs.erase(it);
			break;
		}
	}
}

WSServlet::ptr WSServletDispatch::getWSServlet(const std::string& uri) {
	RWMutexType::ReadLock lock(_mutex);
	auto				  it = _datas.find(uri);
	return it == _datas.end() ? nullptr : it->second;
}

WSServlet::ptr WSServletDispatch::getGlobWSServlet(const std::string& uri) {
	RWMutexType::ReadLock lock(_mutex);
	for(auto it = _globs.begin(); it != _globs.end(); ++it) {
		if(it->first == uri) {
			return it->second;
		}
	}
	return nullptr;
}

WSServlet::ptr WSServletDispatch::getMatchedWSServlet(const std::string& uri) {
	RWMutexType::ReadLock lock(_mutex);
	auto				  mit = _datas.find(uri);
	if(mit != _datas.end()) {
		return mit->second;
	}
	for(auto it = _globs.begin(); it != _globs.end(); ++it) {
		if(!fnmatch(it->first.c_str(), uri.c_str(), 0)) {
			return it->second;
		}
	}
	return _defaultServlet;
}

}  // namespace http
}  // namespace azzato
