#pragma once

#include "orm/column.h"
#include "orm/index.h"
#include <fstream>

namespace azzato {
namespace orm {

class Table {
  public:
	typedef std::shared_ptr<Table> ptr;

	const std::string& getName() const { return _name; }

	const std::string& getNamespace() const { return _namespace; }

	const std::string& getDesc() const { return _desc; }

	const std::vector<Column::ptr>& getCols() const { return _cols; }

	const std::vector<Index::ptr>& getIdxs() const { return _idxs; }

	bool init(const tinyxml2::XMLElement& node);

	void gen(const std::string& path);

	std::string getFilename() const;

  private:
	void		gen_inc(const std::string& path);
	void		gen_src(const std::string& path);
	std::string genToStringInc();
	std::string genToStringSrc(const std::string& class_name);
	std::string genToInsertSQL(const std::string& class_name);
	std::string genToUpdateSQL(const std::string& class_name);
	std::string genToDeleteSQL(const std::string& class_name);

	std::vector<Column::ptr> getPKs() const;
	Column::ptr				 getCol(const std::string& name) const;

	std::string genWhere() const;

	void gen_dao_inc(std::ofstream& ofs);
	void gen_dao_src(std::ofstream& ofs);

	enum DBType {
		TYPE_SQLITE3 = 1,
		TYPE_MYSQL	 = 2
	};

  private:
	std::string				 _name;
	std::string				 _namespace;
	std::string				 _desc;
	std::string				 _subfix	  = "_info";
	DBType					 _type		  = TYPE_SQLITE3;
	std::string				 _dbclass	  = "azzato::IDB";
	std::string				 _queryclass  = "azzato::IDB";
	std::string				 _updateclass = "azzato::IDB";
	std::vector<Column::ptr> _cols;
	std::vector<Index::ptr>	 _idxs;
};

}  // namespace orm
}  // namespace azzato
