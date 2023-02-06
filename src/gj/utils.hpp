
//std::sort generates too much assembly

template <typename T, typename Callable>
void BubbleSort(T *begin, T *end, Callable c)
{
  bool changed = false;
  T *lastEnd = end - 1;

  bool dirty;
  T* it;
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

          T s = it[0];
          it[0] = it[1];
          it[1] = s;
        }
      }

      ++it;
    }
  }
  while(dirty);
}
