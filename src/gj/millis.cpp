#include "millis.h"
#include <arduino.h>

static int32_t gjMillisScale = 1;
static int32_t gjMillisOffset = 0;

void GJSetMillisScaleOffset(int32_t scale, int32_t offset)
{
	gjMillisScale = scale;
	gjMillisOffset = offset;
}

uint32_t GJMillis()
{
	return millis() * gjMillisScale + gjMillisOffset;
}