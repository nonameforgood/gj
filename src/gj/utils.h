#include "base.h"

template <typename T, typename Callable>
void BubbleSort(T *begin, T *end, Callable c);


typedef bool (*pfnCompareU16)(uint16_t a, uint16_t b);
void SortU16(uint16_t *begin, uint16_t *end, pfnCompareU16 c);