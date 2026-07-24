#pragma once

#include "mutex.h"
#include "rock/rock_stream.h"
#include "stream.h"
#include "utils/singleton.h"
#include <map>
#include <unordered_map>

namespace azzato {
/**
 * extern "C" {
 * Module* CreateModule() {
 *  return XX;
 * }
 * void DestoryModule(Module* ptr) {
 *  delete ptr;
 * }
 * }
 */
class Module {
  public:
	enum Type {
		MODULE = 0,
		ROCK   = 1,
	};

	typedef std::shared_ptr<Module> ptr;
	Module(const std::string& name,
		   const std::string& version,
		   const std::string& filename,
		   uint32_t			  type = MODULE);

	virtual ~Module() {}

	virtual void onBeforeArgsParse(int argc, char** argv);
	virtual void onAfterArgsParse(int argc, char** argv);

	virtual bool onLoad();
	virtual bool onUnload();

	virtual bool onConnect(azzato::Stream::ptr stream);
	virtual bool onDisconnect(azzato::Stream::ptr stream);

	virtual bool onServerReady();
	virtual bool onServerUp();

	virtual bool
	handleRequest(azzato::Message::ptr req, azzato::Message::ptr rsp, azzato::Stream::ptr stream);
	virtual bool handleNotify(azzato::Message::ptr notify, azzato::Stream::ptr stream);

	virtual std::string statusString();

	const std::string& getName() const { return _name; }

	const std::string& getVersion() const { return _version; }

	const std::string& getFilename() const { return _filename; }

	const std::string& getId() const { return _id; }

	void setFilename(const std::string& v) { _filename = v; }

	uint32_t getType() const { return _type; }

	void
	registerService(const std::string& server_type, const std::string& domain, const std::string& service);

  protected:
	std::string _name;
	std::string _version;
	std::string _filename;
	std::string _id;
	uint32_t	_type;
};

class RockModule : public Module {
  public:
	typedef std::shared_ptr<RockModule> ptr;
	RockModule(const std::string& name, const std::string& version, const std::string& filename);

	virtual bool handleRockRequest(azzato::RockRequest::ptr	 request,
								   azzato::RockResponse::ptr response,
								   azzato::RockStream::ptr	 stream)								  = 0;
	virtual bool handleRockNotify(azzato::RockNotify::ptr notify, azzato::RockStream::ptr stream) = 0;

	virtual bool
	handleRequest(azzato::Message::ptr req, azzato::Message::ptr rsp, azzato::Stream::ptr stream);
	virtual bool handleNotify(azzato::Message::ptr notify, azzato::Stream::ptr stream);
};

class ModuleManager {
  public:
	typedef RWMutex RWMutexType;

	ModuleManager();

	void add(Module::ptr m);
	void del(const std::string& name);
	void delAll();

	void init();

	Module::ptr get(const std::string& name);

	void onConnect(Stream::ptr stream);
	void onDisconnect(Stream::ptr stream);

	void listAll(std::vector<Module::ptr>& ms);
	void listByType(uint32_t type, std::vector<Module::ptr>& ms);
	void foreach(uint32_t type, std::function<void(Module::ptr)> cb);

  private:
	void initModule(const std::string& path);

  private:
	RWMutexType																   _mutex;
	std::unordered_map<std::string, Module::ptr>							   _modules;
	std::unordered_map<uint32_t, std::unordered_map<std::string, Module::ptr>> _type2Modules;
};

typedef azzato::Singleton<ModuleManager> ModuleMgr;

}  // namespace azzato
