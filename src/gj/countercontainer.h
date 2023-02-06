#pragma once

#include "Vector.h"

class Counter;

class CounterContainer
{
  public:
    void AddCounter(Counter &counter);
    
    bool IsEmpty() const;
    
    template<typename T>
    void ForEachCounter(T &callable);
    
    template<typename T>
    void ForEachCounter(T &callable) const;

    bool CountersHaveData() const;
    
private:
  Vector<Counter*> m_counters;
};

template<typename T>
void CounterContainer::ForEachCounter(T &callable)
{
  for (Counter* counter : m_counters)
      callable(*counter);
}


template<typename T>
void CounterContainer::ForEachCounter(T &callable) const
{
  for (Counter const* counter : m_counters)
      callable(*counter);
}