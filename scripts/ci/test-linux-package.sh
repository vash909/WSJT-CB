#!/usr/bin/env bash
set -euo pipefail
target=${1:?target required}
out="/src/dist/$target"
export DEBIAN_FRONTEND=noninteractive
if command -v apt-get >/dev/null; then
  apt-get update
  apt-get install -y --no-install-recommends "$out"/*.deb xvfb xauth x11-utils \
    procps python3 util-linux passwd ca-certificates
  apt-get check
  test -z "$(dpkg --audit)"
  dpkg-query -W -f='${Version}\n' wsjtcb | grep -x 1.4.1
else
  dnf install -y "$out"/*.rpm xorg-x11-server-Xvfb xorg-x11-xauth xorg-x11-utils \
    procps-ng python3 util-linux shadow-utils
  dnf check
  rpm -V wsjtcb
  rpm -q --qf '%{VERSION}\n' wsjtcb | grep -x 1.4.1
fi
while IFS= read -r -d '' executable; do
  if head -c 4 "$executable" | grep -q ELF; then
    ldd "$executable" | tee -a "$out/installed-libraries.txt"
  fi
done < <(find /usr/lib/wsjtcb/bin -type f -executable -print0)
if grep -q 'not found\|libhamlib' "$out/installed-libraries.txt"; then exit 1; fi
test -f /usr/lib/wsjtcb/share/wsjtcb/cty.dat
test -f /usr/lib/wsjtcb/share/wsjtcb/JPLEPH
useradd --create-home wsjtcb-test
install -d -o wsjtcb-test -g wsjtcb-test /tmp/wsjtcb-test
runuser -u wsjtcb-test -- env QT_QPA_PLATFORM=xcb \
  xvfb-run -a python3 /src/scripts/ci/smoke-linux.py | tee "$out/installation-test.txt"
cp /tmp/wsjtcb-test/application.log "$out/application.log"
