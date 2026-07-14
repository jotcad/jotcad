#!/bin/bash
set -e

if [ ! -d boost/boost ]; then
  echo "Downloading and extracting Boost 1.83.0 headers from SourceForge..."
  mkdir -p boost
  curl -L "https://downloads.sourceforge.net/project/boost/boost/1.83.0/boost_1_83_0.tar.gz" | tar -xz --strip-components=1 -C boost boost_1_83_0/boost
else
  echo "Boost headers already present."
fi
