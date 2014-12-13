/* briteblox_out.c
 *
 * Output a (stream of) byte(s) in bitbang mode to the
 * briteblox245 chip that is (hopefully) attached.
 *
 * We have a little board that has a FT245BM chip and
 * the 8 outputs are connected to several different
 * things that we can turn on and off with this program.
 *
 * If you have an idea about hardware that can easily
 * interface onto an BRITEBLOX chip, I'd like to collect
 * ideas. If I find it worthwhile to make, I'll consider
 * making it, I'll even send you a prototype (against
 * cost-of-material) if you want.
 *
 * At "harddisk-recovery.nl" they have a little board that
 * controls the power to two harddrives and two fans.
 *
 * -- REW R.E.Wolff@BitWizard.nl
 *
 *
 *
 * This program was based on libbriteblox_example_bitbang2232.c
 * which doesn't carry an author or attribution header.
 *
 *
 * This program is distributed under the GPL, version 2.
 * Millions copies of the GPL float around the internet.
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <briteblox.h>

void briteblox_fatal (struct briteblox_context *briteblox, char *str)
{
    fprintf (stderr, "%s: %s\n",
             str, briteblox_get_error_string (briteblox));
    briteblox_free(briteblox);
    exit (1);
}

int main(int argc, char **argv)
{
    struct briteblox_context *briteblox;
    int i, t;
    unsigned char data;
    int delay = 100000; /* 100 thousand microseconds: 1 tenth of a second */

    while ((t = getopt (argc, argv, "d:")) != -1)
    {
        switch (t)
        {
            case 'd':
                delay = atoi (optarg);
                break;
        }
    }

    if ((briteblox = briteblox_new()) == 0)
    {
        fprintf(stderr, "briteblox_bew failed\n");
        return EXIT_FAILURE;
    }

    if (briteblox_usb_open(briteblox, 0x0403, 0x7AD0) < 0)
        briteblox_fatal (briteblox, "Can't open briteblox device");

    if (briteblox_set_bitmode(briteblox, 0xFF, BITMODE_BITBANG) < 0)
        briteblox_fatal (briteblox, "Can't enable bitbang");

    for (i=optind; i < argc ; i++)
    {
        sscanf (argv[i], "%x", &t);
        data = t;
        if (briteblox_write_data(briteblox, &data, 1) < 0)
        {
            fprintf(stderr,"write failed for 0x%x: %s\n",
                    data, briteblox_get_error_string(briteblox));
        }
        usleep(delay);
    }

    briteblox_usb_close(briteblox);
    briteblox_free(briteblox);
    exit (0);
}
