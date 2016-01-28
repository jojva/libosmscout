#!/bin/bash

SYSROOT=/home/dev/workspace/sivm-gen2-produit/third_party/generic3_ucineo/i586-generic3-linux-gnu/sysroot

export CPP="/home/dev/workspace/sivm-gen2-produit/third_party/generic3_ucineo/bin/i586-generic3-linux-gnu-cpp"
export CC="/home/dev/workspace/sivm-gen2-produit/third_party/generic3_ucineo/bin/i586-generic3-linux-gnu-gcc"
export CXX="/home/dev/workspace/sivm-gen2-produit/third_party/generic3_ucineo/bin/i586-generic3-linux-gnu-g++"
export LD="/home/dev/workspace/sivm-gen2-produit/third_party/generic3_ucineo/bin/i586-generic3-linux-gnu-ld"

export CPPFLAGS="--sysroot=$SYSROOT -march=atom"
export CFLAGS="--sysroot=$SYSROOT -march=atom"
export CXXFLAGS="--sysroot=$SYSROOT -march=atom"

export PKG_CONFIG_DIR=""
export PKG_CONFIG_LIBDIR="$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"

# compile libosmscout
# yes, once again:
printf "\nBuilding libosmscout\n"
cd libosmscout
if [ "$1" == "clean" ]; then
	./autogen.sh && ./configure --host=i586-generic3-linux-gnu --with-sysroot=$SYSROOT && make
else
	make
fi
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$(pwd)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/src/.libs
cd ..

# compile libosmscout-import
printf "\nBuilding libosmscout-import\n"
cd libosmscout-import
if [ "$1" == "clean" ]; then
	./autogen.sh && ./configure --host=i586-generic3-linux-gnu --with-sysroot=$SYSROOT && make
else
	make
fi
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$(pwd)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/src/.libs
cd ..

# compile libosmscoutmap
printf "\nBuilding libosmscout-map\n"
cd libosmscout-map
if [ "$1" == "clean" ]; then
	./autogen.sh && ./configure --host=i586-generic3-linux-gnu --with-sysroot=$SYSROOT && make
else
	make
fi
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$(pwd)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/src/.libs
cd ..

# compile libosmscoutmap-cairo
printf "\nBuilding libosmscout-map-cairo\n"
cd libosmscout-map-cairo
if [ "$1" == "clean" ]; then
	./autogen.sh && ./configure --host=i586-generic3-linux-gnu --with-sysroot=$SYSROOT && make
else
	make
fi
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$(pwd)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/src/.libs
cd ..

# compile libosmscoutmap-qt
#printf "\nBuilding libosmscout-map-qt\n"
#cd libosmscout-map-qt
#if [ "$1" == "clean" ]; then
#	./autogen.sh && ./configure --host=i586-generic3-linux-gnu --with-sysroot=$SYSROOT && make
#else
#	./autogen.sh && ./configure --host=i586-generic3-linux-gnu --with-sysroot=$SYSROOT && make
#fi
#export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$(pwd)
#export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/src/.libs
#cd ..

# compile libosmscoutmap-opengl
#printf "\nBuilding libosmscout-map-opengl\n"
#cd libosmscout-map-opengl
#if [ "$1" == "clean" ]; then
#	./autogen.sh && ./configure --host=i586-generic3-linux-gnu --with-sysroot=$SYSROOT && make
#else
#	make
#fi
#export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$(pwd)
#export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/src/.libs
#cd ..

# compile StyleEditor
#printf "\nBuilding StyleEditor\n"
#cd StyleEditor
#if [ "$1" == "clean" ]; then
#	qmake && make
#else
#	qmake && make
#fi
#export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$(pwd)
#export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/src/.libs
#cd ..
