#pragma once

#if defined(ESP32) || defined(ESP_PLATFORM)

#include <vector>
template <typename T>
using Vector = std::vector<T>;

#else

#include <vector>

template <typename T>
using Vector = std::vector<T>;

#if 0
template <typename T>
class Vector
{
public:

  typedef T* iterator;

  uint32_t size() const;
  void resize(uint32_t size);
  void reserve(uint32_t size);
  T* data();
  T const* data() const;

  T& operator[](uint32_t i);

  void push_back(T const &item);
  bool empty() const;

  template<typename L>
  void erase(L l) {}

  T* begin();
  T* end();

  inline T& front() { return *(T*)nullptr;}

  inline void clear() {}

  const T* begin() const;
  const T* end() const;

private:

  T* m_data = nullptr;
  uint32_t m_size = 0;
  uint32_t m_capacity = 0;
};


template <typename T>
T* Vector<T>::begin()
{
  return m_data;
}


template <typename T>
T* Vector<T>::end()
{
  return m_data + m_size;
}



template <typename T>
const T* Vector<T>::begin() const
{
  return m_data;
}


template <typename T>
const T* Vector<T>::end() const
{
  return m_data + m_size;
}


template <typename T>
bool Vector<T>::empty() const
{
  return m_size == 0;
}

template <typename T>
uint32_t Vector<T>::size() const
{
  return m_size;
}

template <typename T>
void Vector<T>::resize(uint32_t size)
{
  T* newData = new T[size];

  uint32_t minCount = Min(size, m_size);

#if defined(ESP32) || defined(ESP_PLATFORM)
  std::copy(m_data, m_data + minCount, newData);
#else
  memcpy(newData, m_data, sizeof(T) * minCount);
#endif

  delete m_data;

  m_data = newData;
  m_size = size;
  m_capacity = size;
}

template <typename T>
void Vector<T>::reserve(uint32_t size)
{
  if (size < m_capacity)
    return;

  T* newData = new T[size];

  uint32_t minCount = Min(size, m_size);
  if (m_data)
  {
#if defined(ESP32) || defined(ESP_PLATFORM)
  std::copy(m_data, m_data + minCount, newData);
#else
  memcpy(newData, m_data, sizeof(T) * minCount);
#endif
  //don't know why, but dtor is called on nullptr when code is optimized
  
    delete [] m_data;
  }
  
  m_data = newData;
  m_capacity = size;
}

template <typename T>
T* Vector<T>::data()
{
  return m_data;
}

template <typename T>
T const * Vector<T>::data() const
{
  return m_data;
}

template <typename T>
T& Vector<T>::operator[](uint32_t i)
{
  return m_data[i];
}


template <typename T>
void Vector<T>::push_back(T const &item)
{
  if (m_size >= m_capacity)
  {
    reserve(size() + 4);
  }

  m_data[m_size] = item;
  m_size++;
}
#endif
#endif