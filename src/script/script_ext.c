/*
	GSC extension registry.
	Files in src/features/ register GSC functions/methods here (from a constructor).
	Scr_GetCustomFunction/Method (script.c) consult it only AFTER the built-in tables
	missed, so existing functions are never shadowed. With no features it does nothing.
*/
#include "script.h"
#include "bmx_features.h"

#define BMX_MAX_EXT 256

static SCRIPTFUNCTION ext_functions[BMX_MAX_EXT];
static int ext_function_count;
static SCRIPTFUNCTION ext_methods[BMX_MAX_EXT];
static int ext_method_count;

void BMX_RegisterFunction(const char *name, SCRIPTFUNCTIONCALL call, int dev) {
	if(!name || !call || ext_function_count >= BMX_MAX_EXT) return;
	ext_functions[ext_function_count].name = name;
	ext_functions[ext_function_count].call = call;
	ext_functions[ext_function_count].developer = dev;
	ext_function_count++;
}

void BMX_RegisterMethod(const char *name, SCRIPTFUNCTIONCALL call, int dev) {
	if(!name || !call || ext_method_count >= BMX_MAX_EXT) return;
	ext_methods[ext_method_count].name = name;
	ext_methods[ext_method_count].call = call;
	ext_methods[ext_method_count].developer = dev;
	ext_method_count++;
}

SCRIPTFUNCTIONCALL BMX_FindExtFunction(const char** fname, int* fdev) {
	for(int i = 0; i < ext_function_count; i++)
		if(!strcmp(*fname, ext_functions[i].name)) {
			*fname = ext_functions[i].name; *fdev = ext_functions[i].developer;
			return ext_functions[i].call;
		}
	return 0;
}

SCRIPTFUNCTIONCALL BMX_FindExtMethod(const char** fname, int* fdev) {
	for(int i = 0; i < ext_method_count; i++)
		if(!strcmp(*fname, ext_methods[i].name)) {
			*fname = ext_methods[i].name; *fdev = ext_methods[i].developer;
			return ext_methods[i].call;
		}
	return 0;
}
