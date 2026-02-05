# TupleOS Architecture

How the kernel actually works, file by file. Written so future-us (or anyone reading this) can understand what's going on without reverse-engineering everything.

Last updated: February 2026 (Phase 0-1)

---

## Boot Flow

Here's what happens from power-on to blinking cursor, in order:

1. **BIOS** does its thing (POST, hardware init)
2. **GRUB** loads from the ISO, finds our kernel via `boot/grub.cfg`
3. **`boot/boot.asm`** — GRUB jumps to `_start`. We set up a 16KB stack and call `kernel_main()`
4. **`kernel/kernel.c`** — `kernel_main()` initializes the GDT, IDT, and keyboard, then prints to screen
5. The kernel sits in a halt loop waiting for interrupts (keyboard, timer, etc.)

That's it. There's no scheduler, no filesystem, no userspace yet. Just a bootable kernel that can respond to hardware.

---

## File Map

### Boot

**`boot/boot.asm`** — The very first code that runs. Written in x86 assembly because you can't set up a stack in C (you need a stack to run C). It does three things:
1. Declares a Multiboot header so GRUB recognizes us as a kernel
2. Points the stack pointer (`esp`) at our 16KB stack
3. Calls `kernel_main()` in C
4. If `kernel_main` ever returns, halts the CPU forever

**`boot/grub.cfg`** — Tells GRUB what to display in the boot menu and where to find our kernel binary. Two lines of actual config.

**`boot/gdt_flush.asm`** — Loads the GDT into the CPU. We can't do this from C because it requires the `lgdt` instruction and a far jump to reload the code segment register. The far jump looks weird — it jumps to the very next line — but the act of jumping is what forces the CPU to actually use the new GDT. Without it, the segment registers still point to GRUB's old GDT.

**`boot/interrupts.asm`** — Assembly stubs for all 48 interrupts (32 CPU exceptions + 16 hardware IRQs). Each stub is almost identical: push the interrupt number onto the stack, save all registers, call our C handler, restore registers, return. The reason we need 48 separate stubs instead of one is that the CPU doesn't tell us which interrupt fired — the stub itself encodes that information by pushing a hardcoded number. Some CPU exceptions push an error code automatically and some don't, so the stubs that don't get one push a dummy zero to keep the stack layout consistent.

### Kernel

**`kernel/kernel.c`** — The main kernel file. Contains the VGA text mode driver and `kernel_main()`. All terminal output goes through here — it writes directly to the VGA buffer at physical address `0xB8000`. Each character on screen is 2 bytes: one for the ASCII character, one for the color. The screen is 80 columns by 25 rows. When text reaches the bottom, everything scrolls up by one line.

Functions:
- `terminal_initialize()` — clears the screen, resets cursor to top-left
- `terminal_putchar()` — prints one character, handles newlines and line wrapping
- `terminal_scroll()` — moves every row up by one, clears the bottom row
- `terminal_writestring()` — prints a null-terminated string
- `terminal_setcolor()` — changes the text color for subsequent output

**`kernel/ports.h`** — Inline functions for talking to hardware via x86 I/O ports. This is how the CPU communicates with devices like the keyboard, PIC, and timer. Two functions: `outb()` sends a byte to a port, `inb()` reads a byte from a port. Every hardware driver in the kernel uses these. Also has `io_wait()` which writes to an unused port to create a tiny delay — some old hardware needs a moment between commands.

**`kernel/gdt.h` / `kernel/gdt.c`** — Sets up the Global Descriptor Table. The GDT is an x86 structure that defines memory segments. We run in "flat mode" where both segments (code and data) cover the entire 4GB address space (base=0, limit=0xFFFFFFFF). This effectively makes segmentation a no-op — we'll use paging for real memory protection later. The CPU requires a GDT even if you don't actually want segmentation, so we give it the simplest possible one: a null entry (required), a kernel code segment, and a kernel data segment.

The entry format is famously ugly — Intel split the base address and limit across non-contiguous bits for backwards compatibility with the 286. `gdt_set_entry()` handles all the bit-shuffling so we don't have to think about it.

**`kernel/idt.h` / `kernel/idt.c`** — Sets up the Interrupt Descriptor Table and handles interrupt dispatch. The IDT maps interrupt numbers (0-255) to handler functions. When interrupt N fires, the CPU looks up IDT[N] and jumps to the handler address stored there.

