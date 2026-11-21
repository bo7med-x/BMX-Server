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

#ifndef X_UTIL_H
#define X_UTIL_H

#include <sys/mman.h>
#include <string.h>
#include "openssl/md5.h"

typedef unsigned char byte;

static int search_memory(int current, int end, byte* bytes, size_t len) {
	while((memcmp((void*)current, bytes, len)) && (current < end))
		current++;
	return (current == end) ? -1 : current;
}

static int patch_memory(int start, int end, byte* search_for, byte* patched, size_t len) {
	int s = search_memory(start, end, search_for, len);
	if(-1 != s)
		memcpy((void*)s, patched, len);
	return s;
}

struct list_element_t {
	void *data;
	struct list_element_t *next;
};

typedef struct list_element_t *LinkedList;

#define new(x, y) (x*)malloc(sizeof(x) * y)
#define list_empty(list) (list == NULL)
#define list_each_element(it, list) \
	struct list_element_t *it = list; \
	for(;it!=NULL;it=it->next)
#define list_each(type, it, list) \
	struct list_element_t *LIST_CURRENT_ELEMENT = list; type it = (list) ? list->data : NULL; \
	for(;LIST_CURRENT_ELEMENT!=NULL;it=(type)LIST_CURRENT_ELEMENT->data,LIST_CURRENT_ELEMENT=LIST_CURRENT_ELEMENT->next)
#define list_remove(list, e) do { \
	struct list_element_t *LIST_TEMP_ELEMENT = list; \
	if(e == list) { \
		list = list->next; free(e->data); free(e); \
	} else { \
		while(LIST_TEMP_ELEMENT->next != e) LIST_TEMP_ELEMENT = LIST_TEMP_ELEMENT->next; \
		LIST_TEMP_ELEMENT->next = e->next; free(e->data); free(e); \
	} \
	} while(0)

typedef void (*Com_Error_t)(int code, const char *fmt, ...);
extern Com_Error_t Com_Error;

static void list_clear(LinkedList *list) {
	if(*list == NULL)
		return;
		
	LinkedList tmp = *list;
	
	while(*list) {
		tmp = *list;
		*list = (*list)->next;
		free(tmp->data);
		free(tmp);
	}
	*list = NULL;
}

static LinkedList _list_add(LinkedList *list, void *data) {
	LinkedList n = NULL, cur = *list;
	n = (LinkedList)malloc(sizeof(*n));
	if(n == NULL) {
		Com_Error(0, "Out of memory!");
		return NULL;
	}
	n->next = NULL;
	n->data = data;
	if(!cur)
		*list = n;
	else {
		while(cur->next)
			cur = cur->next;
		cur->next = n;
	}
	return n;
}

// You must free the result if result is non-NULL.
char *str_replace(char *orig, char *rep, char *with);
size_t filesize(const char* file);
int startsWith(const char *str, const char *pre);

/*
	Hooking
*/

/*
	Debug tracing (build with ./build.sh -d  ->  -DxTRACE)
	Everything is written to stdout AND appended to bmx_server_debug.log in the server's working directory.
	Without xTRACE every macro below compiles to nothing.
*/
#ifdef xTRACE
#include <stdarg.h>
#include <sys/mman.h>
static int xt_applied, xt_skipped, xt_warn;
static void xt_log(const char *file, int line, const char *fmt, ...) {
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	const char *slash = strrchr(file, '/');
	printf("[BMX-DBG %s:%d] %s\n", slash ? slash + 1 : file, line, buf);
	fflush(stdout);
	FILE *f = fopen("bmx_server_debug.log", "a");
	if(f) {
		fprintf(f, "[%s:%d] %s\n", slash ? slash + 1 : file, line, buf);
		fclose(f);
	}
}
static int xt_mapped(unsigned a, unsigned n) {
	unsigned char vec[16];
	unsigned start = a & ~0xfffu;
	return mincore((void*)start, ((a + n - 1) & ~0xfffu) - start + 0x1000, vec) == 0;
}
#define XDBG(...) xt_log(__FILE__, __LINE__, __VA_ARGS__)
#define XDBG_ONCE(name) do { static int _xt_once; if(!_xt_once) { _xt_once = 1; XDBG("HOOK FIRED (first call): %s", name); } } while(0)
#define XSKIP(desc) do { xt_skipped++; XDBG("SKIPPED (not for this binary, compile-time): %s", desc); } while(0)
#define XWARN(...) do { xt_warn++; XDBG("WARNING: " __VA_ARGS__); } while(0)
#else
#define XDBG(...) ((void)0)
#define XDBG_ONCE(name) ((void)0)
#define XSKIP(desc) ((void)0)
#define XWARN(...) ((void)0)
#endif

