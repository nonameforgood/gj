#include "../base.h"
#include "../appendonlyfile.h"
#include "tests.h"

void TestAppendOnlyFile()
{
  //It is absolutely necessary to avoid the situation where
  //testing FLASH runs in a loop of some kind.
  //This could happen if the debugger is disconnected and the chip
  //continues to run, crash, restart, run, crash, restart, and so on
  //This would reach the max FLASH erase count (10k) and 
  //destroy it, making the module useless.
  bool doIt = false;
  if (doIt)
  {
    BEGIN_TEST(CumulFile)
    {
      AppendOnlyFile file("/test");
      uint32_t fileData[] = {7, 8, 9, 10};
      uint32_t fileData2[] = {0x7777, 0x8888, 0x9999, 0xaaaa, 0xbbbb};
      uint32_t blockIndex = 0;

      auto onBlock = [&](uint32_t size, const void *data)
      {
        printf("Block %p %d ", data, size);
        for (int i = 0 ; i < (size/4) ; i++)
        {
          printf("%x ", ((uint32_t*)data)[i]);
        }
        printf("\n\r");
      };

      bool isValid = file.IsValid();
      if (isValid)
      {
        file.ForEach(onBlock);
      }

      file.Erase();
      file.BeginWrite(16);
      file.Write(&fileData[0], 4);
      file.Write(&fileData[1], 4);
      file.Write(&fileData[2], 4);
      file.Write(&fileData[3], 4);
      file.Write(&fileData[3], 4);
      file.EndWrite();


      file.BeginWrite(20);
      file.Write(&fileData2[0], 4);
      file.Write(&fileData2[1], 4);
      file.Write(&fileData2[2], 4);
      file.Write(&fileData2[3], 4);
      file.Write(&fileData2[4], 4);
      file.Write(&fileData2[4], 4);
      file.EndWrite();


      file.ForEach(onBlock);
    }

    BEGIN_TEST(FullCumulFile)
    {
      AppendOnlyFile file("/test");

      file.Erase();
      file.BeginWrite(1012);

      uint32_t fileData[] = {0x7777, 0x8888, 0x9999, 0xaaaa, 0xbbbb};

      for (int i = 0 ; i < 1012 ; i+= 4)
      {
        file.Write(&fileData[i % 5], 4);
      }

      file.EndWrite();

      bool canWrite = file.BeginWrite(4);
      TEST_CASE("AppendOnlyFile", canWrite == false);

      auto onBlock = [](uint32_t size, const void *data)
      {
        printf(" ");
      };

      file.ForEach(onBlock);
    }
  }
}