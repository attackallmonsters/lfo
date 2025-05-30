#!/bin/bash

# creates the Purte Data extension based on the main branch on github
# script has to be executable

set -e

# remounting root filesystem read/write...
sudo mount -o remount,rw /

# removing old lfo directory
rm -r -f lfo

# cloning latest ldo from GitHub...
GIT_SSL_NO_VERIFY=true git clone https://github.com/attackallmonsters/lfo.git

# building release version
cd lfo || { echo "directory lfo not found"; exit 1; }
make release

echo "build complete"
