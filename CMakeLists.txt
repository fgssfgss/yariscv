cmake_minimum_required(VERSION 3.19)
project(yariscvemu C)

set(CMAKE_C_STANDARD 11)

add_executable(yariscvemu main.c src/rv32.c src/rv32.h src/mmu.c src/mmu.h src/memmap.c src/memmap.h src/rv32_p.h src/loader.c src/loader.h src/uart.c src/uart.h src/plic.c src/plic.h src/clint.c src/clint.h src/logger.c src/logger.h)