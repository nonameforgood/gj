#include "base.h"
#include "gjwificlient.h"
#include "lwip/api.h"
#include "lwip/tcp.h"
#include "lwip/err.h"
#include "lwip/dns.h"

#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "freertos/projdefs.h"

#define WC_LOC(f, ...) SetDebugLoc(f)
#define WC_ON_ERROR(r, s) if (r < 0) {s;} 

bool LostConnection(int err)
{
  if (err == ECONNABORTED || err == ECONNRESET || err == ECONNREFUSED || err == pdFREERTOS_ERRNO_ENOTCONN)
  {
    GJ_ERROR("GJWifiClient::LostConnection err:%d\n\r", err);
    return true;
  }

  return false;
}

GJWifiClient::GJWifiClient() = default;

GJWifiClient::GJWifiClient(int conn)
: m_conn(conn)
{
  int flags = fcntl(conn, F_GETFL);
  flags |= O_NONBLOCK;
  fcntl(conn, F_SETFL, flags);
}

GJWifiClient::~GJWifiClient()
{
  LOG("~GJWifiClient\n\r");
  stop();
}

int GJWifiClient::connect(IPAddress ip, uint16_t port)
{
  WC_LOC("GJWifiClient::connect ip\n\r");
  return 0;
}

int GJWifiClient::connect(const char *host, uint16_t port)
{
  return 0;
}

size_t GJWifiClient::write(uint8_t b)
{
  return write(&b, 1);
}

size_t GJWifiClient::write(const uint8_t *buf, size_t size)
{
  if (m_conn <= 0)
    return 0;

  size_t bytes_written = 0;
  WC_LOC("GJWifiClient::write netconn_write_partly");
  bytes_written = send(m_conn, buf, size, 0);
  WC_ON_ERROR(bytes_written, LOG("GJWifiClient::write conn:%d sent:%d err:%d\n\n", m_conn, bytes_written, (int)errno));

  if (bytes_written == -1 || LostConnection(errno))
  {
    stop();
    return 0;
  }

  return (bytes_written > 0) ? bytes_written : 0;
}

int GJWifiClient::available()
{
  if (m_conn <= 0)
    return 0;

  if (!m_rxBuffer.empty())
    return m_rxBuffer.size();

  int bytes_available = 0;
  ioctl(m_conn,FIONREAD,&bytes_available);

  return bytes_available;
}

int GJWifiClient::read()
{
  uint8_t data = 0;
  int res = read(&data, 1);
  if(res < 0) {
      return res;
  }
  if (res == 0) {  //  No data available.
      return -1;
  }
  return data;
  
  return 0;
}

int GJWifiClient::read(uint8_t *buf, size_t size)
{
  int err;

  do
  {
    int bytes_available = 0;
    ioctl(m_conn,FIONREAD,&bytes_available);
    
    if (!bytes_available)
      break;

    int currentSize = m_rxBuffer.size();
    m_rxBuffer.resize(currentSize + bytes_available);

    int received = recv(m_conn, m_rxBuffer.data() + currentSize, bytes_available, 0);
    WC_ON_ERROR(received, LOG("GJWifiClient::read:netconn_recv: err:%d\n", (int)errno));

    if (received > 0)
      m_rxBuffer.resize(m_rxBuffer.size() - (bytes_available - received));

    if (LostConnection(errno))
    {
      stop();
      return 0;
    }
  }
  while(true);

  int32_t copySize = std::min<int>(m_rxBuffer.size(), size);

  if (size != copySize)
  {
    WC_LOC("GJWifiClient::read %d return:%d\n\r", size, copySize);
  }

  memcpy(buf, m_rxBuffer.data(), copySize);

  m_rxBuffer.erase(m_rxBuffer.begin(), m_rxBuffer.begin() + copySize);

  return copySize;
}

int GJWifiClient::peek()
{
  WC_LOC("GJWifiClient::peek UNIMPLEMENTED!!!\n\r");

  return 0;
}

void GJWifiClient::flush()
{
  WC_LOC("GJWifiClient::flush UNIMPLEMENTED!!!\n\r");
}

void GJWifiClient::stop()
{
  if (m_conn <= 0)
    return;

  int err;
  err = shutdown(m_conn, 0);
  WC_ON_ERROR(err, LOG("~Client:shutdown: err:%d\n", (int)err));
  err = close(m_conn);
  WC_ON_ERROR(err, LOG("~Client:close: err:%d\n", (int)err));
  
  m_conn = -1;
}

uint8_t GJWifiClient::connected()
{
  return m_conn >= 0;
}

GJWifiClient::operator bool()
{
  WC_LOC("GJWifiClient::operator bool UNIMPLEMENTED!!!\n\r");
  return false;
}
