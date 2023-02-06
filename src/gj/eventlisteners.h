#pragma once

template <typename T>
class EventListeners
{
public:
    EventListeners(const char *name);
    void AddListener(T cb);
    void CallListeners();
private:
    const char *m_name;
    Vector<T> m_callbacks;
};


template <typename T>
EventListeners<T>::EventListeners(const char *name)
: m_name(name)
{
}

template <typename T>
void EventListeners<T>::AddListener(T cb)
{
  m_callbacks.push_back(cb);
}

template <typename T>
void EventListeners<T>::CallListeners()
{
    //SER("Calling %s listeners\n\r", m_name);
    for (T &cb : m_callbacks)
    {
        //SER("Calling %s event listener\n\r", m_name);
        cb();
    }
}
