#pragma once

#include "module.h"
#include <memory>

namespace azzato {

class Library {
  public:
	static Module::ptr GetModule(const std::string& path);
};

}  // namespace azzato
