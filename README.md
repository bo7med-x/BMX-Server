# BMX Server

Call of Duty 1 Linux server extension for patch 1.5 (patch 1.1 build is also supported).
Builds `bin/bmx_server.so`, which is injected into the stock `cod_lnxded` binary with `LD_PRELOAD`.

BMX Server is a fork of [codextended](https://github.com/riicchhaarrd/codextended), ported and extended for CoD 1.5.

## Features

- Extra GSC functions and player/entity methods
- Q3 exploit fixes and rate limiting
- SQLite support in GSC, optional MySQL support
- Bot support
- Built-in web server
- Optional Steam support and optional unsafe features
- Run CoD 1.1 mods (MiscMod, CoDaM) on 1.5 with minor script adjustments
- Drop-in feature system: any `.c` file in `src/features/` is compiled automatically

## Requirements

- Linux (32-bit capable toolchain)
- `gcc` with 32-bit support (`gcc-multilib`)
- `nasm`
- 32-bit `zlib` and `libssl` development packages
- For MySQL builds (`-m`): 32-bit MySQL/MariaDB client development package (`mysql_config` must be available)
- For Steam builds (`-s`): the Steam SDK (see the paths at the top of `build.sh`)

Debian / Ubuntu example:

```
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install build-essential gcc-multilib nasm zlib1g-dev:i386 libssl-dev:i386
```

## Build

```
./build.sh -5        # CoD 1.5   (-1 = 1.1, the default)
./build.sh -5 -d     # debug build
./build.sh -5 -m     # + MySQL      -s  + Steam      -u  + unsafe features
./build.sh -h        # show usage
```

Flags can be combined, for example `./build.sh -5 -d -u -m`.
The script can be run from any directory. Output: `bin/bmx_server.so`.

### Build in a container

`buildbmx.sh` builds a CoD 1.5 debug + unsafe + MySQL binary inside a Debian buster Docker
container, which is useful when the host toolchain is too new.

```
# put BMX-Server.zip in $HOME, then:
bash ~/buildbmx.sh
```

Requires Docker. Output: `~/bmxbuild/bin/bmx_server.so`.

### Release builds

Release builds (without `-d`) are stripped. For debug builds you can strip debug info
afterwards with `scripts/stripdebuginfo.sh`.

## Installation

1. Build with `./build.sh -5` (or use your own build of `bmx_server.so`).
2. Copy `bin/bmx_server.so` next to your `cod_lnxded` binary.
3. Start the server with the library preloaded:

```
LD_PRELOAD=./bmx_server.so ./cod_lnxded +set dedicated 2 +set net_ip 0.0.0.0 +set net_port 28960 +exec server.cfg
```

The library patches the engine and the game module at startup. A build for 1.5 only works
with a 1.5 server binary, and a build for 1.1 only with a 1.1 binary.

Use `./build.sh -5` for a 1.5 server and `./build.sh -1` for a 1.1 server. Do not mix them.

## Usage

- Print the version from the server console: `bmx` or `bmx_server`
- GSC functions and methods are registered in `src/script/script.c` and `src/script/scr_method_*.c`
- To add your own functions, see `src/features/README.md`

## Mod compatibility (1.1 mods on 1.5)

You can run MiscMod and CoDaM made for CoD 1.1 on a 1.5 server with BMX Server.
Some small script changes may be needed, but after those changes they run successfully.
This has been tested on several 1.1 mods and they work after adjustment.

Bugs and rough edges are still possible. If you find one, please open an issue.
Pull requests and forks are welcome, so feel free to fork the project and improve it.

## Layout

```
build.sh              build script (compiles every file listed in it + src/features/*.c)
buildbmx.sh           Docker based build helper (Debian buster)
scripts/              helper scripts
src/core/             entry point, init, game .so patching (librarymodule.c)
src/common/           cvar, cmd, msg, math, files, util
src/game/             bg_*, g_* (game-side hooks), bots
src/server/           sv_* , net, webserver, steam
src/script/           GSC: script.c (function tables), scr_method_*, strings, sqlite/mysql
src/features/         NEW CODE GOES HERE (auto-compiled) - see features/README.md
src/include/          all headers
legacy/               old unused symbol list
```

## External names

Strings that GSC scripts, the client or server tools read (serverinfo key, build-date cvar,
userinfo build key, version command) are all in `src/include/bmx_compat.h`:
`bmx_server`, `bmx_date`, `bmxbuild`; console commands `bmx` and `bmx_server` print the version.
**GSC scripts / client that read the old names must be updated to these.**

## Troubleshooting

- `fatal error: ... No such file` for 32-bit headers: install `gcc-multilib` and the `:i386` dev packages.
- `nasm: command not found`: install `nasm`.
- `mysql_config: not found` when using `-m`: install the 32-bit MySQL/MariaDB client dev package.
- Server crashes on start: make sure the build target (`-5` or `-1`) matches your `cod_lnxded` version.

## Credits

Based on [codextended](https://github.com/riicchhaarrd/codextended) by riicchhaarrd and contributors,
licensed under GPLv3. Ported to CoD 1.5 and extended as BMX Server.

## License

GPLv3, see `LICENSE`.