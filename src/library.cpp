#include "library.h"

#include "env.h"
#include "log.h"
#include "utils/config.h"
#include <dlfcn.h>

namespace azzato {

static azzato::Logger::ptr g_logger = AZZATO_LOG_NAME("system");

typedef Module* (*create_module)();
typedef void (*destory_module)(Module*);

class ModuleCloser {
  public:
	ModuleCloser(void* handle, destory_module d)
		: _handle(handle)
		, _destory(d) {}

	void operator()(Module* module) {
		std::string name	= module->getName();
		std::string version = module->getVersion();
		std::string path	= module->getFilename();
		_destory(module);
		int rt = dlclose(_handle);
		if(rt) {
			AZZATO_LOG_ERROR(g_logger)
				<< "dlclose handle fail handle=" << _handle << " name=" << name << " version=" << version
				<< " path=" << path << " error=" << dlerror();
		} else {
			AZZATO_LOG_INFO(g_logger) << "destory module=" << name << " version=" << version
									  << " path=" << path << " handle=" << _handle << " success";
		}
	}

  private:
	void*		   _handle;
	destory_module _destory;
};

Module::ptr Library::GetModule(const std::string& path) {
	void* handle = dlopen(path.c_str(), RTLD_NOW);
	if(!handle) {
		AZZATO_LOG_ERROR(g_logger) << "cannot load library path=" << path << " error=" << dlerror();
		return nullptr;
	}

	create_module create = (create_module)dlsym(handle, "CreateModule");
	if(!create) {
		AZZATO_LOG_ERROR(g_logger) << "cannot load symbol CreateModule in " << path << " error=" << dlerror();
		dlclose(handle);
		return nullptr;
	}

	destory_module destory = (destory_module)dlsym(handle, "DestoryModule");
	if(!destory) {
		AZZATO_LOG_ERROR(g_logger) << "cannot load symbol DestoryModule in " << path
								   << " error=" << dlerror();
		dlclose(handle);
		return nullptr;
	}

	Module::ptr module(create(), ModuleCloser(handle, destory));
	module->setFilename(path);
	AZZATO_LOG_INFO(g_logger) << "load module name=" << module->getName()
							  << " version=" << module->getVersion() << " path=" << module->getFilename()
							  << " success";
	Config::loadFromConfigDir(azzato::EnvMgr::getInstance()->getConfigPath(), true);
	return module;
}

}  // namespace azzato
