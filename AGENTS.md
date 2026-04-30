# AGENTS.md

This file provides guidance to agents when working with code in this repository.

## Project Overview

**ApBDC** is an STM32F4-based embedded systems project configured for the **STM32F413xx** microcontroller. This is a bare-metal firmware project using the STM32 HAL (Hardware Abstraction Layer) and CMSIS libraries, built with the GNU ARM Embedded Toolchain in Eclipse.

### Target Hardware
- **Microcontroller**: STM32F413xx (ARM Cortex-M4 with FPU)
- **Flash Memory**: 2048 KB (0x08000000 - 0x081FFFFF)
- **RAM**: 320 KB (0x20000000 - 0x2004FFFF)
- **External Clock**: 8 MHz HSE (configurable via HSE_VALUE)
- **FPU**: Hardware floating-point unit (FPv4-SP-D16)

### Technology Stack
- **Architecture**: ARM Cortex-M4F (Thumb instruction set)
- **HAL Version**: STM32F4xx HAL Driver
- **CMSIS**: Version 5.x
- **Toolchain**: xPack GNU Arm Embedded GCC (arm-none-eabi-gcc)
- **Build System**: Eclipse CDT Managed Build with GNU Make
- **Debug Interface**: Semihosting (trace output via DEBUG)
- **Standard Library**: newlib-nano

## Project Structure

```
ApBDC/
├── src/                          # Application source code
│   ├── main.cpp                  # Main application entry point (currently empty template)
│   ├── initialize-hardware.c     # Hardware initialization (HAL, clocks)
│   ├── stm32f4xx_hal_msp.c      # HAL MSP (MCU Support Package) callbacks
│   └── write.c                   # Low-level write implementation
├── include/                      # Application headers
│   ├── stm32f4xx_hal_conf.h     # HAL configuration (module selection)
│   └── stm32_assert.h           # Assert configuration
├── system/                       # System-level code (CMSIS, HAL, startup)
│   ├── include/
│   │   ├── cmsis/               # CMSIS headers (core, device-specific)
│   │   ├── stm32f4-hal/         # STM32F4 HAL headers
│   │   ├── cortexm/             # Cortex-M exception handlers
│   │   └── diag/                # Diagnostic/trace support
│   └── src/
│       ├── cmsis/               # CMSIS system and vector table
│       ├── stm32f4-hal/         # STM32F4 HAL implementation
│       ├── cortexm/             # Cortex-M startup and exception handling
│       ├── diag/                # Trace implementation
│       └── newlib/              # Newlib syscalls and startup
├── ldscripts/                    # Linker scripts
│   ├── mem.ld                   # Memory layout definition
│   ├── libs.ld                  # Library search paths
│   └── sections.ld              # Section placement
├── .cproject                     # Eclipse CDT project configuration
└── .project                      # Eclipse project metadata
```

## Building and Running

### Build Configurations

The project has two build configurations:

1. **Debug** (default)
   - Optimization: `-Og` (debug-friendly)
   - Debug symbols: Maximum (`-g3`)
   - Defines: `DEBUG`, `USE_FULL_ASSERT`, `TRACE`, `OS_USE_TRACE_SEMIHOSTING_DEBUG`
   - Trace output: Enabled via semihosting

2. **Release**
   - Optimization: `-Os` (size optimization)
   - Debug symbols: None
   - Defines: `NDEBUG`
   - Trace output: Disabled

### Build Commands

**In Eclipse:**
- Build: `Project → Build Project` or `Ctrl+B`
- Clean: `Project → Clean...`
- Build configuration: `Project → Build Configurations → Set Active`


### Build Artifacts

After building, the following files are generated:
- `ApBDC.elf` - ELF executable with debug symbols
- `ApBDC.bin` - Raw binary for flashing
- `ApBDC.hex` - Intel HEX format
- `ApBDC.list` - Disassembly listing
- `ApBDC.map` - Memory map

### Flashing and Debugging

The project is configured for debugging via:
- **Semihosting**: Trace output redirected to debugger console
- **Debug Probe**: Compatible with ST-LINK, J-Link, or other ARM debug probes
- **GDB**: Use `arm-none-eabi-gdb` with appropriate debug server

**Note**: The current `main.cpp` contains only an empty infinite loop. Hardware initialization is performed in `initialize-hardware.c` before `main()` is called.

## Development Conventions

### Code Organization

1. **Application Code** (`src/`, `include/`)
   - Place your application logic in `src/main.cpp`
   - Add custom headers to `include/`
   - Keep hardware-specific code separate from application logic

2. **System Code** (`system/`)
   - **Do not modify** unless you need to customize low-level behavior
   - HAL drivers are pre-compiled and included
   - CMSIS and startup code are provided by STMicroelectronics

3. **HAL Configuration** (`include/stm32f4xx_hal_conf.h`)
   - Enable/disable HAL modules as needed
   - All modules are currently enabled; disable unused ones to reduce code size
   - Adjust clock settings (HSE_VALUE, PLL parameters) for your board

### Compiler Settings

- **Language**: C11 for `.c` files, C++11 for `.cpp` files
- **Warnings**: `-Wall -Wextra` enabled
- **Floating Point**: Hardware FPU with hard ABI (`-mfloat-abi=hard -mfpu=fpv4-sp-d16`)
- **Optimization**: Function and data sections enabled (`-ffunction-sections -fdata-sections`)
- **Linker**: Unused sections removed (`--gc-sections`)
- **C++ Features**: Exceptions and RTTI disabled (`-fno-exceptions -fno-rtti`)

