#!/bin/bash
# BMX Server build script. Run from anywhere: ./build.sh -5
cd "$(dirname "$(readlink -f "$0")")/src" || exit 1

cc="gcc -fcommon -include inc.h -I./include"
#LINK_LIBS="$(/usr/bin/curl-config --static-libs --cflags)";
LINK_LIBS=""
STEAM_INCLUDE=""
STEAM_LINK=""

info()
{
	cat << EOF
	BMX Server - Call of Duty 1 Linux server extension (patch 1.1 / 1.5)

	usage: ./build.sh [-5 | -1] [-d] [-m] [-s] [-u]
	  -5  build for CoD 1.5      -1  build for CoD 1.1 (default)
	  -d  debug build            -m  MySQL support
	  -s  Steam support          -u  unsafe features
EOF
}

OUTPUT_NAME=bmx_server
PATCHVERSION=1
uSTEAM=false
uANY=false
uMYSQL=false
uFEATUREUNSAFE=false
DEBUG=false
DEFINES=""

while getopts “hsmdu15” qo
do
	case $qo in
	h)
		info
		exit 1
		;;
	s)
		uSTEAM=true
		uANY=true
		;;
	m)
		uMYSQL=true
		uANY=true
		;;
	d)
		DEBUG=true
		;;
	u)
		uFEATUREUNSAFE=true
		uANY=true
		;;
	1)
		PATCHVERSION=1
		;;
	5)
		PATCHVERSION=5
		;;
	esac
done

if [ $uSTEAM = true ]; then
STEAM_INCLUDE="-I/home/r/deps/steamsdk/sdk/public"
STEAM_LINK="-L/home/r/deps/steamsdk/sdk/redistributable_bin/linux32 -lsteam_api"
fi

if [ $PATCHVERSION = 5 ]; then
echo "Call of Duty Patch 1.5"
else
echo "Call of Duty Patch 1.1"
fi

if [ $DEBUG = true ]; then
echo "{ BMX SERVER DEVELOPMENT BUILD }"
else
echo "{ BMX SERVER RELEASE BUILD }"
fi

if [ $uANY = true ]; then
echo "Using: "
fi

if [ $uMYSQL = true ]; then
	echo -n "MYSQL, "
fi

if [ $uFEATUREUNSAFE = true ]; then
	echo -n "Unsafe features, "
fi

if [ $uANY = true ]; then
echo ""
fi

#Compiling BMX Server

compiler="$cc -Os -O1 -O3 -s -fvisibility=hidden -w -Wl,--exclude-libs,ALL"

if [ $DEBUG = true ]; then
compiler="$cc -g -w -Wl,--exclude-libs,ALL"
#compiler="$cc -DxDEBUG -DDEBUG -Os -O1 -O3 -s -fvisibility=hidden -w -Wl,--exclude-libs,ALL"
fi

if [ $uMYSQL = true ]; then
DEFINES+="-DuMYSQL "
fi

if [ $uFEATUREUNSAFE = true ]; then
DEFINES+="-DuFEATUREUNSAFE "
fi

if [ $DEBUG = true ]; then
DEFINES+="-DxTRACE "
fi

if [ $PATCHVERSION = 5 ]; then
DEFINES+="-DCODPATCH=5 "
else
DEFINES+="-DCODPATCH=1 "
fi

if [ $uSTEAM = true ]; then
DEFINES+="-DSTEAM_SUPPORT "
fi

params="$DEFINES -std=c99 -I. -m32 -fPIC -Wno-write-strings"
#params="-m32 -fPIC -Wno-write-strings"

mkdir -p ../bin
mkdir -p obj

echo -e "\nCOMPILING"

#echo "[CPPP]"
#for f in cppp/*.c; do
#	filename=$(basename "$f")
#	extension="${filename##*.}"
#	filename="${filename%.*}"
#	$compiler $params -c cppp/$filename.c -o obj/$filename.o
#done


