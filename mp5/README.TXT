CSCE 410/611: MP5 -- README.TXT

This file describes the content of this directory.
A file marked with (*) may be of some interest to the 
student, and he/she may want to have a look at it.
A file marked with (**) is important for this MP, and the
student is strongly encouraged to study the contents of this file.

COMPILATION:
===========

FILE:                   DESCRIPTION:

makefile (**)           Makefile for Linux 64-bit environment.
                        Works with the provided linux image. 
                        Type "make" to create the kernel.
linker.ld               The linker script.

OS COMPONENTS:
=============

FILE:                   DESCRIPTION:

start.asm (*)           The bootloader starts code in this file, which in turn
                        jumps to the main entry in File "kernel.C".
kernel.C (**)           Main file, where the OS components are set up, and the
                        system gets going.

assert.H/C              Implements the "assert()" utility.
utils.H/C               Various utilities (e.g. memcpy, strlen, etc..)

console.H/C             Routines to print to the screen.

machine.H (*)           Definitions of some system constants and low-level
                        machine operations. 
                        (Primarily memory sizes, register set, and
                        enable/disable interrupts)
gdt.H/C                 Global Descriptor Table.
gdt_low.asm             Low-level GDT code, included in "start.asm".
idt.H/C                 Interrupt Descriptor Table.
idt_low.asm             Low-level IDT code, included in "start.asm".
irq.H/C                 mapping of IRQ's into the IDT.
irq_low.asm             Low-level IRQ stuff. (Primarily the interrupt service
                        routines and the routine stub that branches out to the
                        interrupt dispatcher in "interrupts.C". Included in
                        "start.asm".
			
exceptions.H/C (*)      The exception dispatcher.
interrupts.H/C          The interrupt dispatcher.

console.H/C             Routines to print to the screen.

simple_timer.H/C (*)    Routines to control the periodic interval
                        timer. This is an example of an interrupt 
                        handler.

machine_low.H/asm       Various low-level x86 specific stuff.

page_table.H (**)       Definition of the page table interface.

frame_pool.H/C          Definition and implementation of a
                        vanilla physical frame memory manager.
                        DOES NOT SUPORT contiguous
                        allocation. Also DOES NOT SUPPORT release
                        of frames.
                        FEEL FREE TO REPLACE THIS MANAGER WITH YOUR
                        OWN IMPLEMENTATION!!

mem_pool.H/C            Definition and implementation of a vanilla
                        memory manager.
                        DOES NOT SUPPORT release of memory.
                        FEEL FREE TO REPLACE THIS ABOMINATION WITH YOUR
                        OWN IMPLEMENTATION!!
			 

