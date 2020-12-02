set -x
mkdir -p ../test

$PDPPATH/pdp11-aout-gcc -nostdlib -Ttext 0x0 -m45 -Os -N -e _start test.s -o ../test/test.out
$PDPPATH/pdp11-aout-objcopy -O binary ../test/test.out ../test/test.bin
