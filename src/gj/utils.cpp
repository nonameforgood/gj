#include "utils.h"

void SortU16(uint16_t *begin, uint16_t *end, pfnCompareU16 c)
{
  bool changed = false;
  uint16_t *lastEnd = end - 1;

  bool dirty;
  uint16_t* it;
  do
  {
    dirty = false;
    it = begin;

    while(it < lastEnd)
    {
      bool smaller = c(it[0], it[1]);
    
      if (!smaller)
      {
        bool larger = c(it[1], it[0]);

        if (larger)
        {
          dirty = true;

          uint16_t s = it[0];
          it[0] = it[1];
          it[1] = s;
        }
      }

      ++it;
    }
  }
  while(dirty);
}
