#!/bin/bash
cd cmake-build-debug || exit
cmake ..
cd ..
cmake --build ./cmake-build-debug --target all
su -c "cp cmake-build-debug/pcbu_auth /usr/sbin/pcbu_auth && chmod +x /usr/sbin/pcbu_auth && chmod u+s /usr/sbin/pcbu_auth"
