#include "ws.h"
#include <mbedtls/base64.h>
#include <mbedtls/sha1.h>

/*
   0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-------+-+-------------+-------------------------------+
     |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
     |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
     |N|V|V|V|       |S|             |   (if payload len==126/127)   |
     | |1|2|3|       |K|             |                               |
     +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
     |     Extended payload length continued, if payload len == 127  |
     + - - - - - - - - - - - - - - - +-------------------------------+
     |                               |Masking-key, if MASK set to 1  |
     +-------------------------------+-------------------------------+
     | Masking-key (continued)       |          Payload Data         |
     +-------------------------------- - - - - - - - - - - - - - - - +
     :                     Payload Data continued ...                :
     + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
     |                     Payload Data continued ...                |
     +---------------------------------------------------------------+
*/

WSFrame::WSFrame() = default;

WSFrame::WSFrame(const char *text)
{
  OpCode = WSOpCode::TextFrame;
  Payload = strlen(text);
  data = text;
}

WSData::WSData() = default;
WSData::WSData(uint32_t size)
{
  m_buffer = new uint8_t[size];
  m_size = size;
}

WSData::WSData(uint32_t size, uint8_t *data)
{
  m_buffer = new uint8_t[size];
  m_size = size;

  memcpy(m_buffer, data, size);
}


WSData::WSData(WSData &&other)
{
  delete m_buffer;

  m_buffer = other.m_buffer;
  m_size = other.m_size;

  other.m_buffer = nullptr;
  other.m_size = 0;
}

WSData::~WSData()
{
  delete m_buffer;
  m_buffer = 0;
  m_size = 0;
}

WSData& WSData::operator =(WSData && other)
{
  delete m_buffer;

  m_buffer = other.m_buffer;
  m_size = other.m_size;

  other.m_buffer = nullptr;
  other.m_size = 0;

  return *this;
}

const char *GetOpCodeString(WSOpCode opCode)
{
  const char *name[] = {
    "continuation frame",
    "text frame",
    "binary frame",
    "reserved for further non-control frames",
    "reserved for further non-control frames",
    "reserved for further non-control frames",
    "reserved for further non-control frames",
    "reserved for further non-control frames",
    "connection close",
    "ping",
    "pong",
    "reserved for further control frames",
    "reserved for further control frames",
    "reserved for further control frames",
    "reserved for further control frames",
    "reserved for further control frames"};

  return name[(int)opCode];
}

void PrintWSHeader(WSHeader const *header)
{
  SER("FIN:%d\n\r", header->FIN);
  SER("RSV1:%d\n\r", header->RSV1);
  SER("RSV2:%d\n\r", header->RSV2);
  SER("RSV3:%d\n\r", header->RSV3);
  SER("OpCode:%d\n\r", header->OpCode);
  SER("Payload:%d\n\r", header->Payload);
  SER("Mask:%d\n\r", header->Mask);
}


void PrintWSHeader(WSHeader16 const *header)
{
  SER("Payload:%d\n\r", header->Payload);
}


void PrintWSHeader(WSHeaderMask const *header)
{
  SER("Mask:%d,%d,%d,%d\n\r", header->Mask[0], header->Mask[1], header->Mask[2], header->Mask[3]);
}


void PrintWSFrame(WSFrame const &frame)
{
  PrintWSFrame(&frame);
}

void PrintWSFrame(WSFrame const *frame)
{
  printf("FIN:%d\n\r", frame->FIN);
  printf("RSV1:%d\n\r", frame->RSV1);
  printf("RSV2:%d\n\r", frame->RSV2);
  printf("RSV3:%d\n\r", frame->RSV3);
  printf("OpCode:%s(%d)\n\r", GetOpCodeString(frame->OpCode), (int)frame->OpCode);
  printf("Payload:%d\n\r", frame->Payload);
  printf("Mask:%d\n\r", frame->Mask);
}

const WSFrame DecodeWSFrame(WSData const *data)
{
  if (!data)
    return {};

  return DecodeWSFrame(data->m_buffer, (uint16_t)data->m_size);

}
const WSFrame DecodeWSFrame(uint8_t* buf, uint16_t buflen)
{
  if (!buf)
    return {};
    
  WSHeader *header = (WSHeader*)buf;
  buf += sizeof(WSHeader);

  //PrintWSHeader(header);

  uint32_t length = header->Payload;

  if (header->Payload == 126)
  {
    //uint16_t u16Length;
    //memcpy(&u16Length, buf, 2);
    //length = u16Length;
    length = ((uint32_t)buf[0] << 8) | ((uint32_t)buf[1] << 0);
    buf += 2;
    printf("Payload 16:%d\n\r", length);
  }
  else if (header->Payload == 127)
  {
    uint64_t large;
    memcpy(&large, buf, 8);
    length = (uint32_t)large;
    buf += 8;
  }

  uint8_t mask[4];

  if (header->Mask != 0)
  {
    memcpy(&mask, buf, 4);
    buf += 4;

    for (uint32_t i = 0 ; i < length ; ++i)
    {
      buf[i] = buf[i] ^ mask[i % 4]; 
    }
  }

  WSFrame frame;
  frame.OpCode = (WSOpCode)header->OpCode;
  frame.RSV1 = header->RSV1 != 0;
  frame.RSV2 = header->RSV2 != 0;
  frame.RSV3 = header->RSV3 != 0;
  frame.Mask = header->Mask != 0;
  frame.Payload = length;
  frame.data = buf;

  return frame;
}

