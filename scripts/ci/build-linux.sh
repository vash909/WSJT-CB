#!/usr/bin/env bash
set -euo pipefail

target=${1:?target required}
export DEBIAN_FRONTEND=noninteractive
if command -v apt-get >/dev/null; then
  family=deb
  apt-get update
  apt-get install -y --no-install-recommends ca-certificates curl git build-essential \
    cmake ninja-build gfortran pkg-config autoconf automake libtool \
    libudev-dev libusb-1.0-0-dev libboost-log-dev libboost-thread-dev libfftw3-dev \
    qtbase5-dev qtmultimedia5-dev qttools5-dev qttools5-dev-tools \
    libqt5serialport5-dev libqt5websockets5-dev python3 dpkg-dev xz-utils file
else
  family=rpm
  dnf install -y ca-certificates curl-minimal git gcc gcc-c++ gcc-gfortran cmake ninja-build \
    make pkgconf-pkg-config autoconf automake libtool systemd-devel libusbx-devel \
    boost-devel fftw-devel qt5-qtbase-devel qt5-qtmultimedia-devel \
    qt5-qtserialport-devel qt5-qttools-devel qt5-qtwebsockets-devel \
    python3 rpm-build redhat-rpm-config file findutils which tar gzip xz
fi

git config --global --add safe.directory "$PWD"
build="$PWD/build-user/ci-$target"
stage="$build/stage"
out="$PWD/dist/$target"
mkdir -p "$build" "$out"
bash scripts/ci/build-hamlib.sh /opt/wsjtcb-hamlib static "$build/hamlib"
export PKG_CONFIG_PATH=/opt/wsjtcb-hamlib/lib/pkgconfig
cmake -S . -B "$build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/lib/wsjtcb \
  -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_INSTALL_DOCDIR=share/doc/wsjtcb \
  -DCMAKE_PREFIX_PATH=/opt/wsjtcb-hamlib -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=OFF \
  -DWSJT_SKIP_MANPAGES=ON -DWSJT_GENERATE_DOCS=OFF \
  -DWSJT_BUILD_DECODER_TESTS=ON -DWSJT_BUILD_PATH_TESTS=ON
cmake --build "$build" --parallel "${BUILD_JOBS:-4}"
ctest --test-dir "$build" --output-on-failure | tee "$out/regression-tests.txt"
DESTDIR="$stage" cmake --install "$build"

# Keep helper names such as jt9/wsprd private, so WSJT-X can coexist.
install -d "$stage/usr/bin" "$stage/usr/share/applications" "$stage/usr/share/pixmaps"
printf '#!/bin/sh\nexec /usr/lib/wsjtcb/bin/wsjtcb "$@"\n' > "$stage/usr/bin/wsjtcb"
chmod 755 "$stage/usr/bin/wsjtcb"
mv "$stage/usr/lib/wsjtcb/share/applications/wsjtcb.desktop" "$stage/usr/share/applications/"
mv "$stage/usr/lib/wsjtcb/share/pixmaps/wsjtcb_icon.png" "$stage/usr/share/pixmaps/"
rmdir "$stage/usr/lib/wsjtcb/share/applications" "$stage/usr/lib/wsjtcb/share/pixmaps"
version=$(sed -nE 's/^[[:space:]]*VERSION ([0-9]+\.[0-9]+\.[0-9]+)\.[0-9]+.*/\1/p' CMakeLists.txt)
[[ $version == 1.4.1 ]]

elfs=()
while IFS= read -r -d '' file; do
  if file "$file" | grep -q ELF; then
    ldd "$file" > "$build/ldd-last.txt"
    if grep -q 'not found\|libhamlib' "$build/ldd-last.txt"; then
      cat "$build/ldd-last.txt"
      exit 1
    fi
    elfs+=("-e$file")
  fi
done < <(find "$stage/usr/lib/wsjtcb/bin" -type f -executable -print0)

if [[ $family == deb ]]; then
  mkdir -p "$build/debian" "$stage/DEBIAN"
  printf 'Source: wsjtcb\nSection: hamradio\nPriority: optional\nMaintainer: vash909 <iu1bot@xzgroup.net>\nStandards-Version: 4.6.0\n\nPackage: wsjtcb\nArchitecture: any\nDescription: WSJT-CB\n' > "$build/debian/control"
  deps=$(cd "$build" && dpkg-shlibdeps -O "${elfs[@]}" | sed -n 's/^shlibs:Depends=//p')
  [[ -n $deps ]]
  cat > "$stage/DEBIAN/control" <<EOF
Package: wsjtcb
Version: $version
Architecture: amd64
Section: hamradio
Priority: optional
Maintainer: vash909 <iu1bot@xzgroup.net>
Homepage: https://github.com/vash909/WSJT-CB
Depends: $deps, libqt5multimedia5-plugins, libqt5sql5-sqlite
Description: WSJT-CB digital modes for 27 MHz
 WSJT-CB $version Scarlet, built and tested for $target.
EOF
  dpkg-deb --root-owner-group -Zxz --build "$stage" "$out/wsjtcb_${version}_${target}_amd64.deb"
  cp "$stage/DEBIAN/control" "$out/package-metadata.txt"
else
  rpmroot="$build/rpmbuild"
  mkdir -p "$rpmroot"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
  cat > "$rpmroot/SPECS/wsjtcb.spec" <<EOF
%global debug_package %{nil}
%global _build_id_links none
Name: wsjtcb
Version: $version
Release: 1%{?dist}
Summary: WSJT-CB digital modes for 27 MHz
License: GPL-3.0-only
URL: https://github.com/vash909/WSJT-CB
Requires: qt5-qtmultimedia, qt5-qtbase
%description
WSJT-CB $version Scarlet, built and tested for $target.
%install
mkdir -p %{buildroot}
cp -a "$stage"/. %{buildroot}/
%files
/usr/bin/wsjtcb
/usr/lib/wsjtcb
/usr/share/applications/wsjtcb.desktop
/usr/share/pixmaps/wsjtcb_icon.png
EOF
  rpmbuild --define "_topdir $rpmroot" -bb "$rpmroot/SPECS/wsjtcb.spec"
  cp "$rpmroot"/RPMS/x86_64/*.rpm "$out/wsjtcb-${version}-${target}.x86_64.rpm"
  rpm -qp --requires "$out"/*.rpm > "$out/package-metadata.txt"
fi
git rev-parse HEAD > "$out/SOURCE_COMMIT.txt"
(cd "$out" && sha256sum ./*."$family" > SHA256SUMS.txt)
