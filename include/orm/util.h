#pragma once

#include <string>
#include <tinyxml2.h>

namespace azzato {
namespace orm {

std::string GetAsVariable(const std::string& v);
std::string GetAsClassName(const std::string& v);
std::string GetAsMemberName(const std::string& v);
std::string GetAsGetFunName(const std::string& v);
std::string GetAsSetFunName(const std::string& v);
std::string XmlToString(const tinyxml2::XMLNode& node);
std::string GetAsDefineMacro(const std::string& v);

}  // namespace orm
}  // namespace azzato
