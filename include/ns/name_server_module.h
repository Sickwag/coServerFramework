#pragma once

#include "module.h"
#include "ns/ns_protocol.h"

namespace azzato {
namespace ns {

class NameServerModule;

class NSClientInfo {
	friend class NameServerModule;

  public:
	typedef std::shared_ptr<NSClientInfo> ptr;

  private:
	NSNode::ptr								  _node;
	std::map<std::string, std::set<uint32_t>> _domain2cmds;
};

class NameServerModule : public RockModule {
  public:
	typedef std::shared_ptr<NameServerModule> ptr;
	NameServerModule();

	virtual bool handleRockRequest(azzato::RockRequest::ptr	 request,
								   azzato::RockResponse::ptr response,
								   azzato::RockStream::ptr	 stream) override;
	virtual bool handleRockNotify(azzato::RockNotify::ptr notify, azzato::RockStream::ptr stream) override;
	virtual bool onConnect(azzato::Stream::ptr stream) override;
	virtual bool onDisconnect(azzato::Stream::ptr stream) override;
	virtual std::string statusString() override;

  private:
	bool handleRegister(azzato::RockRequest::ptr  request,
						azzato::RockResponse::ptr response,
						azzato::RockStream::ptr	  stream);
	bool handleQuery(azzato::RockRequest::ptr  request,
					 azzato::RockResponse::ptr response,
					 azzato::RockStream::ptr   stream);
	bool handleTick(azzato::RockRequest::ptr  request,
					azzato::RockResponse::ptr response,
					azzato::RockStream::ptr	  stream);

  private:
	NSClientInfo::ptr get(azzato::RockStream::ptr rs);
	void			  set(azzato::RockStream::ptr rs, NSClientInfo::ptr info);

	void setQueryDomain(azzato::RockStream::ptr rs, const std::set<std::string>& ds);

	void doNotify(std::set<std::string>& domains, std::shared_ptr<NotifyMessage> nty);

	std::set<azzato::RockStream::ptr> getStreams(const std::string& domain);

  private:
	NSDomainSet::ptr _domains;

	azzato::RWMutex										 _mutex;
	std::map<azzato::RockStream::ptr, NSClientInfo::ptr> _sessions;

	/// sessoin 关注的域名
	std::map<azzato::RockStream::ptr, std::set<std::string>> _queryDomains;
	/// 域名对应关注的session
	std::map<std::string, std::set<azzato::RockStream::ptr>> _domainToSessions;
};

}  // namespace ns
}  // namespace azzato
