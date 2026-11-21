/* API for adding GSC functions/methods from src/features/*.c without touching script.c */
#ifndef BMX_FEATURES_H
#define BMX_FEATURES_H
#include "script.h"

/* name = GSC name (lowercase), call = void fn(int a1), dev = 0 */
void BMX_RegisterFunction(const char *name, SCRIPTFUNCTIONCALL call, int dev);
void BMX_RegisterMethod(const char *name, SCRIPTFUNCTIONCALL call, int dev);

#endif
