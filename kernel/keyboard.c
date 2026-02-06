#include "keyboard.h"
#include "ports.h"
#include "idt.h"
#include "terminal.h"
#include "shell.h"

#include <stdbool.h>

/*
* Flow:
* 1. User presses a key
* 2. Keyboard controller sends IRQ 1 -> interrupt 33 after PIC remap
* 3. Our handler reads the scancode from port 0x60
* 4. We look up the scancode in a table to get the ASCII character
* 5. We print the character to the screen

* Scancodes aren't ASCII, they're arbitrary numbers assigned to physical key positions. 'A' is 0x1E, 'B' is 0x30, etc.
* The mapping has no logical pattern, it's based on the physical keyboard layout
*/

#define KEYBOARD_DATA_PORT 0x60

#define LEFT_SHIFT_PRESSED 0x2A
#define LEFT_SHIFT_RELEASED 0xAA
#define RIGHT_SHIFT_PRESSED 0x36
#define RIGHT_SHIFT_RELEASED 0xB6

static bool shift_held = false;

// Lowercase / unshifted characters
static const char scancode_to_ascii[128] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.',
    0, 0, 0, 0, 0
};

// Uppercase / shifted characters
static const char scancode_to_ascii_shifted[128] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.',
    0, 0, 0, 0, 0
};

static void keyboard_handler(struct interrupt_frame* frame) {
    (void)frame; // unused parameter to avoid compiler warning

    // Read the scancode from the keyboard controller
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    // Handle shift key presses/releases
    if (scancode == LEFT_SHIFT_PRESSED || scancode == RIGHT_SHIFT_PRESSED) {
        shift_held = true;
        return;
    }
    if (scancode == LEFT_SHIFT_RELEASED || scancode == RIGHT_SHIFT_RELEASED) {
        shift_held = false;
        return;
    }

    // Ignore key releases
    if (scancode & 0x80) {
        return;
    }

    // Look up the ASCII char, and make sure scancode is in bounds before indexing
    if (scancode < 128) {
        char c = shift_held ? scancode_to_ascii_shifted[scancode] : scancode_to_ascii[scancode];

        // If it's a printable char, print it
        if (c != 0) {
            shell_handle_key(c);
        }
    }
}

void keyboard_init(void) {
    // register our handler for IRQ 1 (interrupt 33 after PIC remap)
    idt_register_handler(KEYBOARD_IRQ, keyboard_handler);

    /* The keyboard is alread enabled by the BIOS, so we don't need to send any initialization commands. Just register the handler and we're good to go
    *  A fanicer driver would reset the keyboard, set the repeat rate, enable scanning, set LED state, but the defaults work fine for now
    */
}

