/* This program is distributed under the GPL, version 2 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <briteblox.h>

int main(int argc, char **argv)
{
    struct briteblox_context *briteblox;
    int f,i;
    unsigned char buf[1];
    int retval = 0;

    if ((briteblox = briteblox_new()) == 0)
    {
        fprintf(stderr, "briteblox_new failed\n");
        return EXIT_FAILURE;
    }

    f = briteblox_usb_open(briteblox, 0x0403, 0x6001);

    if (f < 0 && f != -5)
    {
        fprintf(stderr, "unable to open briteblox device: %d (%s)\n", f, briteblox_get_error_string(briteblox));
        retval = 1;
        goto done;
    }

    printf("briteblox open succeeded: %d\n",f);

    printf("enabling bitbang mode\n");
    briteblox_set_bitmode(briteblox, 0xFF, BITMODE_BITBANG);

    usleep(3 * 1000000);

    buf[0] = 0x0;
    printf("turning everything on\n");
    f = briteblox_write_data(briteblox, buf, 1);
    if (f < 0)
    {
        fprintf(stderr,"write failed for 0x%x, error %d (%s)\n",buf[0],f, briteblox_get_error_string(briteblox));
    }

    usleep(3 * 1000000);

    buf[0] = 0xFF;
    printf("turning everything off\n");
    f = briteblox_write_data(briteblox, buf, 1);
    if (f < 0)
    {
        fprintf(stderr,"write failed for 0x%x, error %d (%s)\n",buf[0],f, briteblox_get_error_string(briteblox));
    }

    usleep(3 * 1000000);

    for (i = 0; i < 32; i++)
    {
        buf[0] =  0 | (0xFF ^ 1 << (i % 8));
        if ( i > 0 && (i % 8) == 0)
        {
            printf("\n");
        }
        printf("%02hhx ",buf[0]);
        fflush(stdout);
        f = briteblox_write_data(briteblox, buf, 1);
        if (f < 0)
        {
            fprintf(stderr,"write failed for 0x%x, error %d (%s)\n",buf[0],f, briteblox_get_error_string(briteblox));
        }
        usleep(1 * 1000000);
    }

    printf("\n");

    printf("disabling bitbang mode\n");
    briteblox_disable_bitbang(briteblox);

    briteblox_usb_close(briteblox);
done:
    briteblox_free(briteblox);

    return retval;
}
