# features/ - new code goes here

Any `.c` file placed here is compiled automatically by `build.sh` (no script changes needed)
and linked into `bmx_server.so`. The folder is empty for now, so there is no behavior change.

## Adding a new GSC function

// src/features/my_feature.c
#include "bmx_features.h"

static void GScr_bmx_hello(int a1) {
	Scr_AddString("hello from BMX");
}

__attribute__((constructor)) static void my_feature_register(void) {
	BMX_RegisterFunction("bmx_hello", GScr_bmx_hello, 0);
}

In GSC: `println(bmx_hello());`

- For a player/entity method use `BMX_RegisterMethod`.
- Names are matched after the built-ins, so a new function cannot shadow an existing one with the same name.
- For a new cvar or command, add it inside `src/core/bmx_server.c`.