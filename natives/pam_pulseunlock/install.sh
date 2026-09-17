#!/bin/bash
cd cmake-build-debug || exit
cmake ..
cd ..
cmake --build ./cmake-build-debug --target all
su -c "cp cmake-build-debug/pam_pulseunlock.so /lib/security/pam_pulseunlock.so"
