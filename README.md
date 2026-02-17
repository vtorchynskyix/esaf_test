# debugging

Find the serial number of the target device:
```
$ sudo st-info --probe
2024-07-21T14:32:11 WARN usb.c: skipping ST device : 0x483:0xdf11)
Found 1 stlink programmers
  version:    V2J45S7
  serial:     12002900010000524A51544E
  flash:      32768 (pagesize: 2048)
  sram:       8192
  chipid:     0x466
  dev-type:   STM32G03x_G04x
```

then, leave the `st-util` running, pointed at that device:

```
$ sudo st-util --serial 12002900010000524A51544E
use serial 12002900010000524A51544E
st-util 1.8.0
2024-07-21T14:31:19 INFO common.c: NRST is not connected --> using software reset via AIRCR
2024-07-21T14:31:19 INFO common.c: STM32G03x_G04x: 8 KiB SRAM, 32 KiB flash in at least 2 KiB pages.
2024-07-21T14:31:19 INFO gdb-server.c: Listening at *:4242...
```

in another shell, from git root dir, connect with gdb (`.gdbinit` is in git)

```
$ arm-none-eabi-gdb -ix .gdbinit
GNU gdb (Arm GNU Toolchain 12.3.Rel1 (Build arm-12.35)) 13.2.90.20230627-git
Copyright (C) 2023 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.
Type "show copying" and "show warranty" for details.
This GDB was configured as "--host=x86_64-pc-linux-gnu --target=arm-none-eabi".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<https://bugs.linaro.org/>.
Find the GDB manual and other documentation resources online at:
    <http://www.gnu.org/software/gdb/documentation/>.

For help, type "help".
Type "apropos word" to search for commands related to "word".
"LOCAL GDBINIT"0x080026d8 in HAL_Delay (Delay=Delay@entry=50) at Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal.c:395
395       while ((HAL_GetTick() - tickstart) < wait)
Loading section .isr_vector, size 0xb8 lma 0x8000000
Loading section .text, size 0x46bc lma 0x80000b8
Loading section .rodata, size 0x78 lma 0x8004774
Loading section .init_array, size 0x4 lma 0x80047ec
...
Start address 0x08004690, load size 18440
Transfer rate: 942 bytes/sec, 2634 bytes/write.
```

then to reflash, break with `^C` and use the scripted command `bang`
(see `.gdbinit` for details)

```
^C
Program received signal SIGTRAP, Trace/breakpoint trap.
HAL_GPIO_WritePin (GPIOx=GPIOx@entry=0x50000000, GPIO_Pin=GPIO_Pin@entry=32, PinState=GPIO_PIN_SET)
    at Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_gpio.c:412
412         GPIOx->BSRR = (uint32_t)GPIO_Pin;
warning: File "/home/troy/proj/ookashaka/zhalo/.gdbinit" auto-loading has been declined by your `auto-load safe-path' set to "$debugdir:$datadir/auto-load:/home/troy/proj/cppkit/.gdbinit:/home/troy/proj/initiators/zhalo/.gdbinit".
To enable execution of this file add
        add-auto-load-safe-path /home/troy/proj/ookashaka/zhalo/.gdbinit
line to your configuration file "/home/troy/.config/gdb/gdbinit".
To completely disable this security protection add
        set auto-load safe-path /
line to your configuration file "/home/troy/.config/gdb/gdbinit".
For more information about this security protection see the
"Auto-loading safe path" section in the GDB manual.  E.g., run from the shell:
        info "(gdb)Auto-loading safe path"
(gdb) bang
Loading section .isr_vector, size 0xb8 lma 0x8000000
Loading section .text, size 0x46bc lma 0x80000b8
Loading section .rodata, size 0x78 lma 0x8004774
Loading section .init_array, size 0x4 lma 0x80047ec
Loading section .fini_array, size 0x4 lma 0x80047f0
Loading section .data, size 0x14 lma 0x80047f4
```
