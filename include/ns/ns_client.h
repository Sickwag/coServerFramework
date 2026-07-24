#pragma once

#include "ns/ns_protocol.h"
#include "rock/rock_stream.h"

namespace azzato {
namespace ns {

class NSClient : public RockConnection {
  public:
	typedef std::shared_ptr<NSClient> ptr;
	NSClient();
	~NSClient();

	const std::set<std::string>& getQueryDomains();
	void						 setQueryDomains(const std::set<std::string>& v);

	void addQueryDomain(const std::string& domain);
	void delQueryDomain(const std::string& domain);

	bool hasQueryDomain(const std::string& domain);

	RockResult::ptr query();

	void init();
	void uninit();

	NSDomainSet::ptr getDomains() const { return _domains; }

  private:
	void onQueryDomainChange();
	bool onConnect(azzato::AsyncSocketStream::ptr stream);
	void onDisconnect(azzato::AsyncSocketStream::ptr stream);
	bool onNotify(azzato::RockNotify::ptr, azzato::RockStream::ptr);

	void onTimer();

  private:
	azzato::RWMutex		  _mutex;
	std::set<std::string> _queryDomains;
	NSDomainSet::ptr	  _domains;
	uint32_t			  _sn = 0;
	azzato::Timer::ptr	  _timer;
};

}  // namespace ns
}  // namespace azzato
