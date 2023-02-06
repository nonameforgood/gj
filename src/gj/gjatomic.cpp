
#include "gjatomic.h"
#include <freertos/atomic.h>

uint32_t AtomicInc(uint32_t &var)
{
    return Atomic_Increment_u32(&var);
}

uint32_t AtomicDec(uint32_t &var)
{
    return Atomic_Decrement_u32(&var);
}

uint32_t AtomicCompareAndExchange(uint32_t &var, uint32_t newValue, uint32_t comparand)
{
    uint32_t ulReturnValue;

    ATOMIC_ENTER_CRITICAL();
    {
        if( var == comparand )
        {
            ulReturnValue = comparand;
            var = newValue;
        }
        else
        {
            ulReturnValue = var;
        }
    }
    ATOMIC_EXIT_CRITICAL();

    return ulReturnValue;
}
