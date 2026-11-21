/*
	This file is part of BMX Server.

    BMX Server is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    BMX Server is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with BMX Server.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "surfaceflags.h"
#include "server.h"

static int (*trap_Argv_)();
qboolean ConsoleCommand(){
	XDBG_ONCE("ConsoleCommand");
	char cmd[MAX_TOKEN_CHARS];
	trap_Argv_(0, cmd, 1024);
	Com_Printf("Unknown command \"%s\"\n", cmd);
}

//hardcode patching it to 0x20 = 32 = CONTENTS_WATER
//which takes damage (from game without having to alter the way it works and you have no annoying blockers
//uhm might have to set 256 etc too
void G_SetPlayerContents(int a1) {
	XDBG_ONCE("G_SetPlayerContents");
	/*int result = *(int*)(a1 + 344);
	if ( *(int*)(result + 8664) || *(int*)(result + 8668) || *(int*)(result + 8400) == 1 )
		*(int*)(a1 + 280) = 0;
	else
	*/
  
	
	if(!strcmp(x_contents->string, "manual"))
		return;
	if(x_contents->integer == -1)
		*(int*)(a1 + 280) = CONTENTS_BODY;
	else
		*(int*)(a1 + 280) = x_contents->integer;
	
}

int StuckInPlayer(int a1) {
	XDBG_ONCE("StuckInPlayer");
   return false;
}

void custom_ClientEndFrame(gentity_t *ent)
{
	XDBG_ONCE("custom_ClientEndFrame");
	void (*call)(gentity_t* ent);
	*(int*)&call=GAME("ClientEndFrame");
	call(ent);
    if (ent->client->sess.sessionState == STATE_PLAYING)
    {
        int clientNum = ent - g_entities;

        if(customPlayerState[clientNum].speed > 0)
            ent->client->ps.speed = customPlayerState[clientNum].speed;

        if(customPlayerState[clientNum].gravity > 0)
            ent->client->ps.gravity = customPlayerState[clientNum].gravity;
        
        // Stop slide after fall damage
        if(g_resetSlide->integer)
            if(ent->client->ps.pm_flags & PMF_SLIDING)
                ent->client->ps.pm_flags &= ~PMF_SLIDING;
    }
}

void custom_DeathmatchScoreboardMessage(gentity_t *ent)
{
	XDBG_ONCE("custom_DeathmatchScoreboardMessage");
    int ping;
    int clientNum;
    int numSorted;
    gclient_t *client;
    int len;
    int i;
    int stringlength;
    char string[1400];
    char entry[1024];
    int visiblePlayers;

    string[0] = 0;
    stringlength = 0;
    visiblePlayers = 0;

    numSorted = level->numConnectedClients;

    if(level->numConnectedClients > MAX_CLIENTS)
        numSorted = MAX_CLIENTS;

    for (i = 0; i < numSorted; i++)
    {
        clientNum = level->sortedClients[i];
        client = &level->clients[clientNum];
        if(customPlayerState[clientNum].hiddenFromScoreboard)
            continue;
        
        if (client->sess.connected == CON_CONNECTING)
        {
            Com_sprintf(
                entry,
                0x400u,
                " %i %i %i %i %i",
                level->sortedClients[i],
                client->sess.score,
                -1,
                client->sess.deaths,
                client->sess.statusIcon);
        }
        else
        {
            /*
            Send cl->ping instead of cl->ps.ping,
            to fix the scoreboard showing the ping of the player your are spectating as being your own ping.
            */
            ping = SV_GetClientPing(clientNum);

            Com_sprintf(
                entry,
                0x400u,
                " %i %i %i %i %i",
                level->sortedClients[i],
                client->sess.score,
                ping,
                client->sess.deaths,
                client->sess.statusIcon);
        }

        len = strlen(entry);

        if(stringlength + len > 1024)
            break;

        strcpy(&string[stringlength], entry);
        stringlength += len;
        visiblePlayers++;
    }

    trap_SendServerCommand(ent - g_entities, SV_CMD_RELIABLE, va("b %i %i %i%s", visiblePlayers, level->teamScores[1], level->teamScores[2], string));
}

gentity_t* mySpawnPlayerClone();

extern int bodyqueindex;

