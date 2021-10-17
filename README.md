# yariscv

yariscv is an Yet Another RISC-V emulator

### Features

Implements rv32imac architecture with sv32 mmu... and it has a very dumb TLB cache implementation
Implements clint, plic is a stub
Implements litex-like UART(seems like it's working)
Uses stdin and stdout as input for integrated UART

### How to build it

    mkdir build && cd build
    cmake .. 
    make
    
### How to run it
    
**Run**    

    ./yariscv 

**Command line arguments**

    [-f|--firmare] - pass firmware to the emulator(opensbi is preferred)
    [-i|--initrd] - pass linux initrd image
    [-d|--dtb] - pass dtb file

### TODO

1. Fix timer issues
2. Fix issues with UART and console
3. Implement proper TLB cache 
4. Improve speed
5. Implement f and d RISC-V extensions 

