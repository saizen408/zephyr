.. _te0950_r5:

Trenz TE0950 Development Board RPU Cortex-R5
############################################

Overview
********
This configuration provides support for the RPU, real-time processing unit on Trenz
TE0950 development board, it can operate as following:

* Two independent R5 cores with their own TCMs (tightly coupled memories)
* Or as a single dual lock step unit with double the TCM size.

This processing unit is based on an ARM Cortex-R5 CPU, it also enables the following devices:

* ARM PL-390 Generic Interrupt Controller
* Xilinx Zynq TTC (Cadence TTC)
* Xilinx Zynq UART

Hardware
********
Supported Features
==================

The following hardware features are supported:

+--------------+------------+----------------------+
| Interface    | Controller | Driver/Component     |
+==============+============+======================+
| GIC          | on-chip    | generic interrupt    |
|              |            | controller           |
+--------------+------------+----------------------+
| TTC          | on-chip    | system timer         |
+--------------+------------+----------------------+
| UART         | on-chip    | serial port          |
+--------------+------------+----------------------+

The kernel currently does not support other hardware features on this platform.

Devices
========
System Timer
------------

This board configuration uses a system timer tick frequency of 1000 Hz.

Serial Port
-----------

This board configuration uses a single serial communication channel with the
on-chip UART1.

Memories
--------

Although Flash, DDR and OCM memory regions are defined in the DTS file,
all the code plus data of the application will be loaded in the sram0 region,
which points to the DDR memory. The ocm0 memory area is currently available
for usage, although nothing is placed there by default.

References
**********

1. ARMv7-A and ARMv7-R Architecture Reference Manual (ARM DDI 0406C ID051414)
2. Cortex-R5 and Cortex-R5F Technical Reference Manual (ARM DDI 0460C ID021511)
3. Zynq UltraScale+ Device Technical Reference Manual (UG1085)
4. Kria vck190 Vision AI Starter Kit User Guide (UG1089)
5. Trenz TE0950 TRM (https://wiki.trenz-electronic.de/display/PD/TE0950+TRM)
