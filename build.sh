#!/bin/bash
set -e

cd "$(dirname "$0")"

echo "=== Build FuxOS ==="

nasm -f bin boot/boot.asm   -o build/boot.bin
nasm -f bin boot/stage2.asm -o build/stage2.bin

mkdir -p build/obj

CFLAGS="-m64 -ffreestanding -fno-pic -fno-stack-protector -mno-red-zone -mgeneral-regs-only -O2 -Ikernel/include"

OBJS=""
for f in kernel/*.c; do
    obj="build/obj/$(basename "${f%.c}").o"
    x86_64-elf-gcc $CFLAGS -c "$f" -o "$obj"
    OBJS="$OBJS $obj"
done

x86_64-elf-ld -m elf_x86_64 -T kernel/linker.ld -nostdlib -o build/kernel.elf $OBJS
x86_64-elf-objcopy -O binary build/kernel.elf build/kernel.bin

dd if=/dev/zero        of=build/fuxos.img bs=512 count=2880
dd if=build/boot.bin   of=build/fuxos.img conv=notrunc
dd if=build/stage2.bin of=build/fuxos.img bs=512 seek=1 conv=notrunc
dd if=build/kernel.bin of=build/fuxos.img bs=512 seek=17 conv=notrunc

echo "=== Starting QEMU ==="

qemu-system-x86_64 \
    -drive format=raw,file=build/fuxos.img,if=floppy \
    -boot a \
    -no-reboot \
    -no-shutdown