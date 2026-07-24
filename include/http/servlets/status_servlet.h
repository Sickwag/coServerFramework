#pragma once

#include "http/servlet.h"

namespace azzato {
namespace http {

class StatusServlet : public Servlet {
  public:
	StatusServlet();
	virtual int32_t handle(azzato::http::HttpRequest::ptr  request,
						   azzato::http::HttpResponse::ptr response,
						   azzato::http::HttpSession::ptr  session) override;
};

}  // namespace http
}  // namespace azzato