echo "[ROOT]"
$compiler $params -c core/init.c -o obj/init.o
$compiler $params -c core/librarymodule.c -o obj/librarymodule.o
$compiler $params -c core/bmx_server.c -o obj/bmx_server.o
echo "[COMMON]"
$compiler $params -c common/cvar.c -o obj/cvar.o
$compiler $params -c common/cmd.c -o obj/cmd.o
$compiler $params -c common/msg.c -o obj/msg.o
$compiler $params -c common/curl.c -o obj/curl.o
$compiler $params -c common/util.c -o obj/util.o
echo "[GAME]"
$compiler $params -c common/shared.c -o obj/shared.o
$compiler $params -c script/script.c -o obj/script.o
$compiler $params -c game/bg_pmove.c -o obj/bg_pmove.o
$compiler $params -c game/bg_misc.c -o obj/bg_misc.o
$compiler $params -c game/g_utils.c -o obj/g_utils.o
$compiler $params -c game/g_spawn.c -o obj/g_spawn.o
$compiler $params -c game/g_active.c -o obj/g_active.o
$compiler $params -c common/q_math.c -o obj/q_math.o
$compiler $params -c common/files.c -o obj/files.o
$compiler $params -c game/gsc_bots.c -o obj/gsc_bots.o
echo "[SERVER]"
nasm -f elf server/sv_snapshot.asm -o obj/sv_snapshot_asm.o
$compiler $params -c server/sv_snapshot.c -o obj/sv_snapshot.o
$compiler $params -c server/sv_commands.c -o obj/sv_commands.o
$compiler $params -c server/sv_client.c -o obj/sv_client.o
$compiler $params -c server/sv_world.c -o obj/sv_world.o
$compiler $params -c server/sv_game.c -o obj/sv_game.o
$compiler $params -c server/sv_init.c -o obj/sv_init.o
$compiler $params -c server/net_chan.c -o obj/net_chan.o
$compiler $params -c server/sv_main.c -o obj/sv_main.o
$compiler $params -c server/x_clientcmds.c -o obj/x_clientcmds.o
$compiler $params -c server/unix_net.c -o obj/unix_net.o
$compiler $params -c server/webserver.c -o obj/webserver.o

if [ $uSTEAM = true ]; then
g++ -include inc.h -Os -O1 -O3 -s -fvisibility=hidden -w -Wl,--exclude-libs,ALL -I. -I./include -m32 -fPIC -Wno-write-strings $STEAM_INCLUDE -c server/steam.cpp -o obj/steam.o
fi

echo "[SCRIPT]"
$compiler $params -c script/pre.c -o obj/pre.o
$compiler $params -c script/scr_method_player.c -o obj/scr_method_player.o
$compiler $params -c script/scr_string.c -o obj/scr_string.o
$compiler $params -c script/scr_fields.c -o obj/scr_fields.o
$compiler $params -c script/scr_method_entity.c -o obj/scr_method_entity.o
$compiler $params -c script/scr_sqlite.c -o obj/scr_sqlite.o
$compiler $params -c script/script_ext.c -o obj/script_ext.o

if [ $uMYSQL = true ]; then
$compiler $params -c script/scr_mysql.c -o obj/scr_mysql.o
fi

echo "[FEATURES]"
for f in features/*.c; do
	[ -e "$f" ] || continue
	$compiler $params -c "$f" -o "obj/feat_$(basename "$f" .c).o"
done

obj="$(ls obj/*.o)"

#if [ $DEBUG = true ]; then
#for f in obj/*.o; do
#if [ "$f" != "duktape.o" ] && [ "$f" != "steam.o" ]; then
#objcopy --keep-symbols=symbols.txt --prefix-symbols=bmx_ $f
#objcopy --redefine-syms=symbs.txt $f
#objcopy --prefix-symbols=`basename $f .o`_ $f
#objcopy --prefix-symbols=bmx_ $f
#echo "placeholder" > /dev/null
#fi
#done
#fi

if [ $uMYSQL = true ]; then
if [ $DEBUG = true ]; then
$compiler -m32 -shared -L/lib32 -L/home/lib `mysql_config --libs --include` -o ../bin/$OUTPUT_NAME.so $obj -Os -lz $LINK_LIBS $STEAM_LINK -ldl -lm -Wall
else
$compiler -m32 -shared -L/lib32 -L/home/lib `mysql_config --libs --include` -o ../bin/$OUTPUT_NAME.so $obj -Os -s -lz $LINK_LIBS $STEAM_LINK -ldl -lm -Wall
fi
#$compiler -m32 -shared -L/lib32 `mysql_config --libs` `mysql_config --include` -o ../bin/$OUTPUT_NAME.so $obj -Os -s -lz $LINK_LIBS -ldl -lm -Wall
else
if [ $DEBUG = true ]; then
$compiler -m32 -shared -L/lib32 -L./lib -o ../bin/$OUTPUT_NAME.so $obj -lz $LINK_LIBS $STEAM_LINK -ldl -lm -Wall
else
$compiler -m32 -shared -L/lib32 -L./lib -o ../bin/$OUTPUT_NAME.so $obj -Os -s -lz $LINK_LIBS $STEAM_LINK -ldl -lm -Wall
fi
fi
#rm -rf ./obj
find ./obj -name '*.o' ! -name 'duktape.o' -delete
if [ ! -f "../bin/$OUTPUT_NAME.so" ]; then
	echo "BUILD FAILED: ../bin/$OUTPUT_NAME.so was not produced"
	exit 1
fi
echo "Done: bin/$OUTPUT_NAME.so"
