#include "base.h"
#include <string.h>

GJ_PERSISTENT bool g_verbose = false;

bool DoEvery(uint32_t &last, uint32_t duration)
{
  uint32_t const m = GetElapsedMillis();
  if ((m - last) >= duration || last == 0)
  {
    last = m;
    return true;
  }

  return false;
}

uint32_t gj_vsprintf(char *target, uint32_t targetSize, const char *format, va_list vaList )
{
  if (strlen(format) > targetSize )
  {
    sprintf(target, "ERR:gj_vsprintf fmt overflow\n\r");
    return strlen(target);
  }

  uint32_t ret = vsprintf(target, format, vaList);

  if (ret > targetSize)
  {
    //at this point the stack is already corrupted
    sprintf(target, "ERR:gj_vsprintf result overflow");
    return strlen(target);
  }

  return ret;
}

void SetVerbose(bool enable)
{
  g_verbose = enable;
}

bool IsVerbose()
{
  return g_verbose;
}

#if defined(NRF)

#include "ble.h"

struct ErrorString
{
  const uint32_t code;
  const char * const m_string;
};

#define NRF_ERROR_STRING(cat, id) {cat##_##id, #id}

const ErrorString g_errorStrings[] =
{
  NRF_ERROR_STRING(NRF, SUCCESS                    ),
  NRF_ERROR_STRING(NRF_ERROR, SVC_HANDLER_MISSING  ),
  NRF_ERROR_STRING(NRF_ERROR, SOFTDEVICE_NOT_ENABLED),
  NRF_ERROR_STRING(NRF_ERROR, INTERNAL             ),
  NRF_ERROR_STRING(NRF_ERROR, NO_MEM               ),
  NRF_ERROR_STRING(NRF_ERROR, NOT_FOUND            ),
  NRF_ERROR_STRING(NRF_ERROR, NOT_SUPPORTED        ),
  NRF_ERROR_STRING(NRF_ERROR, INVALID_PARAM        ),
  NRF_ERROR_STRING(NRF_ERROR, INVALID_STATE        ),
  NRF_ERROR_STRING(NRF_ERROR, INVALID_LENGTH       ),
  NRF_ERROR_STRING(NRF_ERROR, INVALID_FLAGS        ),
  NRF_ERROR_STRING(NRF_ERROR, INVALID_DATA         ),
  NRF_ERROR_STRING(NRF_ERROR, DATA_SIZE            ),
  NRF_ERROR_STRING(NRF_ERROR, TIMEOUT              ),
  NRF_ERROR_STRING(NRF_ERROR, NULL                 ),
  NRF_ERROR_STRING(NRF_ERROR, FORBIDDEN            ),
  NRF_ERROR_STRING(NRF_ERROR, INVALID_ADDR         ),
  NRF_ERROR_STRING(NRF_ERROR, BUSY                 ),
  NRF_ERROR_STRING(NRF_ERROR, CONN_COUNT           ),
  NRF_ERROR_STRING(NRF_ERROR, RESOURCES            ),
  NRF_ERROR_STRING(BLE_ERROR, GATTS_INVALID_ATTR_TYPE),
  NRF_ERROR_STRING(BLE_ERROR, GATTS_SYS_ATTR_MISSING)
};

const char *ErrorToName(uint32_t err_code)
{
  for ( const ErrorString &s : g_errorStrings)
  {
    if (err_code == s.code)
      return s.m_string;
  }

  return "Unknown";
}
#endif