#!/bin/bash
# LCD object-lifetime stress test (host). Needs gcc/g++ with AddressSanitizer and a configured
# firmware build (for managed_components/lvgl__lvgl and build/config/sdkconfig.h).
# usage: test/lcd-lifetime/run.sh   (from the repository root)
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd); HERE=$ROOT/test/lcd-lifetime; W=${TMPDIR:-/tmp}/costean-lcd-lifetime
rm -rf $W && mkdir -p $W/displays && cp -r $HERE/inc $HERE/boards $HERE/*.cpp $HERE/*.h $W/
mkdir -p $W/otp && cp $ROOT/main/otp/qrcodegen.* $W/otp/
for f in ui.cpp ui.h ui_helpers.cpp ui_helpers.h ui_octa.cpp octa_data.h octa_logic.h ui_select.h; do cp $ROOT/main/displays/$f $W/displays/; done
cp -r $ROOT/main/displays/images $W/displays/
L=$ROOT/managed_components/lvgl__lvgl; CFG=$ROOT/build/config; cd $W
INC="-I. -Iinc -Idisplays -I$L -I$L/src -include $CFG/sdkconfig.h -DLV_CONF_SKIP -DLV_CONF_KCONFIG_EXTERNAL_INCLUDE=\"sdkconfig.h\" -I$CFG -DLV_LVGL_H_INCLUDE_SIMPLE"
for V in asan device; do
  if [ $V = asan ]; then F="-g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined -DLV_MEM_CUSTOM=1"; else F="-g -O1 -DLV_MEM_CUSTOM=0 -DLV_MEM_ADD_JUNK=1"; fi
  mkdir -p o$V; export F INC V
  find $L/src -name '*.c' | xargs -P16 -I{} bash -c 'gcc $F -w -c $INC "{}" -o o$V/lv_$(echo "{}" | md5sum | cut -c1-12).o'
  for f in displays/images/*.c displays/images/themes/themes.c; do gcc $F -w -c $INC "$f" -o o$V/a_$(basename $f .c).o; done
  for f in displays/ui.cpp displays/ui_octa.cpp displays/ui_helpers.cpp octa_data_sim.cpp lifetime_test.cpp; do g++ -std=c++17 $F -w -c $INC "$f" -o o$V/c_$(basename $f .cpp).o; done
  g++ $F o$V/*.o -o lt-$V
  for s in portal mining midanim overlays shutdown; do ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 ./lt-$V $s | tail -1 | sed "s/^/[$V] /"; done
done
