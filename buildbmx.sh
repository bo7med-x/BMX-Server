#!/bin/bash
# Builds BMX Server (CoD 1.5, debug+unsafe+mysql) inside a Debian buster container.
# Usage: put BMX-Server.zip in $HOME, then: bash ~/buildbmx.sh
set -e
cd ~
rm -rf bmxbuild && mkdir bmxbuild
unzip -q BMX-Server.zip -d /tmp/bmxz
cp -r /tmp/bmxz/BMX-Server/. bmxbuild/
rm -rf /tmp/bmxz

docker run --rm -u root -v "$HOME/bmxbuild":/bmx \
  quay.io/parkervcp/pterodactyl-images:debian_source bash -c '
set -e
cat > /etc/apt/sources.list << EOF
deb http://archive.debian.org/debian buster main contrib non-free
deb http://archive.debian.org/debian-security buster/updates main contrib non-free
EOF
echo "Acquire::Check-Valid-Until \"false\";" > /etc/apt/apt.conf.d/99ignore-valid-until
dpkg --add-architecture i386
apt-get update
apt-get install -y --allow-unauthenticated build-essential gcc-multilib g++-multilib \
  libc6-dev-i386 nasm libz-dev:i386 libssl-dev:i386 default-libmysqlclient-dev:i386 \
  libmariadb-dev:i386 libmariadb3:i386 libcurl4-openssl-dev:i386
ln -sf /usr/bin/mariadb_config /usr/bin/mysql_config
cd /bmx
rm -rf src/obj bin
bash build.sh -5 -d -u -m
SO=/bmx/bin/bmx_server.so
[ -f "$SO" ] || { echo "BUILD FAILED: $SO not found"; exit 1; }
echo "OUTPUT: $SO"
echo "DEBUG strings: $(strings "$SO" | grep -c BMX-DBG)"
strings "$SO" | grep -m1 "built for CODPATCH=5" || echo "WARNING: not a CODPATCH=5 build"
cp "$SO" /bmx/bmx_server.so
'

echo "=== done ==="
ls -la ~/bmxbuild/bmx_server.so
cloudshell download ~/bmxbuild/bmx_server.so
