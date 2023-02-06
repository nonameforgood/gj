#pragma once
#include "base.h"

uint32_t AtomicInc(uint32_t &var);
uint32_t AtomicDec(uint32_t &var);
uint32_t AtomicCompareAndExchange(uint32_t &var, uint32_t newValue, uint32_t comparand);