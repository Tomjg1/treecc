gcc -nostdinc -nodefaultlibs -nostartfiles -nostdlib -no-pie -fno-pie start.c &&
gcc -nostdinc -nodefaultlibs -nostartfiles -nostdlib -no-pie -fno-pie -c undef.c &&
gcc -nostdinc -nodefaultlibs -nostartfiles -nostdlib -no-pie -fno-pie -c def.c &&
gcc -nostdinc -nodefaultlibs -nostartfiles -nostdlib -no-pie -fno-pie -r undef.o def.o -o def-undef.o &&
readelf def.o --all > elf_def.txt &&
readelf undef.o --all > elf_undef.txt &&
readelf def-undef.o --all > elf_def-undef.txt

# objcopy -O binary --only-section=.text a.out text.bin
# objcopy -O binary --only-section=.data a.out data.bin
# objcopy -O binary --only-section=.rodata a.out rodata.bin