static void xp_nop(unsigned int start, unsigned int end, const char *expr, const char *file, int line) {
	int len = ( end < start ) ? end : ( end - start );
	#ifdef xTRACE
	if(!xt_mapped(start, len)) { xt_warn++; xt_log(file, line, "NOP %s: address %08x NOT MAPPED, len %d -> would crash!", expr, start, len); return; }
	{
		char hex[128] = {0}; int k;
		for(k = 0; k < len && k < 16; k++) sprintf(hex + k*3, "%02x ", ((unsigned char*)start)[k]);
		xt_applied++;
		xt_log(file, line, "NOP  %-40s addr=%08x len=%d old bytes: %s", expr, start, len, hex);
	}
	#endif
	mprotect((void *)start, len, PROT_READ | PROT_WRITE | PROT_EXEC);
	memset((void*)start, 0x90, len);
}

static void xp_jmp(unsigned int off, unsigned int loc, const char *expr, const char *file, int line) {
	#ifdef xTRACE
	/* some hooks (e.g. _Scr_RunCurrentThreads) re-install their jmp every frame:
	   log each distinct (addr,target) only once so the log is not flooded */
	{
		static unsigned int seen[128][2]; static int nseen;
		int i, dup = 0;
		for(i = 0; i < nseen; i++) if(seen[i][0] == off && seen[i][1] == loc) { dup = 1; break; }
		if(dup) goto xp_jmp_write;
		if(nseen < 128) { seen[nseen][0] = off; seen[nseen][1] = loc; nseen++; }
	}
	if(!xt_mapped(off, 5)) { xt_warn++; xt_log(file, line, "JMP %s: address %08x NOT MAPPED -> would crash!", expr, off); return; }
	{
		unsigned char *p = (unsigned char*)off;
		xt_applied++;
		xt_log(file, line, "JMP  %-40s addr=%08x -> %08x old bytes: %02x %02x %02x %02x %02x%s", expr, off, loc, p[0], p[1], p[2], p[3], p[4],
			(p[0] == 0x55 || p[0] == 0xe9 || p[0] == 0x53 || p[0] == 0x57 || p[0] == 0x56) ? "" : "   <-- WARNING: unusual function prologue, is this really a function start?");
	}
	xp_jmp_write: ;
	#endif
	mprotect((void*)off, 5, PROT_READ | PROT_WRITE | PROT_EXEC);
	*(unsigned char*)off = 0xe9;
	int foffset = loc - (off + 5);
	memcpy((void*)(off + 1), &foffset, 4);
}

static void xp_call(unsigned int off, unsigned int loc, const char *expr, const char *file, int line) {
	#ifdef xTRACE
	if(!xt_mapped(off, 5)) { xt_warn++; xt_log(file, line, "CALL %s: address %08x NOT MAPPED -> would crash!", expr, off); return; }
	{
		unsigned char *p = (unsigned char*)off;
		int rel; memcpy(&rel, p + 1, 4);
		if(p[0] != 0xe8) {
			xt_warn++;
			xt_log(file, line, "CALL %-40s addr=%08x: opcode is %02x, NOT E8 (call)! bytes %02x %02x %02x %02x %02x -> WRONG ADDRESS, NOT patched", expr, off, p[0], p[0], p[1], p[2], p[3], p[4]);
			return;
		}
		xt_applied++;
		xt_log(file, line, "CALL %-40s addr=%08x orig target=%08x -> new target=%08x", expr, off, off + 5 + rel, loc);
	}
	#endif
	mprotect((void *)off, 5, PROT_READ | PROT_WRITE | PROT_EXEC);
	int foffset = loc - (off + 5);
	memcpy((void *)(off + 1), &foffset, 4);
}

