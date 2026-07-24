#include "ns/ns_client.h"
#include "log.h"
#include "utils/util.h"

namespace azzato {
namespace ns {

static azzato::Logger::ptr g_logger = AZZATO_LOG_NAME("system");

NSClient::NSClient() { _domains.reset(new azzato::ns::NSDomainSet); }

NSClient::~NSClient() { AZZATO_LOG_DEBUG(g_logger) << "NSClient::~NSClient"; }

const std::set<std::string>& NSClient::getQueryDomains() {
	azzato::RWMutex::ReadLock lock(_mutex);
	return _queryDomains;
}

void NSClient::setQueryDomains(const std::set<std::string>& v) {
	azzato::RWMutex::WriteLock lock(_mutex);
	_queryDomains = v;
	lock.unlock();
	onQueryDomainChange();
}

void NSClient::addQueryDomain(const std::string& domain) {
	azzato::RWMutex::WriteLock lock(_mutex);
	_queryDomains.insert(domain);
	lock.unlock();
	onQueryDomainChange();
}

bool NSClient::hasQueryDomain(const std::string& domain) {
	azzato::RWMutex::ReadLock lock(_mutex);
	return _queryDomains.count(domain) > 0;
}

void NSClient::delQueryDomain(const std::string& domain) {
	azzato::RWMutex::WriteLock lock(_mutex);
	_queryDomains.erase(domain);
	lock.unlock();
	onQueryDomainChange();
}

RockResult::ptr NSClient::query() {
	azzato::RockRequest::ptr req = std::make_shared<azzato::RockRequest>();
	req->setSn(azzato::Atomic::addFetch(_sn, 1));
	req->setCmd((int)NSCommand::QUERY);
	auto data = std::make_shared<azzato::ns::QueryRequest>();

	azzato::RWMutex::ReadLock lock(_mutex);
	for(auto& i : _queryDomains) {
		data->add_domains(i);
	}
	if(_queryDomains.empty()) {
		return std::make_shared<RockResult>(0, 0, nullptr, nullptr);
	}
	lock.unlock();

	req->setAsPB(*data);
	auto rt = request(req, 1000);
	do {
		if(!rt->response) {
			AZZATO_LOG_ERROR(g_logger) << "query error result=" << rt->result;
			break;
		}
		auto rsp = rt->response->getAsPB<azzato::ns::QueryResponse>();
		if(!rsp) {
			AZZATO_LOG_ERROR(g_logger) << "invalid data not QueryResponse";
			break;
		}

		NSDomainSet::ptr domains(new NSDomainSet);
		for(auto& i : rsp->infos()) {
			if(!hasQueryDomain(i.domain())) {
				continue;
			}
			auto	 domain = domains->get(i.domain(), true);
			uint32_t cmd	= i.cmd();

			for(auto& n : i.nodes()) {
				NSNode::ptr node(new NSNode(n.ip(), n.port(), n.weight()));
				if(!(node->getId() >> 32)) {
					AZZATO_LOG_ERROR(g_logger) << "invalid node: " << node->toString();
					continue;
				}
				domain->add(cmd, node);
			}
		}
		_domains->swap(*domains);
	} while(false);
	return rt;
}

void NSClient::onQueryDomainChange() {
	if(isConnected()) {
		query();
	}
}

void NSClient::init() {
	auto self = std::dynamic_pointer_cast<NSClient>(shared_from_this());
	setConnectCb(std::bind(&NSClient::onConnect, self, std::placeholders::_1));
	setDisconnectCb(std::bind(&NSClient::onDisconnect, self, std::placeholders::_1));
	setNotifyHandler(std::bind(&NSClient::onNotify, self, std::placeholders::_1, std::placeholders::_2));
}

void NSClient::uninit() {
	setConnectCb(nullptr);
	setDisconnectCb(nullptr);
	setNotifyHandler(nullptr);

	if(_timer) {
		_timer->cancel();
	}
}

bool NSClient::onConnect(azzato::AsyncSocketStream::ptr stream) {
	if(_timer) {
		_timer->cancel();
	}
	auto self = std::dynamic_pointer_cast<NSClient>(shared_from_this());
	_timer	  = _iomanager->addTimer(30 * 1000, std::bind(&NSClient::onTimer, self), true);
	_iomanager->schedule(std::bind(&NSClient::query, self));
	return true;
}

void NSClient::onTimer() {
	azzato::RockRequest::ptr req = std::make_shared<azzato::RockRequest>();
	req->setSn(azzato::Atomic::addFetch(_sn, 1));
	req->setCmd((uint32_t)NSCommand::TICK);
	auto rt = request(req, 1000);
	if(!rt->response) {
		AZZATO_LOG_ERROR(g_logger) << "tick error result=" << rt->result;
	}
	sleep(1000);
	query();
}

void NSClient::onDisconnect(azzato::AsyncSocketStream::ptr stream) {}

bool NSClient::onNotify(azzato::RockNotify::ptr nty, azzato::RockStream::ptr stream) {
	do {
		if(nty->getNotify() == (uint32_t)NSNotify::NODE_CHANGE) {
			auto nm = nty->getAsPB<azzato::ns::NotifyMessage>();
			if(!nm) {
				AZZATO_LOG_ERROR(g_logger) << "invalid node_change data";
				break;
			}

			for(auto& i : nm->dels()) {
				if(!hasQueryDomain(i.domain())) {
					continue;
				}
				auto domain = _domains->get(i.domain());
				if(!domain) {
					continue;
				}
				int cmd = i.cmd();
				for(auto& n : i.nodes()) {
					NSNode::ptr node(new NSNode(n.ip(), n.port(), n.weight()));
					domain->del(cmd, node->getId());
				}
			}

			for(auto& i : nm->updates()) {
				if(!hasQueryDomain(i.domain())) {
					continue;
				}
				auto domain = _domains->get(i.domain(), true);
				int	 cmd	= i.cmd();
				for(auto& n : i.nodes()) {
					NSNode::ptr node(new NSNode(n.ip(), n.port(), n.weight()));
					if(node->getId() >> 32) {
						domain->add(cmd, node);
					} else {
						AZZATO_LOG_ERROR(g_logger) << "invalid node: " << node->toString();
					}
				}
			}
		}
	} while(false);
	return true;
}

}  // namespace ns
}  // namespace azzato
