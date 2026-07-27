#include "env.h"

#include <iostream>

int main(int argc, char** argv) {
	std::cout << "argc=" << argc << std::endl;
	azzato::EnvMgr::getInstance()->addHelp("s", "start with the terminal");
	azzato::EnvMgr::getInstance()->addHelp("d", "run as daemon");
	azzato::EnvMgr::getInstance()->addHelp("p", "print help");
	if(!azzato::EnvMgr::getInstance()->init(argc, argv)) {
		azzato::EnvMgr::getInstance()->printHelp();
		return 0;
	}

	std::cout << "exe=" << azzato::EnvMgr::getInstance()->getExe() << std::endl;
	std::cout << "cwd=" << azzato::EnvMgr::getInstance()->getCwd() << std::endl;

	std::cout << "path=" << azzato::EnvMgr::getInstance()->getEnv("PATH", "xxx") << std::endl;
	std::cout << "test=" << azzato::EnvMgr::getInstance()->getEnv("TEST", "") << std::endl;
	std::cout << "set env " << azzato::EnvMgr::getInstance()->setEnv("TEST", "yy") << std::endl;
	std::cout << "test=" << azzato::EnvMgr::getInstance()->getEnv("TEST", "") << std::endl;
	if(azzato::EnvMgr::getInstance()->has("p")) {
		azzato::EnvMgr::getInstance()->printHelp();
	}
	return 0;
}
