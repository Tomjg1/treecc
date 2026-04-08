musl-gcc  -no-pie -fno-pie -static -nostdlib -c undef.c &&
musl-gcc  -no-pie -fno-pie -static -nostdlib -c def.c &&
musl-gcc  -no-pie -fno-pie -static -nostdlib -r undef.o def.o -o def-undef.o &&
musl-gcc  -no-pie -fno-pie -static  -nostdlib undef.o def.o -o gcc.out &&
readelf def.o --all > elf_def.txt &&
readelf undef.o --all > elf_undef.txt &&
readelf def-undef.o --all > elf_def-undef.txt

# objcopy -O binary --only-section=.text a.out text.bin
# objcopy -O binary --only-section=.data a.out data.bin
# objcopy -O binary --only-section=.rodata a.out rodata.bin