Also handles PIC remapping. The PIC (Programmable Interrupt Controller) is the chip that routes hardware interrupts to the CPU. By default it maps IRQ 0-7 to interrupts 8-15, which collides with CPU exceptions. We reprogram it to use interrupts 32-47 instead. This involves sending a specific sequence of bytes (ICW1-ICW4) to ports 0x20/0x21 (master PIC) and 0xA0/0xA1 (slave PIC). It's a rigid protocol, not something we designed — it's just how the Intel 8259 PIC chip works.

Interrupt dispatch: each interrupt has a slot in a function pointer array. Drivers register their handlers by calling `idt_register_handler(interrupt_number, function)`. When an interrupt fires, the assembly stub calls `interrupt_handler()` in C, which looks up the right function pointer and calls it. For hardware IRQs, we also send an "End of Interrupt" (EOI) signal to the PIC, or it won't send us any more interrupts.

### Config / Build

**`linker.ld`** — Tells the linker how to arrange the final kernel binary in memory. The kernel gets loaded at physical address 0x100000 (1MB mark) — everything below that is reserved by the BIOS and legacy hardware. Sections are placed in order: `.multiboot` (the header GRUB looks for), `.text` (code), `.rodata` (read-only data), `.data` (initialized globals), `.bss` (uninitialized globals). All sections are 4KB-aligned because paging (when we add it) works in 4KB pages.

**`Makefile`** — Build automation. Compiles assembly with `i686-elf-as`, compiles C with `i686-elf-gcc`, links everything with our linker script, and packages it into a bootable ISO using `grub-mkrescue`. The cross-compiler is necessary because our system compiler targets Linux/Windows, but we need code that runs on bare metal with no OS. Key flags: `-ffreestanding` (no OS assumptions), `-nostdlib` (no libc), `-fno-stack-protector` (no libc-dependent stack protection).

---

## How Interrupts Work (the full picture)

This confused us at first so here's the complete flow:

1. A key is pressed on the keyboard
2. The keyboard controller sends an electrical signal on IRQ line 1
3. The PIC receives it, translates IRQ 1 to interrupt 33 (because we remapped IRQs to start at 32)
4. The PIC signals the CPU: "hey, interrupt 33"
5. The CPU stops whatever it's doing, pushes EIP/CS/EFLAGS onto the stack
6. The CPU looks up IDT[33], finds the address of `irq1` in `interrupts.asm`
7. `irq1` pushes the interrupt number (33) and jumps to `isr_common`
8. `isr_common` saves all registers (`pusha`), then calls `interrupt_handler()` in C
9. `interrupt_handler()` looks up `handlers[33]`, finds the keyboard handler function, calls it
10. The keyboard handler reads the scancode from port 0x60, translates it to a character, prints it
11. `interrupt_handler()` sends EOI to the PIC ("thanks, I handled it, send me more")
12. Control returns to `isr_common`, which restores all registers and runs `iret`
13. The CPU pops EIP/CS/EFLAGS and resumes whatever it was doing before

---

## Memory Layout

0x00000000 ┌─────────────────────────┐
│ Real mode IVT & BIOS    │ (reserved, don't touch)
0x000A0000 ├─────────────────────────┤
│ VGA memory              │ (0xB8000 = text mode buffer)
0x00100000 ├─────────────────────────┤
│ OUR KERNEL LIVES HERE   │ (loaded by GRUB)
│  .multiboot             │
│  .text (code)           │
│  .rodata (constants)    │
│  .data (globals)        │
│  .bss (stack, etc.)     │
├─────────────────────────┤
│ Free memory             │ (we'll manage this in Phase 2)
0xFFFFFFFF └─────────────────────────┘


---

## Segment Selectors Quick Reference

These hex values show up everywhere in the assembly and can be confusing:

- `0x00` — Null segment (GDT index 0). Unused, CPU requires it to exist.
- `0x08` — Kernel code segment (GDT index 1). Used in far jumps and IDT entries.
- `0x10` — Kernel data segment (GDT index 2). Loaded into DS, ES, SS, etc.

The formula: `selector = GDT_index * 8`. Each GDT entry is 8 bytes, and the lower 3 bits of a selector are flags (privilege level + GDT/LDT bit), so index 1 = 0x08, index 2 = 0x10.