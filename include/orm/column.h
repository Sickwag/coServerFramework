#pragma once

#include "tinyxml2.h"
#include <memory>
#include <string>

namespace azzato {
namespace orm {

class Table;

class Column {
	friend class Table;

  public:
	typedef std::shared_ptr<Column> ptr;

	enum Type {
		TYPE_NULL = 0,
		TYPE_INT8,
		TYPE_UINT8,
		TYPE_INT16,
		TYPE_UINT16,
		TYPE_INT32,
		TYPE_UINT32,
		TYPE_FLOAT,
		TYPE_DOUBLE,
		TYPE_INT64,
		TYPE_UINT64,
		TYPE_STRING,
		TYPE_TEXT,
		TYPE_BLOB,
		TYPE_TIMESTAMP
	};

	const std::string& getName() const { return _name; }

	const std::string& getType() const { return _type; }

	const std::string& getDesc() const { return _desc; }

	const std::string& getDefault() const { return _default; }

	std::string getDefaultValueString();
	std::string getSQLite3Default();

	bool isAutoIncrement() const { return _autoIncrement; }

	Type getDType() const { return _dtype; }

	bool init(const tinyxml2::XMLElement& node);

	std::string getMemberDefine() const;
	std::string getGetFunDefine() const;
	std::string getSetFunDefine() const;
	std::string getSetFunImpl(const std::string& class_name, int idx) const;

	int getIndex() const { return _index; }

	static Type		   ParseType(const std::string& v);
	static std::string TypeToString(Type type);

	std::string getDTypeString() { return TypeToString(_dtype); }

	std::string getSQLite3TypeString();
	std::string getMySQLTypeString();

	std::string getBindString();
	std::string getGetString();

	const std::string& getUpdate() const { return _update; }

  private:
	std::string _name;
	std::string _type;
	std::string _default;
	std::string _update;
	std::string _desc;
	int			_index;

	bool _autoIncrement;
	Type _dtype;
	int	 _length;
};

}  // namespace orm
}  // namespace azzato
