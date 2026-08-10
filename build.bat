@echo off
setlocal enabledelayedexpansion

echo Building MICF Kernel v0.6.0

if exist build (
    rmdir /s /q build
    if errorlevel 1 (
        echo Failed to delete build directory. Close any program using it and retry.
        pause
        exit /b 1
    )
)
mkdir build

echo Assembling boot/boot.s
i686-elf-as boot/boot.s -o build/boot.o
if errorlevel 1 goto error

set C_FILES=kernel\main.c kernel\arch\gdt.c kernel\arch\idt.c kernel\arch\paging.c kernel\mm\pmm.c kernel\task\process.c kernel\ipc\ipc.c kernel\drivers\keyboard.c kernel\drivers\ata.c kernel\fs\fat32.c kernel\lib\print.c kernel\lib\string.c

for %%f in (%C_FILES%) do (
    echo Compiling %%f
    i686-elf-gcc -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Iinclude -Ikernel -c %%f -o build\%%~nf.o
    if errorlevel 1 goto error
)

echo Linking...
i686-elf-gcc -T linker.ld -ffreestanding -O2 -nostdlib -lgcc -o micf.elf build\boot.o build\main.o build\gdt.o build\idt.o build\paging.o build\pmm.o build\process.o build\ipc.o build\keyboard.o build\ata.o build\fat32.o build\print.o build\string.o
if errorlevel 1 goto error

echo Cleaning object files...
del /q build\*.o 2>nul
rmdir build 2>nul

echo Build successful! Kernel image: micf.elf
echo You can now run with: qemu-system-i386 -kernel micf.elf -hda disk.img
goto end

:error
echo Build failed.
pause
exit /b 1

:end
pause