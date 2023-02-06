#pragma once

#include "base.h"

enum class WSOpCode
{
    ContinuationFrame = 0,
    TextFrame = 1,
    BinaryFrame = 2,
    reserved3 = 3,
    reserved4 = 4,
    reserved5 = 5,    
    reserved6 = 6,
    reserved7 = 7,
    CloseConnection = 8,
    Ping = 9,
    Pong = 10,
    reserved11 = 11,
    reserved12 = 12,
    reserved13 = 13,
    reserved14 = 14,
    reserved15 = 15
};

struct WSFrame
{
  WSFrame();
  WSFrame(const char *text);

  WSOpCode OpCode = WSOpCode::ContinuationFrame;
    
  bool RSV3 = false;
  bool RSV2 = false;
  bool RSV1 = false;
  bool FIN = true;
  bool Mask = false;
  
  uint32_t Payload = 0;

  const void *data = nullptr;
};


struct WSHeader
{
  uint8_t OpCode : 4; 
  
  uint8_t RSV3 : 1;
  uint8_t RSV2 : 1;
  uint8_t RSV1 : 1;
  uint8_t FIN : 1;
  
  uint8_t Payload : 7; 
  uint8_t Mask : 1; 
};
static_assert(sizeof(WSHeader) == 2);


struct WSHeader16
{
    uint16_t Payload;
};
static_assert(sizeof(WSHeader16) == 2);

struct WSHeader64
{
    uint64_t Payload;
};
static_assert(sizeof(WSHeader64) == 8);

struct WSHeaderMask
{
    uint8_t Mask[4];
};
static_assert(sizeof(WSHeaderMask) == 4);

struct WSData
{
  WSData();
  WSData(uint32_t size);
  WSData(uint32_t size, uint8_t *data);
  WSData(WSData &&other);
  ~WSData();

  WSData& operator =(WSData && other);

  uint8_t *m_buffer = nullptr;
  uint32_t m_size = 0;
};

const char *GetOpCodeString(WSOpCode opCode);

void PrintWSHeader(WSHeader const *header);
void PrintWSHeader(WSHeader16 const *header);
void PrintWSHeader(WSHeaderMask const *header);
void PrintWSFrame(WSFrame const *frame);
void PrintWSFrame(WSFrame const &frame);

char* ws_hash_handshake(char* handshake,uint8_t len);
bool prepare_response(char* buf,uint32_t buflen,char* handshake,char* protocol);

const WSFrame DecodeWSFrame(uint8_t* buf, uint16_t buflen);
const WSFrame DecodeWSFrame(WSData const *data);
void EncodeWSFrame(WSFrame const &frame, uint8_t *&buffer, uint32_t &size);

void EncodeWSFrame(WSFrame const &frame, WSData &data);