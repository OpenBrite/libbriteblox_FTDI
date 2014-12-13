/* bitbang_ft2232.c

   Output some flickering in bitbang mode to the FT2232

   Thanks to max@koeln.ccc.de for fixing and extending
   the example for the second channel.

   This program is distributed under the GPL, version 2
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <briteblox.h>

int main(int argc, char **argv)
{
    struct briteblox_context *briteblox, *briteblox2;
    unsigned char buf[1];
    int f,i;

    // Init 1. channel
    if ((briteblox = briteblox_new()) == 0)
    {
        fprintf(stderr, "briteblox_new failed\n");
        return EXIT_FAILURE;
    }

    briteblox_set_interface(briteblox, INTERFACE_A);
    f = briteblox_usb_open(briteblox, 0x0403, 0x7AD0);
    if (f < 0 && f != -5)
    {
        fprintf(stderr, "unable to open briteblox device: %d (%s)\n", f, briteblox_get_error_string(briteblox));
        briteblox_free(briteblox);
        exit(-1);
    }
    printf("briteblox open succeeded(channel 1): %d\n",f);

    printf("enabling bitbang mode(channel 1)\n");
    briteblox_set_bitmode(briteblox, 0xFF, BITMODE_BITBANG);

    // Init 2. channel
    if ((briteblox2 = briteblox_new()) == 0)
    {
        fprintf(stderr, "briteblox_new failed\n");
        return EXIT_FAILURE;
    }
    briteblox_set_interface(briteblox2, INTERFACE_B);
    f = briteblox_usb_open(briteblox2, 0x0403, 0x7AD0);
    if (f < 0 && f != -5)
    {
        fprintf(stderr, "unable to open briteblox device: %d (%s)\n", f, briteblox_get_error_string(briteblox2));
        briteblox_free(briteblox2);
        exit(-1);
    }
    printf("briteblox open succeeded(channel 2): %d\n",f);

    printf("enabling bitbang mode (channel 2)\n");
    briteblox_set_bitmode(briteblox2, 0xFF, BITMODE_BITBANG);

    // Write data
    printf("startloop\n");
    for (i = 0; i < 23; i++)
    {
        buf[0] =  0x1;
        printf("porta: %02i: 0x%02x \n",i,buf[0]);
        f = briteblox_write_data(briteblox, buf, 1);
        if (f < 0)
            fprintf(stderr,"write failed on channel 1 for 0x%x, error %d (%s)\n", buf[0], f, briteblox_get_error_string(briteblox));
        usleep(1 * 1000000);

        buf[0] =  0x2;
        printf("porta: %02i: 0x%02x \n",i,buf[0]);
        f = briteblox_write_data(briteblox, buf, 1);
        if (f < 0)
            fprintf(stderr,"write failed on channel 1 for 0x%x, error %d (%s)\n", buf[0], f, briteblox_get_error_string(briteblox));
        usleep(1 * 1000000);

        buf[0] =  0x1;
        printf("portb: %02i: 0x%02x \n",i,buf[0]);
        f = briteblox_write_data(briteblox2, buf, 1);
        if (f < 0)
            fprintf(stderr,"write failed on channel 2 for 0x%x, error %d (%s)\n", buf[0], f, briteblox_get_error_string(briteblox2));
        usleep(1 * 1000000);

        buf[0] =  0x2;
        printf("portb: %02i: 0x%02x \n",i,buf[0]);
        f = briteblox_write_data(briteblox2, buf, 1);
        if (f < 0)
            fprintf(stderr,"write failed on channel 2 for 0x%x, error %d (%s)\n", buf[0], f, briteblox_get_error_string(briteblox2));
        usleep(1 * 1000000);
    }
    printf("\n");

    printf("disabling bitbang mode(channel 1)\n");
    briteblox_disable_bitbang(briteblox);
    briteblox_usb_close(briteblox);
    briteblox_free(briteblox);

    printf("disabling bitbang mode(channel 2)\n");
    briteblox_disable_bitbang(briteblox2);
    briteblox_usb_close(briteblox2);
    briteblox_free(briteblox2);

    return 0;
}
