/*
	bmx_compat.h - names that other things depend on (GSC scripts, client, tools).

	These strings are visible OUTSIDE this source tree (GSC scripts, cfg files, the game
	client, server-list tools, start scripts). Changing a value silently breaks whatever
	reads it, so they live here in one place. Keep the value unless you also change the
	other side.

	  BMX_SERVERINFO_KEY      serverinfo key / cvar   (getcvar("...") in GSC, client detection)
	  BMX_DATE_CVAR           build-date cvar
	  BMX_CMD_LONG                   console command that prints the version (besides "bmx")
	  BMX_USERINFO_BUILD_KEY  userinfo key the client sends with its build number
*/
#ifndef BMX_COMPAT_H
#define BMX_COMPAT_H

#define BMX_SERVERINFO_KEY      "bmx_server"
#define BMX_DATE_CVAR           "bmx_date"
#define BMX_CMD_LONG            "bmx_server"
#define BMX_USERINFO_BUILD_KEY  "bmxbuild"

#endif
