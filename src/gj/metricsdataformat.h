#pragma once

#include "string.h"

class MetricsDataFormat
{
  public:

    virtual uint16_t GetID() const = 0;
    virtual const char *GetDescription() const = 0;
    virtual String ToURIString(const uint8_t *data, uint32_t size) const = 0;
};