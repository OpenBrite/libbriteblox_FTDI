/* simple.c

   Simple libbriteblox usage example

   This program is distributed under the GPL, version 2
*/

#include <stdio.h>
#include <stdlib.h>
#include <briteblox.h>

int main(void)
{
    int ret;
    struct briteblox_context *briteblox;
    struct briteblox_version_info version;
    if ((briteblox = briteblox_new()) == 0)
   {
        fprintf(stderr, "briteblox_new failed\n");
        return EXIT_FAILURE;
    }

    version = briteblox_get_library_version();
    printf("Initialized libbriteblox %s (major: %d, minor: %d, micro: %d, snapshot ver: %s)\n",
        version.version_str, version.major, version.minor, version.micro,
        version.snapshot_str);

    if ((ret = briteblox_usb_open(briteblox, 0x0403, 0x6001)) < 0)
    {
        fprintf(stderr, "unable to open briteblox device: %d (%s)\n", ret, briteblox_get_error_string(briteblox));
        briteblox_free(briteblox);
        return EXIT_FAILURE;
    }

    // Read out BRITEBLOXChip-ID of R type chips
    if (briteblox->type == TYPE_R)
    {
        unsigned int chipid;
        printf("briteblox_read_chipid: %d\n", briteblox_read_chipid(briteblox, &chipid));
        printf("BRITEBLOX chipid: %X\n", chipid);
    }

    if ((ret = briteblox_usb_close(briteblox)) < 0)
    {
        fprintf(stderr, "unable to close briteblox device: %d (%s)\n", ret, briteblox_get_error_string(briteblox));
        briteblox_free(briteblox);
        return EXIT_FAILURE;
    }

    briteblox_free(briteblox);

    return EXIT_SUCCESS;
}
