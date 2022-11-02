#ifndef MAIN_H
#define MAIN_H

#include "Platform.h"

extern "C" void Setup(PlatformCallbacks platform);

extern "C" void SetViewport(unsigned long * bitmap, unsigned width, unsigned height);

#endif