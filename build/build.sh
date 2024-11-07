#!/bin/bash
# Copyright (c), 2023-2024, Huawei Tech. Co., Ltd.

PRJ_DIR=$(dirname $(readlink -f "$0"))
SRC_DIR=${PRJ_DIR}/../src/libso

function print_help()
{
	echo "Usage: $0 OPERATOR OPTIONS"
	echo "OPERATOR: "
	echo "	-i build and install"
	echo "	-b build"
	echo "	-c clean"

}

function clean()
{
	cd ${SRC_DIR}
	rm -rf ./build
}

function build()
{
	echo "start compile"
	cd ${SRC_DIR}
	cmake . -DXD_INSTALL_BINDIR=$1 -B build
	cd build
	make
}

function install() 
{
	cd ${SRC_DIR}
	cmake . -DXD_INSTALL_BINDIR=$1 -B build
	cd build
	make install
}

[ "$1" == "-c" ] && {
	clean
	exit  0
}

[ "$1" == "-b" ] && {
	INSTALL_DIR=$2
	[ -z $2 ] && {
		INSTALL_DIR=/usr/lib64
		mkdir -p ${INSTALL_DIR}
	}

	build ${INSTALL_DIR}
	exit  0
}

[ "$1" == "-i" ] && {
    INSTALL_DIR=$2
    [ -z $2 ] && {
        INSTALL_DIR=/usr/lib64
        mkdir -p ${INSTALL_DIR}
    }
	install ${INSTALL_DIR}
	exit  0
}

if [ -z $1 ] || [ $1 == "-h" ]
	then
		print_help
		exit 0
fi
