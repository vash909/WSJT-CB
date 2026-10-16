#!/usr/bin/env bash
set -euo pipefail

prefix=${1:?installation prefix required}
linkage=${2:-static}
work=${3:-build-user/hamlib-ci}
mkdir -p "$work"
curl --fail --location --retry 3 \
  https://github.com/Hamlib/Hamlib/releases/download/4.6.5/hamlib-4.6.5.tar.gz \
  --output "$work/hamlib.tar.gz"
printf '%s  %s\n' 90d6f1dba59417c00f8f4545131c7efd31930cd0e178598980a8210425e3852e "$work/hamlib.tar.gz" | sha256sum --check
tar -xzf "$work/hamlib.tar.gz" -C "$work"
cd "$work/hamlib-4.6.5"
if [[ $linkage == shared ]]; then
  flags=(--enable-shared --disable-static)
else
  flags=(--disable-shared --enable-static)
fi
./configure --prefix="$prefix" "${flags[@]}" --without-readline --disable-cxx-binding
make -j"${BUILD_JOBS:-4}"
make install
