.. zephyr:code-sample:: sys_control
   :name: System Control

   Monitor system statistics of the versal (temperature, voltage, etc.)

Overview
********

A system controller that can be used with the te0950_mbv32 board that
monitores systems statistics of the versal and also controls the fan speed

Building and Running
********************

This application can be built and executed on QEMU as follows:

.. zephyr-app-commands::
   :zephyr-app: kondor-os/sys_control
   :host-os: unix
   :board: te0950_mbv32
   :goals: run
   :compact:
