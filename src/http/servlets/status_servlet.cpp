#include "http/servlets/status_servlet.h"
#include "application.h"
#include "daemon.h"
#include "module.h"
#include "utils/util.h"
#include "worker.h"

namespace azzato {
namespace http {

StatusServlet::StatusServlet()
	: Servlet("StatusServlet") {}

std::string format_used_time(int64_t ts) {
	std::stringstream ss;
	bool			  v = false;
	if(ts >= 3600 * 24) {
		ss << (ts / 3600 / 24) << "d ";
		ts = ts % (3600 * 24);
		v  = true;
	}
	if(ts >= 3600) {
		ss << (ts / 3600) << "h ";
		ts = ts % 3600;
		v  = true;
	} else if(v) {
		ss << "0h ";
	}

	if(ts >= 60) {
		ss << (ts / 60) << "m ";
		ts = ts % 60;
	} else if(v) {
		ss << "0m ";
	}
	ss << ts << "s";
	return ss.str();
}

int32_t StatusServlet::handle(azzato::http::HttpRequest::ptr  request,
							  azzato::http::HttpResponse::ptr response,
							  azzato::http::HttpSession::ptr  session) {
	response->setHeader("Content-Type", "text/text; charset=utf-8");
#define XX(key) ss << std::setw(30) << std::right << key ": "
	std::stringstream ss;
	ss << "===================================================" << std::endl;
	XX("server_version") << "sylar/1.0.0" << std::endl;

	std::vector<Module::ptr> ms;
	ModuleMgr::getInstance()->listAll(ms);

	XX("modules");
	for(size_t i = 0; i < ms.size(); ++i) {
		if(i) {
			ss << ";";
		}
		ss << ms[i]->getId();
	}
	ss << std::endl;
	XX("host") << getHostName() << std::endl;
	XX("ipv4") << getIPv4() << std::endl;
	XX("daemon_id") << ProcessInfoMgr::getInstance()->parent_id << std::endl;
	XX("main_id") << ProcessInfoMgr::getInstance()->main_id << std::endl;
	XX("daemon_start") << time2Str(ProcessInfoMgr::getInstance()->parent_start_time) << std::endl;
	XX("main_start") << time2Str(ProcessInfoMgr::getInstance()->main_start_time) << std::endl;
	XX("restart_count") << ProcessInfoMgr::getInstance()->restart_count << std::endl;
	XX("daemon_running_time") << format_used_time(time(0) - ProcessInfoMgr::getInstance()->parent_start_time)
							  << std::endl;
	XX("main_running_time") << format_used_time(time(0) - ProcessInfoMgr::getInstance()->main_start_time)
							<< std::endl;
	ss << "===================================================" << std::endl;
	XX("fibers") << azzato::Fiber::getTotalFibers() << std::endl;
	ss << "===================================================" << std::endl;
	ss << "<Logger>" << std::endl;
	ss << azzato::LoggerMgr::getInstance()->toYamlString() << std::endl;
	ss << "===================================================" << std::endl;
	ss << "<Woker>" << std::endl;
	azzato::WorkerMgr::getInstance()->dump(ss) << std::endl;

	std::map<std::string, std::vector<TcpServer::ptr>> servers;
	azzato::Application::getInstance()->listAllServer(servers);
	ss << "===================================================" << std::endl;
	for(auto it = servers.begin(); it != servers.end(); ++it) {
		if(it != servers.begin()) {
			ss << "***************************************************" << std::endl;
		}
		ss << "<Server." << it->first << ">" << std::endl;
		azzato::http::HttpServer::ptr hs;
		for(auto iit = it->second.begin(); iit != it->second.end(); ++iit) {
			if(iit != it->second.begin()) {
				ss << "---------------------------------------------------" << std::endl;
			}
			if(!hs) {
				hs = std::dynamic_pointer_cast<azzato::http::HttpServer>(*iit);
			}
			ss << (*iit)->toString() << std::endl;
		}
		if(hs) {
			auto sd = hs->getServletDispatch();
			if(sd) {
				std::map<std::string, IServletCreator::ptr> infos;
				sd->listAllServletCreator(infos);
				if(!infos.empty()) {
					ss << "[Servlets]" << std::endl;
#define XX2(key) ss << std::setw(30) << std::right << key << ": "
					for(auto& i : infos) {
						XX2(i.first) << i.second->getName() << std::endl;
					}
					infos.clear();
				}
				sd->listAllGlobServletCreator(infos);
				if(!infos.empty()) {
					ss << "[Servlets.Globs]" << std::endl;
					for(auto& i : infos) {
						XX2(i.first) << i.second->getName() << std::endl;
					}
					infos.clear();
				}
			}
		}
	}
	ss << "===================================================" << std::endl;
	for(size_t i = 0; i < ms.size(); ++i) {
		if(i) {
			ss << "***************************************************" << std::endl;
		}
		ss << ms[i]->statusString() << std::endl;
	}

	response->setBody(ss.str());
	return 0;
}

}  // namespace http
}  // namespace azzato
