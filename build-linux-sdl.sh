#!/bin/bash

if [ -z $MOAI_ROOT ]; then
	echo 'MOAI_ROOT is not set'
	exit 1
fi

if [ ! -d build ]; then
	PROJECT_DIR=$(dirname $(readlink -f "$0"))
	mkdir build
	cd build
	cmake \
	-DBUILD_LINUX=TRUE \
	-DSDL_HOST=TRUE \
	-DMOAI_BOX2D=TRUE \
	-DMOAI_CHIPMUNK=TRUE \
	-DMOAI_CURL=TRUE \
	-DMOAI_CRYPTO=TRUE \
	-DMOAI_EXPAT=TRUE \
	-DMOAI_FREETYPE=TRUE \
	-DMOAI_JSON=TRUE \
	-DMOAI_JPG=TRUE \
	-DMOAI_MONGOOSE=TRUE \
	-DMOAI_LUAEXT=TRUE \
	-DMOAI_OGG=TRUE \
	-DMOAI_OPENSSL=TRUE \
	-DMOAI_SQLITE3=TRUE \
	-DMOAI_TINYXML=TRUE \
	-DMOAI_PNG=TRUE \
	-DMOAI_SFMT=TRUE \
	-DMOAI_VORBIS=TRUE \
	-DMOAI_UNTZ=TRUE \
	-DMOAI_LUAJIT=TRUE \
	-DMOAI_HTTP_CLIENT=TRUE \
	-DCMAKE_BUILD_TYPE=Release \
	-DCUSTOM_HOST=$PROJECT_DIR/cmake \
	-DPLUGIN_DIR=$PROJECT_DIR/plugins \
	-DPLUGIN_MOAI-ENET=1 \
	$MOAI_ROOT/cmake
fi

make $*
