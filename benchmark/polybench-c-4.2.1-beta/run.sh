DIRNAME=$(dirname $0)
SABLE_WASM=$DIRNAME/../../build/sable-wasm
LOADER=$DIRNAME/../../build/tester

$SABLE_WASM --unsafe --opt "$1" -o "$1.o"
ld -shared --eh-frame-hdr "$1.o" -o "$1.sable"
$LOADER "$1.sable"

rm -f "$1.o" "$1.sable"