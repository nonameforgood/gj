#include "wificonfig.h"
#include "file.h"
#include "base.h"
#include "crypt.h"

bool ReadWifiConfig(const char *filepath, WifiConfig &config)
{
  GJFile file(filepath, GJFile::Mode::Read);
  
  int const fileSize = file.Size();
  if (!fileSize)
  {
    LOG("ERROR: '%s' is missing or empty\n\r", filepath);
    return false;
  }
 
  String configEncrypted;
  configEncrypted.reserve(fileSize+1);
  
  int const readSize = file.Read(configEncrypted.begin(), fileSize);
  if (fileSize != readSize)
  {
    LOG("ERROR: '%s' file size != read size\n\r", filepath);
    return false;
  }
  
  configEncrypted[fileSize] = 0;
  
  String content;

  //This is by no means very secure
  //It just makes it a bit more difficult to retrieve the password
  Decrypt("gjserver_gjserver", configEncrypted.c_str(), content);
  
  char *configIt = content.begin();

  int ssidStoredLength = 0;
  sscanf(configIt, "%d", &ssidStoredLength);
  
  char *ssidSep = strstr(configIt, ";");
  if (!ssidSep)
  {
    LOG("ERROR: '%s' is missing ssid semicolon separator\n\r", filepath);
    return false;
  }
  
  char *passSep = strstr(ssidSep + 1, ";");
  if (!passSep)
  {
    LOG("ERROR: '%s' is missing pass semicolon separator\n\r", filepath);
    return false;
  }
  
  int const ssidLength = (passSep - ssidSep) - 1;
  int const passLength = (configIt + fileSize - passSep) - 1;
  
  if (!ssidLength)
  {
    LOG("ERROR: '%s' contains empty ssid\n\r", filepath);
    return false;
  }
  
  if (ssidLength != ssidStoredLength)
  {
    LOG("ERROR: '%s' stored ssid length != actual ssid length\n\r", filepath);
    return false;
  }
  
  *ssidSep = 0;
  *passSep = 0;
  
  config.m_ssid.concat(ssidSep + 1);
  config.m_pass.concat(passSep + 1);

  return true;
}

bool WriteWifiConfig(const char *filepath, WifiConfig const &config)
{
  uint32_t const ssidLength = strlen(config.m_ssid.c_str());
  
  if (!ssidLength)
  {
    LOG("ERROR:SSID is empty\n\r");
    return false;
  }
 
  String buffer;
  buffer.reserve(ssidLength + strlen(config.m_pass.c_str()) + 32);
  sprintf(buffer.begin(), "%d;%s;%s", ssidLength, config.m_ssid.c_str(), config.m_pass.c_str());
  
  String encrypted;
  Encrypt("gjserver_gjserver", buffer.c_str(), encrypted);
  
  if (encrypted.isEmpty())
  {
    LOG("ERROR:Encrypted Wifi config is empty\n\r");
    return false;
  }

  GJFile file(filepath, GJFile::Mode::Write);
  uint32_t const written = file.Write(encrypted.c_str(), encrypted.length());
  
  if (written != encrypted.length())
  {
    LOG("ERROR:Encrypted wifi config not written\n\r");
    return false;
  }
  
  LOG("Wifi config written\n\r");

  return true;
}