#define __nop(a, b)  xp_nop((a), (b), #a, __FILE__, __LINE__)
#define __jmp(a, b)  xp_jmp((a), (b), #a, __FILE__, __LINE__)
#define __call(a, b) xp_call((a), (b), #a, __FILE__, __LINE__)

/*
	MD5
*/

static char xhash_md5[33];

static char* get_md5(char* str) {
	unsigned char digest[MD5_DIGEST_LENGTH];
	MD5((unsigned char*)str, strlen(str), (unsigned char*)&digest);
	for(int i = 0; i < 16; i++)
		sprintf(&xhash_md5[i*2], "%02x", (unsigned int)digest[i]);
	return &xhash_md5[0];
}

static char *get_md5b(unsigned char *buf, size_t len) {
	unsigned char digest[MD5_DIGEST_LENGTH];
	MD5(buf, len, (unsigned char*)&digest);
	for(int i = 0; i < 16; i++)
		sprintf(&xhash_md5[i*2], "%02x", (unsigned int)digest[i]);
	return &xhash_md5[0];
}

static char* get_pass_hash(char* password, char* salt) {
	char tmp[32*((!strcmp(salt,""))?1:2)+1];
	if(strcmp(salt,"")) {
		sprintf(tmp, "%s", get_md5(salt));
		sprintf(tmp, "%s%s", tmp, get_md5(password));
	} else {
		sprintf(tmp, "%s", get_md5(password));
	}
	return get_md5(tmp);
}


static void dumpbase(int* base, size_t len, const char *fn) {
	static char dumpbase_path[64];
	
	snprintf(dumpbase_path, 63, "./dumps/%s.dump", fn);
	
	unsigned int result;
	float rf;
	char* rstr;
	
	FILE* f = fopen(dumpbase_path, "wb");
	if(f) {
		fprintf(f, "File: %s\nDUMPFILE\n\n");
		
		for(unsigned int i = 0; i < len; i++) {
			result = *(unsigned int*)(base + i);
			rf = *(float*)(base + i);
			rstr = (char*)(base + i);
			fprintf(f, "%d: Hex: %x, Decimal: %d, Float: %f, String: %s\n", i, result, result, rf, rstr);
		}
		fclose(f);
	}
	
	printf("Successfully dumped '%s' to location '%s'.\n", fn, dumpbase_path);
}


/*
	Thanks to kung foo man for providing unprotect_lib
*/

static int unprotect_lib(char *libname)
{
    char buf[512];
    char flags[4];
    void *low, *high;
    FILE *fp;
    fp = fopen("/proc/self/maps", "r");
    if ( ! fp)
        return 0;
    while (fgets(buf, sizeof(buf), fp))
    {
        if ( ! strstr(buf, libname))
            continue;
        if (sscanf (buf, "%p-%p %4c", &low, &high, flags) != 3)
            continue;
        //printf(buf);
        //printf("%08x %08x diff:%.8p\n", low, high, (int)high-(int)low);
        mprotect((void *)low, (int)high-(int)low, PROT_READ | PROT_WRITE | PROT_EXEC);
    }
    fclose(fp);
    return 1;
}

static inline void xor_crypt(char *key, char *string, int n) {
    int i;
	int c_use = 'x';
    int keyLength = strlen(key);
    for(i = 0; i < n; i++)
        string += string[i] ^ (c_use /*+i*/) % 255;//string[i] = string[i] ^ key[i%keyLength];
}

#endif