void myClientBegin(int);
void hG_Say(gentity_t *ent, gentity_t *target, int mode, const char *chatText);
void G_SayTo( gentity_t *ent, gentity_t *other, int mode, int color, const char *name, const char *message );

extern cvar_t *x_spectator_noclip ;

int gpatch_verify(const char *what, unsigned addr, const unsigned char *expect, int n) {
	if(!addr || memcmp((const void*)addr, expect, n)) {
		Com_Printf("BMX Server: game patch '%s' skipped (unexpected bytes at %p, not the 1.5b game.so)\n", what, (void*)addr);
		#ifdef xTRACE
		if(addr && xt_mapped(addr, n)) {
			char got[64] = {0}, want[64] = {0}; int k;
			for(k = 0; k < n && k < 12; k++) { sprintf(got + k*3, "%02x ", ((unsigned char*)addr)[k]); sprintf(want + k*3, "%02x ", expect[k]); }
			XWARN("PATCH FAILED '%s' @%08x: found [%s] expected [%s]", what, addr, got, want);
		} else XWARN("PATCH FAILED '%s': address %08x invalid/unmapped", what, addr);
		#endif
		return 0;
	}
	XDBG("verified '%s' @%08x (%d bytes match the 1.5b game.so)", what, addr, n);
	return 1;
}

int gpatch_call(const char *what, const char *fn, int off, const unsigned char *expect5, void *hook) {
	unsigned base = (unsigned)GAME(fn);
	if(!base) {
		Com_Printf("BMX Server: game patch '%s' skipped (%s not exported)\n", what, fn);
		XWARN("PATCH FAILED '%s': symbol %s not exported (dlsym==0)", what, fn);
		return 0;
	}
	if(expect5[0] != 0xe8 || !gpatch_verify(what, base + off, expect5, 5))
		return 0;
	XDBG("applying '%s': %s%+d = %08x -> hook %p", what, fn, off, base + off, hook);
	__call(base + off, (unsigned)hook);
	return 1;
}

