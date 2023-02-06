#pragma once

//#define GJ_SERVER
#ifdef GJ_SERVER

#include "function_ref.hpp"

struct GJServerArgs
{
    const char *m_hostName = nullptr;
    const char *m_buildDate = nullptr;
    tl::function_ref<void(void)> m_onRequest;
};

void InitServer(GJServerArgs const &args);
void UpdateServer();

#endif //GJ_SERVER
