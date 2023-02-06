#include "base.h"

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 2)
  #include <aes/esp_aes.h>
#else
  #include <hwcrypto/aes.h>
#endif

void Crypt(const char *key, int mode, const char *input, String &output)
{
  esp_aes_context cryptContext = {};
  esp_aes_init(&cryptContext);
  esp_aes_setkey(&cryptContext, (unsigned char*)key, 128);

  unsigned char block[16] = {};
  uint32_t const length = strlen(input);
  
  for (uint32_t i = 0 ; i < length ; )
  {
    output.concat("                ");
    
    uint32_t const remain = length - i;
    uint32_t const dataSize = std::min<uint32_t>(16, remain);
    
    memcpy(block, input + i, dataSize);
    memset(block + dataSize, 0, 16 - dataSize);
    
    int ret = esp_aes_crypt_ecb(&cryptContext, mode, block, (unsigned char*)output.begin() + i);
    if (ret != 0)
    {
      LOG("ERROR: esp_aes_crypt_ecb failed:%d\n\r", ret);
    }
    
    i += dataSize;
  }
}

void Encrypt(const char *key, const char *input, String &output)
{
  Crypt(key, ESP_AES_ENCRYPT, input, output);
}

void Decrypt(const char *key, const char *input, String &output)
{
  Crypt(key, ESP_AES_DECRYPT, input, output);
}