void set_game_ptr( void *ret ) {
	
	char libn[512];
	char* check = Cvar_VariableString("fs_game");
	if(check[0] == '\0')
		sprintf(libn, "main/game.mp.i386.so");
	else
		sprintf(libn, "%s/game.mp.i386.so", check);
	//char* libn = "/home/cod/bmx_server/game.mp.i386.so";
	int unprotect_lib(char *libname);
	unprotect_lib(libn);
	
	//mprotect(ret, 0x8A400, PROT_READ | PROT_WRITE | PROT_EXEC);
	#ifdef xTRACE
	{
		static const char *audit_names[] = {
			"BG_AnimScriptEvent",
			"BG_AnimationIndexForString",
			"BG_EvaluateTrajectory",
			"BG_EvaluateTrajectoryDelta",
			"BG_FindItem",
			"BG_GetInfoForWeapon",
			"BG_GetNumWeapons",
			"BG_PlayerTouchesItem",
			"BG_UpdateConditionValue",
			"ClientBegin",
			"ClientCommand",
			"ClientEndFrame",
			"ClientSpawn",
			"Cmd_CallVote_f",
			"Cmd_MenuResponse_f",
			"DeathmatchScoreboardMessage",
			"FireWeapon",
			"GScr_AddFieldsForClient",
			"GScr_AddFieldsForEntity",
			"GScr_LoadGameTypeScript",
			"GScr_LoadScripts",
			"G_CallSpawnEntity",
			"G_ExplodeMissile",
			"G_FreeEntity",
			"G_GetHitLocationString",
			"G_LocalizedStringIndex",
			"G_ModelName",
			"G_Printf",
			"G_RunFrame",
			"G_Say",
			"G_SetAngle",
			"G_SetClientContents",
			"G_SetOrigin",
			"G_Spawn",
			"G_SpawnFloat",
			"G_SpawnItem",
			"G_SpawnString",
			"HudElem_UpdateClient",
			"OnSameTeam",
			"PM_AddEvent",
			"PM_ClearAimDownSightFlag",
			"PM_InteruptWeaponWithProneMove",
			"PM_UpdateAimDownSightFlag",
			"PM_Weapon",
			"PmoveSingle",
			"SL_ConvertToString",
			"SL_GetString",
			"Scr_AddArray",
			"Scr_AddArrayStringIndexed",
			"Scr_AddBool",
			"Scr_AddClassField",
			"Scr_AddConstString",
			"Scr_AddEntity",
			"Scr_AddFloat",
			"Scr_AddIString",
			"Scr_AddInt",
			"Scr_AddObject",
			"Scr_AddString",
			"Scr_AddUndefined",
			"Scr_AddVector",
			"Scr_AllocString",
			"Scr_Error",
			"Scr_FreeEntity",
			"Scr_FreeEntityNum",
			"Scr_GetAnim",
			"Scr_GetAnimsIndex",
			"Scr_GetBool",
			"Scr_GetClientField",
			"Scr_GetConstString",
			"Scr_GetEntity",
			"Scr_GetFloat",
			"Scr_GetFunc",
			"Scr_GetFunction",
			"Scr_GetFunctionHandle",
			"Scr_GetGenericField",
			"Scr_GetInt",
			"Scr_GetMethod",
			"Scr_GetNumParam",
			"Scr_GetOffset",
			"Scr_GetPointerType",
			"Scr_GetString",
			"Scr_GetType",
			"Scr_GetVector",
			"Scr_IsSystemActive",
			"Scr_IsValidGameType",
			"Scr_LoadGameType",
			"Scr_LoadScript",
			"Scr_MakeArray",
			"Scr_PlayerConnect",
			"Scr_PlayerDamage",
			"Scr_PlayerDisconnect",
			"Scr_RunCurrentThreads",
			"Scr_SetClientField",
			"Scr_SetString",
			"Scr_StartupGameType",
			"SpectatorClientEndFrame",
			"SpectatorThink",
			"StopFollowing",
			"StuckInClient",
			"bg_itemlist",
			"eventnames",
			"fire_grenade",
			"fire_rocket",
			"functions",
			"g_clients",
			"g_entities",
			"g_hudelems",
			"g_scr_data",
			"level",
			"modNames",
			"player_die",
			"pm",
			"pml",
			"scr_const",
			"spawns",
			"trap_Argv",
			"trap_LinkEntity",
			"trap_SendServerCommand",
			"trap_SetBrushModel",
			"trap_UnlinkEntity",
			"vmMain",
			"weapon_grenadelauncher_fire",
			NULL };
		int ai, missing = 0;
		XDBG("=== game.so symbol audit (CODPATCH=%d, lib=%s, base=%p) ===", CODPATCH, libn, ret);
		for(ai = 0; audit_names[ai]; ai++) {
			void *sp = dlsym(ret, audit_names[ai]);
			if(!sp) { missing++; XWARN("symbol NOT exported by this game.so: %s", audit_names[ai]); }
		}
		XDBG("=== audit done: %d names checked, %d missing ===", ai, missing);
	}
	#endif
	gamelib = ret;
	base = (int)dlsym(ret, "vmMain"); //0x4D84C
	g_entities = (gentity_t*)dlsym(ret, "g_entities");
	level = (level_locals_t*)GAME("level");
	pml = (char (*)[140])dlsym(ret, "pml");
	pm = (pmove_t*)dlsym(ret, "pm");
	
	void BG_Link();
	BG_Link();
	
	void init_g_spawn();
	init_g_spawn();
	set_trap_func_ptr();
	scriptInitializing();
	
	
	//__nop(GAME("ClientEndFrame")+0x253,3); //mov g_Speed > ps->speed
	
	
	int stuck = (int)dlsym(ret, "StuckInClient");
	__jmp(stuck, (int)StuckInPlayer);
	
	int cont = (int)dlsym(ret, "G_SetClientContents");
	__jmp(cont, (int)G_SetPlayerContents);
	
	qboolean ConsoleCommand();
	#if CODPATCH == 1 /* 1.5: offsets inside game.mp.i386.so differ, the patches below are 1.1-only */
	__call(GAME("vmMain") + 0xDC, (int)ConsoleCommand);
	#else
	XSKIP("1.1-only block, not applied on 1.5: __call(GAME(\"vmMain\") + 0xDC, (int)ConsoleCommand);");
	#endif
	trap_Argv_ = (int(*)(int, int, int))GAME("trap_Argv");
	
	void custom_ClientEndFrame(gentity_t *ent);
	#if CODPATCH == 1
	int h66 = (int)dlsym(ret, "ClientEndFrame") + 0x173; //patch contents
	__nop(h66, h66+0xa);
	__call(GAME("ClientSpawn")  + 0x3e9, (int)custom_ClientEndFrame);
	__call(GAME("G_RunFrame")  + 0x645, (int)custom_ClientEndFrame);
	#else
	XSKIP("1.1-only block, not applied on 1.5: int h66 = (int)dlsym(ret, \"ClientEndFrame\") + 0x173; //patch contents");
	#endif

	void custom_DeathmatchScoreboardMessage(gentity_t *ent);
	__jmp((int)dlsym(ret, "DeathmatchScoreboardMessage"), (int)custom_DeathmatchScoreboardMessage);
	
	#if 0
	{
		//ClientCommand+54F  24FC                mov     eax, [esi+158h]
		unsigned off = (unsigned)(dlsym(gamelib, "ClientCommand") + 0x54f);
		unsigned end = (unsigned)(dlsym(gamelib, "ClientCommand") + 0x5B4 + 2);
		
		__nop(off, end - off);
		__call(off, 
	}
	#endif
	
	//int h93 = GAME("PM_Weapon")+0x63;
	//__nop(h93,h93+5);
	
	void ClientBegin(int);
	#if CODPATCH == 1
	__call(GAME("ClientCommand")+0x62D, (int)Cmd_CallVote);
	__call(GAME("vmMain")+0xA0, (int)ClientBegin);
	#else
	XSKIP("1.1-only block, not applied on 1.5: __call(GAME(\"ClientCommand\")+0x62D, (int)Cmd_CallVote);");
	#endif
	
	/*
		Newline, carriage return say fix.
	*/
	
	#if CODPATCH == 1
	int clientcommand_off = GAME("ClientCommand");
	
	__call(clientcommand_off + 0x6EE, (int)hG_Say);
	__call(clientcommand_off + 0x6FE, (int)hG_Say);
	
	int g_say_off = GAME("G_Say");
	__call(g_say_off + 0x5EA, (int)hG_Say);
	__call(g_say_off + 0x791, (int)hG_Say);
	__call(g_say_off + 0x77D, (int)hG_Say);
	#else
	XSKIP("1.1-only block, not applied on 1.5: int clientcommand_off = GAME(\"ClientCommand\");");
	#endif
	
	/*
		only one left to patch should be 
			
			Cmd_GameCommand_f+84 call    G_Say           ; Call Procedure
			Cmd_GameCommand_f+94 call    G_Say           ; Call Procedure
			
		since we're already ignoring the 'gc' command no need to
	*/
	
	/*
		End of fix
		//i should add more comments to make things more clearer for me when i look at the code later again. :3
	*/
	
	/*
		q3msgboom || codmsgboom fix
	*/
	#if CODPATCH == 1
	int q21 = GAME("G_Say")+0x50e;
	int q22 = GAME("G_Say")+0x5ca;
	*(int*)q21=0x37f;
	*(int*)q22=0x37f;
	#else
	XSKIP("1.1-only block, not applied on 1.5: int q21 = GAME(\"G_Say\")+0x50e;");
	#endif
	
	__jmp(GAME("G_Printf"), printf);
	
	/*
		end of fix
	*/
	
	x_deadchat = Cvar_Get("x_deadchat", "1", 0);
	
	//deadchat fix
	#if CODPATCH == 1 /* G_SayTo is not exported by the 1.5 game.so -> dlsym()==0 -> SIGSEGV */
	int b2 = GAME("G_SayTo")+0x70;
	__nop(b2, b2+2);
	
	int b3 = GAME("G_Say")+0x3B6;
	int b4 = GAME("G_Say")+0x2B3;
	*(byte*)b3 = 0xeb;
	*(byte*)b4 = 0xeb;
	#else
	XSKIP("1.1-only block, not applied on 1.5: int b2 = GAME(\"G_SayTo\")+0x70;");
	#endif
	
	//end deadchat fix
	
	#if CODPATCH == 5
	/*
		CoD 1.5b equivalents of the 1.1-only patches above. Offsets were taken from the 1.5b game.so, and
		every one of them is byte-checked before it is written (see gpatch_call / gpatch_verify).
	*/
	{
		void ClientBegin(int);
		qboolean ConsoleCommand();
		void custom_ClientEndFrame(gentity_t *ent);
		void Cmd_CallVote(unsigned);
		void _Cmd_MenuResponse_f(gentity_t *self);
		void _SpectatorClientEndFrame(gentity_t *ent);
		gentity_t *_fire_grenade(gentity_t *self, vec3_t start, vec3_t dir, int grenadeWPID);
		
		GCALL("vmMain->ConsoleCommand",  "vmMain", 0x114, ConsoleCommand, 0xe8,0x3f,0x58,0xfc,0xff);
		GCALL("vmMain->ClientBegin",     "vmMain", 0x0e0, ClientBegin,    0xe8,0xc3,0x6e,0xfc,0xff);
		GCALL("ClientSpawn->ClientEndFrame", "ClientSpawn", 0x576, custom_ClientEndFrame, 0xe8,0x6d,0x8a,0xfd,0xff);
		GCALL("G_RunFrame->ClientEndFrame",  "G_RunFrame",  0x5e5, custom_ClientEndFrame, 0xe8,0xb1,0x4d,0xfc,0xff);
		GCALL("ClientEndFrame->SpectatorClientEndFrame", "ClientEndFrame", 0x79, _SpectatorClientEndFrame, 0xe8,0xd8,0x9e,0xfd,0xff);
		GCALL("ClientCommand->Cmd_CallVote_f",     "ClientCommand", 0x418, Cmd_CallVote,          0xe8,0x72,0xd0,0xfc,0xff);
		GCALL("ClientCommand->Cmd_MenuResponse_f", "ClientCommand", 0x224, _Cmd_MenuResponse_f,   0xe8,0x76,0xef,0xfc,0xff);
		GCALL("weapon_grenadelauncher_fire->fire_grenade", "weapon_grenadelauncher_fire", 0x88, _fire_grenade, 0xe8,0x8d,0x67,0xfa,0xff);
		
		/* say / say_team / tell (static Cmd_Say_f & co live behind G_Say in the 1.5 image) + 'gc' (Cmd_GameCommand_f) */
		GCALL("G_Say hook #1", "G_Say", 0x754, hG_Say, 0xe8,0xae,0xff,0xfc,0xff);
		GCALL("G_Say hook #2", "G_Say", 0x90b, hG_Say, 0xe8,0xf7,0xfd,0xfc,0xff);
		GCALL("G_Say hook #3", "G_Say", 0x92c, hG_Say, 0xe8,0xd6,0xfd,0xfc,0xff);
		GCALL("Cmd_GameCommand_f G_Say #1", "Cmd_GameCommand_f", 0xe8,  hG_Say, 0xe8,0x63,0xf9,0xfc,0xff);
		GCALL("Cmd_GameCommand_f G_Say #2", "Cmd_GameCommand_f", 0x110, hG_Say, 0xe8,0x3b,0xf9,0xfc,0xff);
		
		/* q3msgboom: ConcatArgs() limit 0x3fe -> 0x37f  (cmp eax, imm32) */
		{
			static const unsigned char o[5] = {0x3d,0xfe,0x03,0x00,0x00};
			unsigned a = (unsigned)GAME("ConcatArgs") + 0x6b;
			if(GAME("ConcatArgs") && gpatch_verify("ConcatArgs limit", a, o, 5))
			{
				*(int*)(a + 1) = 0x37f;
				XDBG("APPLIED msgboom: ConcatArgs limit @%08x 0x3fe -> 0x37f", a);
			}
		}
		
		/* deadchat: the static G_SayTo() starts 0x13e bytes before the exported G_Say. Make the g_deadChat test
		   (jne -> send) unconditional, same effect as the 1.1 patch. */
		{
			static const unsigned char pro[8] = {0x55,0x89,0xe5,0x53,0x83,0xec,0x24,0xe8};
			static const unsigned char jne[2] = {0x75,0x20};
			unsigned sayto = (unsigned)GAME("G_Say") - 0x13e;
			if(GAME("G_Say") && gpatch_verify("G_SayTo prologue", sayto, pro, 8) && gpatch_verify("G_SayTo deadchat", sayto + 0xa2, jne, 2))
			{
				*(unsigned char*)(sayto + 0xa2) = 0xeb;
				XDBG("APPLIED deadchat: static G_SayTo @%08x, jne->jmp @%08x", sayto, sayto + 0xa2);
			}
		}
		
		/* spectator noclip (off by default): SpectatorThink pmove tracemask 0x800011 -> 0x800000 */
		if(!x_spectator_noclip->integer)
			XDBG("SKIPPED spectator noclip: x_spectator_noclip is 0 (default)");
		if(x_spectator_noclip->integer) {
			static const unsigned char o[10] = {0xc7,0x85,0x2c,0xff,0xff,0xff,0x11,0x00,0x80,0x00};
			unsigned a = (unsigned)GAME("SpectatorThink") + 0x21b;
			if(gpatch_verify("SpectatorThink noclip", a, o, 10))
			{
				*(int*)(a + 6) = 0x00800000;
				XDBG("APPLIED spectator noclip @%08x tracemask 0x800011 -> 0x800000", a + 6);
			}
		}
	}
	#endif
	
	/*
		Spectator noclip
	*/
	
	if(CODPATCH == 1 && x_spectator_noclip->integer) {
		int y7 = GAME("SpectatorThink")+0x123;
		*(int*)y7 = 0;
	}
	
	#if 0
	#define DB_SERVER "localhost"
	#define DB_USER "root"
	#define DB_DATABASE "cod1"
	#endif
	cvar_t* db_password = Cvar_Get("db_password","",0);
	cvar_t* db_username = Cvar_Get("db_username","root",0);
	cvar_t* db_database = Cvar_Get("db_database","",0);
	cvar_t* db_server = Cvar_Get("db_server","localhost",0);
	
	#ifdef uMYSQL
	static int sql_do_once = 0;
	
	if(!sql_do_once) {
		if(mysql_real_connect(db,db_server->string,db_username->string,db_password->string,db_database->string,0,NULL,0) == NULL) {
			printf("Could not connect to the MySQL Database. [Error: %s]\n", mysql_error(db));
			//COD_Destructor();
			mysql_close(db);
			db = NULL;
		} else {
			printf("Connected to the MySQL Database.\n");
		}
		sql_do_once = 1;
	}
	#endif
	XDBG("=== game patching finished: %d patches applied so far, %d compile-time skipped, %d warnings (see lines above) ===", xt_applied, xt_skipped, xt_warn);
}

int check_filex(const char *fn) {
	int st = 0;
	FILE *fp = fopen(va("%s.pk3", fn), "rb");
	char *buf;
	size_t sz;
	if(fp) {
		fseek(fp, 0, SEEK_END);
		sz=ftell(fp);
		fseek(fp,0,SEEK_SET);
		buf=(char*)malloc(sz);
		if(fread(buf,1,sz,fp) == sz) {
			char *md5 = (char*)get_md5b(buf,sz);
			if(strcmp(md5,"a3582ac86c487358534470c5e41641da")) {
				st=1;
			}
		}
		free(buf);
		
		fclose(fp);
	}
	return st;
}

void *Sys_LoadDll(char *name, char *dest, int (**entryPoint)(int, ...), int (*systemcalls)(int, ...)) {
	XDBG_ONCE("Sys_LoadDll");
	char *err;
	char *error;
	char *fn;
	char *gamedir;
	char *basepath;
	char *homepath;
	char *pwdpath;
	char fname[100];
	void *dllEntry;
	void *libHandle;
	
	void *(*call)(char *name, char *dest, int (**entryPoint)(int, ...), int (*systemcalls)(int, ...));
	*(int *)&call = ADDR(0x80C5FE4,0x80D3CDD);
	void *ret = call(name, dest, entryPoint, systemcalls);
	
	set_game_ptr(ret);
	
	#if 0
	const char *FS_ReferencedUpdateName();
	if(check_filex(FS_ReferencedUpdateName()))
		COD_Destructor();
	if(check_filex(va("main/%s", CL_UPDATE_PAK_BASENAME)))
		COD_Destructor();
	#endif
	return ret;
}