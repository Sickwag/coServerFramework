#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace azzato {

class EMailEntity {
  public:
	typedef std::shared_ptr<EMailEntity> ptr;
	static EMailEntity::ptr				 CreateAttach(const std::string& filename);

	void		addHeader(const std::string& key, const std::string& val);
	std::string getHeader(const std::string& key, const std::string& def = "");

	const std::string& getContent() const { return _content; }

	void setContent(const std::string& v) { _content = v; }

	std::string toString() const;

  private:
	std::map<std::string, std::string> _headers;
	std::string						   _content;
};

class EMail {
  public:
	typedef std::shared_ptr<EMail> ptr;
	static EMail::ptr			   Create(const std::string&			  from_address,
										  const std::string&			  from_passwd,
										  const std::string&			  title,
										  const std::string&			  body,
										  const std::vector<std::string>& to_address,
										  const std::vector<std::string>& cc_address  = {},
										  const std::vector<std::string>& bcc_address = {});

	const std::string& getFromEMailAddress() const { return _fromEMailAddress; }

	const std::string& getFromEMailPasswd() const { return _fromEMailPasswd; }

	const std::string& getTitle() const { return _title; }

	const std::string& getBody() const { return _body; }

	void setFromEMailAddress(const std::string& v) { _fromEMailAddress = v; }

	void setFromEMailPasswd(const std::string& v) { _fromEMailPasswd = v; }

	void setTitle(const std::string& v) { _title = v; }

	void setBody(const std::string& v) { _body = v; }

	const std::vector<std::string>& getToEMailAddress() const { return _toEMailAddress; }

	const std::vector<std::string>& getCcEMailAddress() const { return _ccEMailAddress; }

	const std::vector<std::string>& getBccEMailAddress() const { return _bccEMailAddress; }

	void setToEMailAddress(const std::vector<std::string>& v) { _toEMailAddress = v; }

	void setCcEMailAddress(const std::vector<std::string>& v) { _ccEMailAddress = v; }

	void setBccEMailAddress(const std::vector<std::string>& v) { _bccEMailAddress = v; }

	void addEntity(EMailEntity::ptr entity);

	const std::vector<EMailEntity::ptr>& getEntitys() const { return _entitys; }

  private:
	std::string					  _fromEMailAddress;
	std::string					  _fromEMailPasswd;
	std::string					  _title;
	std::string					  _body;
	std::vector<std::string>	  _toEMailAddress;
	std::vector<std::string>	  _ccEMailAddress;
	std::vector<std::string>	  _bccEMailAddress;
	std::vector<EMailEntity::ptr> _entitys;
};

}  // namespace azzato
