$ErrorActionPreference = "Stop"

$ProjectPath = $PSScriptRoot
$WslPath = "/mnt/e/Projects/FuxOS"

Write-Host "=== Build FuxOS ==="

# ============================================================
# Prepare build directories
# ============================================================

New-Item -ItemType Directory -Force -Path "$ProjectPath\build" | Out-Null
New-Item -ItemType Directory -Force -Path "$ProjectPath\build\obj" | Out-Null

# ============================================================
# Assemble bootloader
# ============================================================

Write-Host "[ASM] boot/boot.asm"

wsl bash -lc "cd '$WslPath' && nasm -f bin boot/boot.asm -o build/boot.bin"

Write-Host "[ASM] boot/stage2.asm"

wsl bash -lc "cd '$WslPath' && nasm -f bin boot/stage2.asm -o build/stage2.bin"

# ============================================================
# Compile kernel/kernel.c
# ============================================================

$CFlags = "-m64 -ffreestanding -fno-pic -fno-stack-protector -mno-red-zone -mgeneral-regs-only -O2 -Ikernel/include -fPIE"

$KernelSource = "$ProjectPath\kernel\kernel.c"
$KernelObject = "build/obj/kernel.o"

Write-Host "[CC] kernel/kernel.c"

wsl bash -lc "cd '$WslPath' && gcc $CFlags -lc kernel/kernel.c -o $KernelObject"

# ============================================================
# Compile kernel/src/*.c
# ============================================================

$SourceFiles = Get-ChildItem "$ProjectPath\kernel\src\*.c"

foreach ($File in $SourceFiles) {

    $ObjectName = [System.IO.Path]::GetFileNameWithoutExtension($File.Name) + ".o"
    $ObjectPath = "build/obj/$ObjectName"

    Write-Host "[CC] kernel/src/$($File.Name)"

    wsl bash -lc "cd '$WslPath' && gcc $CFlags -lc 'kernel/src/$($File.Name)' -o '$ObjectPath'"
}

# ============================================================
# Link kernel
# ============================================================

$Objects = Get-ChildItem "$ProjectPath\build\obj\*.o" |
        ForEach-Object {
            "build/obj/$($_.Name)"
        }

$ObjectArgs = $Objects -join " "

Write-Host "[LD] kernel.elf"

wsl bash -lc "cd '$WslPath' && ld -m elf_x86_64 -T kernel/linker.ld -nostdlib -o build/kernel.elf $ObjectArgs"

# ============================================================
# Convert ELF -> binary
# ============================================================

Write-Host "[OBJCOPY] kernel.bin"

wsl bash -lc "cd '$WslPath' && objcopy -O binary build/kernel.elf build/kernel.bin"

# ============================================================
# Create floppy image
# ============================================================

Write-Host "[IMG] Creating floppy image"

wsl bash -lc "cd '$WslPath' && dd if=/dev/zero of=build/fuxos.img bs=512 count=2880 status=none"

# Boot sector
wsl bash -lc "cd '$WslPath' && dd if=build/boot.bin of=build/fuxos.img conv=notrunc status=none"

# Stage 2
wsl bash -lc "cd '$WslPath' && dd if=build/stage2.bin of=build/fuxos.img bs=512 seek=1 conv=notrunc status=none"

# Kernel
wsl bash -lc "cd '$WslPath' && dd if=build/kernel.bin of=build/fuxos.img bs=512 seek=17 conv=notrunc status=none"

# ============================================================
# Run QEMU
# ============================================================

Write-Host "=== Starting QEMU ==="

wsl bash -lc "cd '$WslPath' && qemu-system-x86_64 -drive format=raw,file=build/fuxos.img,if=floppy -boot a -no-reboot -no-shutdown"