void EncodeWSFrame(WSFrame const &frame, WSData &userData)
{
  WSData wsData;

  WSHeader header;
  WSHeader16 header16 = {};
  WSHeader64 header64 = {};
  WSHeaderMask headerMask = {(uint8_t)(rand() % 0xff), (uint8_t)(rand() % 0xff), (uint8_t)(rand() % 0xff), (uint8_t)(rand() % 0xff)};
  
  header.FIN = frame.FIN;
  header.RSV1 = frame.RSV1;
  header.RSV2 = frame.RSV2;
  header.RSV3 = frame.RSV3;
  header.Mask = frame.Mask;
  header.OpCode = (uint8_t)frame.OpCode;
  
  uint32_t headerSize = sizeof(WSHeader);

  if (frame.Mask)
    headerSize += sizeof(WSHeaderMask);
  
  if (frame.Payload < 126)
  {
    header.Payload = (uint8_t)frame.Payload;
  }
  else if (frame.Payload < 65536)
  {
      header.Payload = (uint8_t)126;
      header16.Payload = frame.Payload;
      headerSize += sizeof(WSHeader16);
  }
  else 
  {
      header.Payload = (uint8_t)127;
      header64.Payload = frame.Payload;
      headerSize += sizeof(WSHeader64);
  }
  
  //PrintWSHeader(&header);

  //if (frame.Mask)
   //   PrintWSHeader(&headerMask);

  wsData.m_size = headerSize + frame.Payload;
  wsData.m_buffer = new uint8_t[wsData.m_size];
  
  uint8_t *it = wsData.m_buffer;
  
  memcpy(it, &header, sizeof(WSHeader));  
  it += sizeof(WSHeader);
  
  if (header16.Payload)
  {
      it[0] = (uint8_t)(header16.Payload >> 8);
      it[1] = (uint8_t)(header16.Payload & 0xff);
      //memcpy(it, &header16, sizeof(WSHeader16));  
      it += sizeof(WSHeader16);
  }
  
  if (header64.Payload)
  {
      memcpy(it, &header64, sizeof(WSHeader64));  
      it += sizeof(WSHeader64);
  }
  if (frame.Mask)
  {
    memcpy(it, &headerMask, sizeof(WSHeaderMask));  
    it += sizeof(WSHeaderMask);
  }

  memcpy(it, frame.data, frame.Payload);
  
  if (frame.Mask)
  {
    for (uint32_t i = 0 ; i < frame.Payload ; ++i)
    {
        it[i] = it[i] ^ headerMask.Mask[i % 4]; 
    }
  }

  userData = std::move(wsData);
}


char* ws_hash_handshake(char* handshake,uint8_t len) {
  const char hash[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  const uint8_t hash_len = sizeof(hash);
  char* ret;
  char key[64];
  unsigned char sha1sum[20];
  unsigned int ret_len;

  if(!len) return NULL;
  ret = (char*)malloc(32);

  memcpy(key,handshake,len);
  strlcpy(&key[len],hash,sizeof(key));
  mbedtls_sha1((unsigned char*)key,len+hash_len-1,sha1sum);
  mbedtls_base64_encode(NULL, 0, &ret_len, sha1sum, 20);
  if(!mbedtls_base64_encode((unsigned char*)ret,32,&ret_len,sha1sum,20)) {
    ret[ret_len] = '\0';
    return ret;
  }
  free(ret);
  return NULL;
}

 bool prepare_response(char* buf,uint32_t buflen,char* handshake,char* protocol) {
  const char WS_HEADER[] = "Upgrade: websocket\r\n";
  const char WS_KEY[] = "Sec-WebSocket-Key: ";
  const char WS_RSP[] = "HTTP/1.1 101 Switching Protocols\r\n" \
                        "Upgrade: websocket\r\n" \
                        "Connection: Upgrade\r\n" \
                        "Sec-WebSocket-Accept: %s\r\n" \
                        "%s\r\n";

  char* key_start;
  char* key_end;
  char* hashed_key;

  if(!strstr(buf,WS_HEADER)) return 0;
  if(!buflen) return 0;
  key_start = strstr(buf,WS_KEY);
  if(!key_start) return 0;
  key_start += 19;
  key_end = strstr(key_start,"\r\n");
  if(!key_end) return 0;

  hashed_key = ws_hash_handshake(key_start,key_end-key_start);
  if(!hashed_key) return 0;
  if(protocol) {
    char tmp[256];
    sprintf(tmp,WS_RSP,hashed_key,"Sec-WebSocket-Protocol: %s\r\n");
    sprintf(handshake,tmp,protocol);
  }
  else {
    sprintf(handshake,WS_RSP,hashed_key,"");
  }
  free(hashed_key);
  return 1;
}