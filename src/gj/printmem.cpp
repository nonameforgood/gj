#include "printmem.h"

void PrintMemLine( uint32_t address, unsigned char const *data, uint16_t length, uint32_t lineLength )
{
  uint32_t const maxLength = Max<int>(length,lineLength);
                             // 11 chars:0x00000000 | bin of maxLength * 2 | space | maxLength chars | \n\r | null char
  uint32_t const bufferLength = 11                  + maxLength * 2        + 1     + maxLength       + 2    + 1         ;
  char buffer[bufferLength + 4]; //+ 4 for some protection
  char *it = buffer;
  
  it += sprintf(it, "0x%05x ", address);
  
  //out hex
  for ( uint32_t j = 0 ; j < length ; ++j )
  {
    it += sprintf(it, "%02x", (uint16_t)data[j]);
  }
  
  for ( uint32_t j = length ; j < lineLength ; ++j )
  {
    strcpy(it, "  ");
    it += 2;
  }

  strcpy(it, "  ");
  it += 2;

  char constexpr space = 0x20;
  char constexpr del = 0x7f;
  
  //out char
  for ( uint32_t j = 0 ; j < length ; ++j )
  {
    if ( data[j] >= space && data[j] != del )
    { 
      it[0] = data[j];
    }
    else
      it[0] = '.';
        
    ++it;
  }
  
  strcpy(it, "\n\r");
  it += 2;
  
  if (it > (buffer + bufferLength))
  {
    LOG("ERROR:PrintMemLine buffer overflow\n\r");
  }
  
  SER(buffer);
}


void PrintMem( uint32_t lineLength, uint32_t address, unsigned char const *data, uint32_t length )
{

  for ( uint32_t i = 0 ; i < length ; i += lineLength )
  {
    uint32_t remaining = length - i;
    uint16_t batchSize = Min<uint32_t>( lineLength, remaining );

    PrintMemLine( address + i, data + i, batchSize, lineLength );
  }

}
