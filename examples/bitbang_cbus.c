/*  bitbang_cbus.c

    Example to use CBUS bitbang mode of newer chipsets.
    You must enable CBUS bitbang mode in the EEPROM first.

    Thanks to Steve Brown <sbrown@ewol.com> for the
    the information how to do it.

    The top nibble controls input/output and the bottom nibble
    controls the state of the lines set to output. The datasheet isn't clear
    what happens if you set a bit in the output register when that line is
    conditioned for input. This is described in more detail
    in the FT232R bitbang app note.

    BITMASK
    CBUS Bits
    3210 3210
    xxxx xxxx
    |    |------ Output Control 0->LO, 1->HI
    |----------- Input/Output   0->Input, 1->Output

    Example:
    All pins to output with 0 bit high: 0xF1 (11110001)
    Bits 0 and 1 to input, 2 and 3 to output and masked high: 0xCC (11001100)

    The input is standard "0x" hex notation.
    A carriage return terminates the program.

    This program is distributed under the GPL, version 2
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <briteblox.h>

int main(void)
{
    struct briteblox_context *briteblox;
    int f;
    unsigned char buf[1];
    unsigned char bitmask;
    char input[10];

    if ((briteblox = briteblox_new()) == 0)
    {
        fprintf(stderr, "briteblox_new failed\n");
        return EXIT_FAILURE;
    }

    f = briteblox_usb_open(briteblox, 0x0403, 0x7AD0);
    if (f < 0 && f != -5)
    {
        fprintf(stderr, "unable to open briteblox device: %d (%s)\n", f, briteblox_get_error_string(briteblox));
        briteblox_free(briteblox);
        exit(-1);
    }
    printf("briteblox open succeeded: %d\n",f);

    while (1)
    {
        // Set bitmask from input
        fgets(input, sizeof(input) - 1, stdin);
        if (input[0] == '\n') break;
        bitmask = strtol(input, NULL, 0);
        printf("Using bitmask 0x%02x\n", bitmask);
        f = briteblox_set_bitmode(briteblox, bitmask, BITMODE_CBUS);
        if (f < 0)
        {
            fprintf(stderr, "set_bitmode failed for 0x%x, error %d (%s)\n", bitmask, f, briteblox_get_error_string(briteblox));
            briteblox_usb_close(briteblox);
            briteblox_free(briteblox);
            exit(-1);
        }

        // read CBUS
        f = briteblox_read_pins(briteblox, &buf[0]);
        if (f < 0)
        {
            fprintf(stderr, "read_pins failed, error %d (%s)\n", f, briteblox_get_error_string(briteblox));
            briteblox_usb_close(briteblox);
            briteblox_free(briteblox);
            exit(-1);
        }
        printf("Read returned 0x%01x\n", buf[0] & 0x0f);
    }
    printf("disabling bitbang mode\n");
    briteblox_disable_bitbang(briteblox);

    briteblox_usb_close(briteblox);
    briteblox_free(briteblox);

    return 0;
}
