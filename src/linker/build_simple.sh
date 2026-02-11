gcc -nostdinc -nodefaultlibs -nostartfiles -nostdlib -no-pie -fno-pie start.c
objcopy -O binary --only-section=.text a.out text.bin
objcopy -O binary --only-section=.data a.out data.bin
objcopy -O binary --only-section=.rodata a.out rodata.bin
