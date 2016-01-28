#!/bin/bash

sudo scp -i ~/.ssh/id_rsa_ucineo -r libosmscout/src/.libs/*.so* root@192.168.0.81:/opt/bin/test-carto/libs/
sudo scp -i ~/.ssh/id_rsa_ucineo -r libosmscout-map/src/.libs/*.so* root@192.168.0.81:/opt/bin/test-carto/libs/
sudo scp -i ~/.ssh/id_rsa_ucineo -r libosmscout-map-cairo/src/.libs/*.so* root@192.168.0.81:/opt/bin/test-carto/libs/