### Memory Configuration

The linker script (`ldscripts/mem.ld`) defines:
- **FLASH**: 2048 KB at 0x08000000 (code and constants)
- **RAM**: 320 KB at 0x20000000 (data, bss, heap, stack)
- **CCMRAM**: Not used (0 length) - can be enabled if needed

To modify memory layout:
1. Edit `ldscripts/mem.ld` to match your hardware
2. Adjust FLASH and RAM ORIGIN/LENGTH as needed
3. Rebuild the project

### Clock Configuration

The system clock is configured in `src/initialize-hardware.c`:
- **Default**: HSE (8 MHz) → PLL → System Clock
- **PLL Configuration**: Adjust `PLLM`, `PLLN`, `PLLP`, `PLLQ` for your requirements
- **Warning**: The default configuration may not provide 48 MHz for USB

**To customize**:
1. Modify `SystemClock_Config()` in `initialize-hardware.c`
2. Update `HSE_VALUE` in compiler defines if using different crystal
3. Verify clock tree matches your board's hardware

### Debugging and Tracing

The project uses the µOS++ trace infrastructure:
- **Trace Macros**: `trace_printf()`, `trace_puts()`, etc.
- **Output**: Semihosting to debugger console (Debug build only)
- **Configuration**: `OS_USE_TRACE_SEMIHOSTING_DEBUG` in Debug build

**To add trace output**:
```c
#include "diag/trace.h"

trace_printf("Hello from STM32! Counter: %d\n", counter);
```

### Assertions

When `USE_FULL_ASSERT` is defined (Debug build):
- HAL parameter checks are enabled
- Failed assertions call `assert_failed()` in `initialize-hardware.c`
- Default behavior: Print error and enter infinite loop

### Adding New Source Files

1. Place `.c`/`.cpp` files in `src/` or subdirectories
2. Place `.h` files in `include/` or subdirectories
3. Eclipse will automatically detect and compile them
4. For manual Makefile: Add to source list in build configuration

### HAL Module Usage

To use a HAL peripheral (e.g., GPIO):
1. Ensure module is enabled in `stm32f4xx_hal_conf.h`
2. Include the header: `#include "stm32f4xx_hal_gpio.h"`
3. Initialize in `HAL_MspInit()` or custom init function
4. Implement MSP callbacks in `stm32f4xx_hal_msp.c` if needed

### Best Practices

- **Startup**: Hardware initialization happens in `__initialize_hardware()` before `main()`
- **Interrupts**: Implement handlers in `system/src/cortexm/exception-handlers.c` or your own files
- **Peripheral Init**: Use HAL initialization functions (e.g., `HAL_GPIO_Init()`)
- **Error Handling**: Check HAL function return values (`HAL_OK`, `HAL_ERROR`, etc.)
- **Power Management**: Configure voltage scaling and clock gating as needed
- **Code Size**: Disable unused HAL modules to reduce flash usage

## Common Tasks

### Blinking an LED
```c
// In main.cpp
#include "stm32f4xx_hal.h"

int main(void) {
    // Enable GPIO clock
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    // Configure PA5 as output
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    while (1) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(500);
    }
}
```

### Using UART for Communication
1. Enable `HAL_UART_MODULE_ENABLED` in `stm32f4xx_hal_conf.h`
2. Configure UART in `stm32f4xx_hal_msp.c` (GPIO, clock, interrupts)
3. Initialize UART handle and use `HAL_UART_Transmit()` / `HAL_UART_Receive()`

### Handling Interrupts
1. Define handler in your code or `exception-handlers.c`
2. Enable interrupt in NVIC using `HAL_NVIC_EnableIRQ()`
3. Implement callback functions (e.g., `HAL_GPIO_EXTI_Callback()`)

## Troubleshooting

### Build Errors
- **Missing headers**: Check include paths in project settings
- **Undefined references**: Ensure all required HAL modules are enabled and compiled
- **Linker errors**: Verify memory layout in `mem.ld` matches your device

### Runtime Issues
- **Hard Fault**: Check stack size, memory corruption, or invalid peripheral access
- **Clock not working**: Verify HSE_VALUE matches your crystal frequency
- **No trace output**: Ensure semihosting is enabled in debugger configuration

### Flash/RAM Overflow
- Disable unused HAL modules in `stm32f4xx_hal_conf.h`
- Use Release build with size optimization
- Review `.map` file to identify large consumers

## Additional Resources

- **STM32F413 Reference Manual**: Detailed peripheral descriptions
- **STM32F4 HAL User Manual**: HAL API documentation
- **CMSIS Documentation**: ARM CMSIS standard
- **µOS++ Project**: https://github.com/micro-os-plus

## Notes for AI Agents

- This is a **bare-metal embedded project** - no operating system
- **Memory is constrained** - be mindful of code size and RAM usage
- **Real-time considerations** - avoid blocking operations in interrupt handlers
- **Hardware-specific** - code must match the STM32F413xx capabilities
- **Build in Eclipse** - project uses Eclipse CDT managed build system
- **Current state**: Template project with empty `main()` - ready for application development
