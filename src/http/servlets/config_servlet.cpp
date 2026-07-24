#include "http/servlets/config_servlet.h"
#include "utils/config.h"
#include "utils/json_util.h"
#include "utils/util.h"

namespace azzato {
namespace http {

ConfigServlet::ConfigServlet()
	: Servlet("ConfigServlet") {}

int32_t ConfigServlet::handle(azzato::http::HttpRequest::ptr  request,
							  azzato::http::HttpResponse::ptr response,
							  azzato::http::HttpSession::ptr  session) {
	std::string type = request->getParam("type");
	if(type == "json") {
		response->setHeader("Content-Type", "text/json charset=utf-8");
	} else {
		response->setHeader("Content-Type", "text/yaml charset=utf-8");
	}
	YAML::Node node;
	azzato::Config::visit([&node](ConfigVarBase::ptr base) {
		YAML::Node n;
		try {
			n = YAML::Load(base->toString());
		} catch(...) {
			return;
		}
		node[base->getName()]				   = n;
		node[base->getName() + "$description"] = base->getDescription();
	});
	if(type == "json") {
		Json::Value jvalue;
		if(yamlToJson(node, jvalue)) {
			response->setBody(JsonUtil::toString(jvalue));
			return 0;
		}
	}
	std::stringstream ss;
	ss << node;
	response->setBody(ss.str());
	return 0;
}

}  // namespace http
}  // namespace azzato
