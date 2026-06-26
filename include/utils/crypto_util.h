#pragma once

#include <memory>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <stdint.h>
#include <string>

namespace azzato {

class CryptoUtil {
  public:
	// key 32字节
	static int32_t AES256Ecb(const void* key, const void* in, int32_t in_len, void* out, bool encode);

	// key 16字节
	static int32_t AES128Ecb(const void* key, const void* in, int32_t in_len, void* out, bool encode);

	// key 32字节
	// iv 16字节
	static int32_t
	AES256Cbc(const void* key, const void* iv, const void* in, int32_t in_len, void* out, bool encode);

	// key 16字节
	// iv 16字节
	static int32_t
	AES128Cbc(const void* key, const void* iv, const void* in, int32_t in_len, void* out, bool encode);

	static int32_t Crypto(const EVP_CIPHER* cipher,
						  bool				enc,
						  const void*		key,
						  const void*		iv,
						  const void*		in,
						  int32_t			in_len,
						  void*				out,
						  int32_t*			out_len);
};

class RSACipher {
  public:
	using ptr = std::shared_ptr<RSACipher>;

	static int32_t
	GenerateKey(const std::string& pubkey_file, const std::string& prikey_file, uint32_t length = 1024);

	static RSACipher::ptr Create(const std::string& pubkey_file, const std::string& prikey_file);

	RSACipher();
	~RSACipher();

	int32_t privateEncrypt(const void* from, int flen, void* to, int padding = RSA_NO_PADDING);
	int32_t publicEncrypt(const void* from, int flen, void* to, int padding = RSA_NO_PADDING);
	int32_t privateDecrypt(const void* from, int flen, void* to, int padding = RSA_NO_PADDING);
	int32_t publicDecrypt(const void* from, int flen, void* to, int padding = RSA_NO_PADDING);
	int32_t privateEncrypt(const void* from, int flen, std::string& to, int padding = RSA_NO_PADDING);
	int32_t publicEncrypt(const void* from, int flen, std::string& to, int padding = RSA_NO_PADDING);
	int32_t privateDecrypt(const void* from, int flen, std::string& to, int padding = RSA_NO_PADDING);
	int32_t publicDecrypt(const void* from, int flen, std::string& to, int padding = RSA_NO_PADDING);

	const std::string& getPubkeyStr() const { return\s_.*\s }
	const std::string& getPrikeyStr() const {return\s_.*\s}

	int32_t getPubRSASize();
	int32_t getPriRSASize();

  private:
	RSA*		m_pubkey;
	RSA*		m_prikey;
	std::string m_pubkeyStr;
	std::string m_prikeyStr;
};

}  // namespace azzato