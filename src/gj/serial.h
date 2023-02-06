#pragma once

#include "function_ref.hpp"
#include "stl/function.h"
#include <stdarg.h>


void __attribute__ ((noinline)) gjSerialString(const char *buffer);
void __attribute__ ((noinline)) gjFormatSerialString(const char *format, ...);
void __attribute__ ((noinline)) gjOutputSerialLargeString(const char *str);
bool IsSerialEnabled();
bool IsTraceEnabled();


#define SER(s, ...)  gjFormatSerialString(s, ##__VA_ARGS__) 

//ser(s, ##__VA_ARGS__) 
#define SER_LARGE(s) {if(IsSerialEnabled()) gjOutputSerialLargeString(s);} 
#define SER_COND(c, s, ...) {if((c)) SER(s, ##__VA_ARGS__);} 
#define ON_SER(s) {if(IsSerialEnabled()) s;}

#define TRACE(s, ...) {if(IsSerialEnabled() && IsTraceEnabled()) gjFormatSerialString(s, ##__VA_ARGS__);} 

typedef std::function<void(const char*)> TerminalHandler;
typedef std::function<bool()> TerminalReadyHandler;

void InitSerial(uint32_t rate = 115200);
void UpdateSerial();

uint32_t AddTerminalHandler(TerminalHandler handler, TerminalReadyHandler ready = nullptr);
void RemoveTerminalHandler(uint32_t index);

bool AreTerminalsReady();

uint32_t AddEtsHandler(TerminalHandler handler);
void RemoveEtsHandler(uint32_t index);
