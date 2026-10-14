#include <std_include.hpp>
#include <bcrypt.h>
#include <wincrypt.h>
#include <utils/cryptography.hpp>
namespace utils::cryptography {
 namespace sha256 {
 std::string compute(const std::string& data,bool hex) {
  BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};DWORD size{},written{};
  if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA provider");
  BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&written,0);
  std::vector<unsigned char> object(size);std::array<unsigned char,32> digest{};
  if(BCryptCreateHash(algorithm,&hash,object.data(),size,nullptr,0,0)<0 || BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())),static_cast<ULONG>(data.size()),0)<0 || BCryptFinishHash(hash,digest.data(),32,0)<0)throw std::runtime_error("SHA hash");
  BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);
  if(!hex)return {reinterpret_cast<const char*>(digest.data()),digest.size()};
  constexpr char digits[]="0123456789ABCDEF";std::string result;
  for(auto byte:digest){result+=digits[byte>>4];result+=digits[byte&15];}return result;
 }
 }
 namespace base64 {
 std::string encode(const std::string& data) {
  DWORD count{};const auto flags=CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF;
  CryptBinaryToStringA(reinterpret_cast<const BYTE*>(data.data()),static_cast<DWORD>(data.size()),flags,nullptr,&count);
  std::string result(count,0);if(!CryptBinaryToStringA(reinterpret_cast<const BYTE*>(data.data()),static_cast<DWORD>(data.size()),flags,result.data(),&count))throw std::runtime_error("base64 encode");
  if(!result.empty() && result.back()==0)result.pop_back();return result;
 }
 std::string decode(const std::string& data) {
  DWORD count{};CryptStringToBinaryA(data.data(),static_cast<DWORD>(data.size()),CRYPT_STRING_BASE64,nullptr,&count,nullptr,nullptr);
  std::string result(count,0);if(!CryptStringToBinaryA(data.data(),static_cast<DWORD>(data.size()),CRYPT_STRING_BASE64,reinterpret_cast<BYTE*>(result.data()),&count,nullptr,nullptr))throw std::runtime_error("base64 decode");return result;
 }
 }
}
