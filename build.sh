#!/bin/bash

# compile libosmscout
# yes, once again:
printf "\nBuilding libosmscout\n"
cd libosmscout 
make
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$(pwd)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/src/.libs
cd ..

# compile libosmscoutimport
printf "\nBuilding libosmscout-import\n"
cd libosmscout-import
make
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$(pwd)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/src/.libs
cd ..

# compile libosmscoutmap
printf "\nBuilding libosmscout-map\n"
cd libosmscout-map
make
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$(pwd)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/src/.libs
cd ..

# compile libosmscoutmap-cairo
printf "\nBuilding libosmscout-map-cairo\n"
cd libosmscout-map-cairo
make
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$(pwd)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/src/.libs
cd ..

# compile libosmscoutmap-qt
printf "\nBuilding libosmscout-map-qt\n"
cd libosmscout-map-qt
make
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$(pwd)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/src/.libs
cd ..

# compile libosmscoutmap-opengl
printf "\nBuilding libosmscout-map-opengl\n"
cd libosmscout-map-opengl
make
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$(pwd)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/src/.libs
cd ..

# compile OSMScout2
printf "\nBuilding OSMScout2\n"
cd OSMScout2/
make
cd ..

# compile the Import tool
printf "\nBuilding Import tool\n"
cd Import/
make
cd ..

# compile the demos
printf "\nBuilding Demos\n"
cd Demos/
make
cd ..

echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH" >> ~/.bashrc
echo "export LD_LIBRARY_PATH" >> ~/.bashrc
