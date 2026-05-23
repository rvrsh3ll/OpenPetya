// keyboard.c

#include "keyboard.h"
#include "io.h"

static int shift_pressed = 0;
static int caps_lock = 0;

static const char scancode_table[] = {
    0,    0,   '1', '2', '3', '4', '5', '6',        // 0x00~0x07
    '7', '8', '9', '0', '-', '=',  '\b', '\t',      // 0x08~0x0F
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',         // 0x10~0x17
    'o', 'p', '[', ']', '\n',  0,  'a', 's',        // 0x18~0x1F
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',         // 0x20~0x27
    '\'', '`',  0,  '\\', 'z', 'x', 'c', 'v',       // 0x28~0x2F
    'b', 'n', 'm', ',', '.', '/',   0,  '*',        // 0x30~0x37
     0,  ' ',                                       // 0x38~0x39
};

static const char scancode_shift_table[] = {
    0,    0,   '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', '\n',  0,  'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~',   0,  '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?',   0,  '*',
     0,  ' ',
};

static uint8_t read_scancode(void)
{
    while (!keyboard_hashkey());

    return inb(KEYBOARD_DATA_PORT);
}

int keyboard_hashkey(void)
{
    return inb(KEYBOARD_STATUS_PORT) & 0x01;
}

char keyboard_getchar(void)
{
    while (1)
    {
        uint8_t sc = read_scancode();

        if (sc & 0x80)
        {
            uint8_t released = sc & 0x7F;
            if (released == 0x2A || released == 0x36)
                shift_pressed = 0;

            continue;
        }

        if (sc == 0x2A || sc == 0x36)
        {
            shift_pressed = 1;
            continue;
        }

        if (sc == 0x3A)
        {
            caps_lock = !caps_lock;
            continue;
        }

        if (sc >= sizeof(scancode_table))
            continue;

        char c;
        if (shift_pressed)
            c = scancode_shift_table[sc];
        else
            c = scancode_table[sc];

        if (c == 0)
            continue;

        // Caps Lock only affects letters
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
        {
            if (caps_lock ^ shift_pressed)
            {
                if (c >= 'a' && c <= 'z')
                    c -= 32;
            }
            else
            {
                if (c >= 'A' && c <= 'Z')
                    c += 32;
            }
        }

        return c;
    }
}

void keyboard_readline(char *buffer, int max)
{
    int i = 0;
    while (i < max - 1)
    {
        char c = keyboard_getchar();

        if (c == '\n')
        {
            vga_putchar('\n');
            break;
        }

        if (c == '\b')
        {
            if (i > 0)
            {
                i--;
                vga_putchar('\b');
            }

            continue;
        }

        buffer[i++] = c;
        vga_putchar(c);
    }

    buffer[i] = '\0';
}

void keyboard_flush(void)
{
    for (volatile uint32_t i = 0; i < 20000000; i++);

    while (inb(KEYBOARD_STATUS_PORT) & 0x01)
        inb(KEYBOARD_DATA_PORT);
}