#!/bin/bash

# compile libosmscout
# yes, once again:
printf "\nBuilding libosmscout\n"
cd libosmscout 
./autogen.sh && ./configure && make
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$(pwd)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/src/.libs
cd ..

# compile libosmscoutimport
printf "\nBuilding libosmscout-import\n"
cd libosmscout-import
./autogen.sh && ./configure && make
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$(pwd)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/src/.libs
cd ..

# compile libosmscoutmap
printf "\nBuilding libosmscout-map\n"
cd libosmscout-map
./autogen.sh && ./configure && make
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$(pwd)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/src/.libs
cd ..

# compile libosmscoutmap-cairo
printf "\nBuilding libosmscout-map-cairo\n"
cd libosmscout-map-cairo
./autogen.sh && ./configure && make
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$(pwd)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/src/.libs
cd ..

# compile libosmscoutmap-qt
printf "\nBuilding libosmscout-map-qt\n"
cd libosmscout-map-qt
./autogen.sh && ./configure && make
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$(pwd)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/src/.libs
cd ..

# compile libosmscoutmap-opengl
printf "\nBuilding libosmscout-map-opengl\n"
cd libosmscout-map-opengl
./autogen.sh && ./configure && make
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$(pwd)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/src/.libs
cd ..

# compile OSMScout2
printf "\nBuilding OSMScout2\n"
cd OSMScout2/
qmake && make
cd ..

# compile the Import tool
printf "\nBuilding Import tool\n"
cd Import/
./autogen.sh && ./configure && make
cd ..

# compile the demos
printf "\nBuilding Demos\n"
cd Demos/
./autogen.sh && ./configure && make
cd ..
