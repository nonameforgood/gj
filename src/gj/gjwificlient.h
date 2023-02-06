#pragma once
#include <Client.h>
#include "vector.h"

class GJWifiClient : public Client
{
public:
  GJWifiClient();
  GJWifiClient(int conn);
  virtual ~GJWifiClient();

  virtual int connect(IPAddress ip, uint16_t port);
  virtual int connect(const char *host, uint16_t port);
  virtual size_t write(uint8_t);
  virtual size_t write(const uint8_t *buf, size_t size);
  virtual int available();
  virtual int read();
  virtual int read(uint8_t *buf, size_t size);
  virtual int peek();
  virtual void flush();
  virtual void stop();
  virtual uint8_t connected();
  virtual operator bool();

protected:
  int m_conn = -1;
  Vector<uint8_t> m_rxBuffer;
};