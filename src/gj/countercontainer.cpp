#include "countercontainer.h"
#include "counter.h"

void CounterContainer::AddCounter(Counter &counter)
{
  m_counters.push_back(&counter);
}

bool CounterContainer::IsEmpty() const
{
  return m_counters.empty();
}

bool CounterContainer::CountersHaveData() const
{
  for (Counter* counter : m_counters)
  {
    if (counter->HasData())
      return true;
  }
  return false; 
}

//void CounterContainer::Reset()
//{
//  for (Counter* counter : m_counters)
//      counter->Reset();
//    
//  Init();
//}

