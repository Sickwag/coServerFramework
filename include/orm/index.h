#pragma once

#include "tinyxml2.h"
#include <memory>
#include <string>
#include <vector>

namespace azzato {
namespace orm {

class Index {
  public:
	enum Type {
		TYPE_NULL = 0,
		TYPE_PK,
		TYPE_UNIQ,
		TYPE_INDEX
	};

	typedef std::shared_ptr<Index> ptr;

	const std::string& getName() const { return _name; }

	const std::string& getType() const { return _type; }

	const std::string& getDesc() const { return _desc; }

	const std::vector<std::string>& getCols() const { return _cols; }

	Type getDType() const { return _dtype; }

	bool init(const tinyxml2::XMLElement& node);

	bool isPK() const { return _type == "pk"; }

	static Type		   ParseType(const std::string& v);
	static std::string TypeToString(Type v);

  private:
	std::string				 _name;
	std::string				 _type;
	std::string				 _desc;
	std::vector<std::string> _cols;

	Type _dtype;
};

}  // namespace orm
}  // namespace azzato
