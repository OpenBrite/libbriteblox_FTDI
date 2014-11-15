/* find_all.c

   Example for briteblox_usb_find_all()

   This program is distributed under the GPL, version 2
*/

#include <stdio.h>
#include <stdlib.h>
#include <briteblox.h>

int main(void)
{
    int ret, i;
    struct briteblox_context *briteblox;
    struct briteblox_device_list *devlist, *curdev;
    char manufacturer[128], description[128];
    int retval = EXIT_SUCCESS;

    if ((briteblox = briteblox_new()) == 0)
    {
        fprintf(stderr, "briteblox_new failed\n");
        return EXIT_FAILURE;
    }

    if ((ret = briteblox_usb_find_all(briteblox, &devlist, 0, 0)) < 0)
    {
        fprintf(stderr, "briteblox_usb_find_all failed: %d (%s)\n", ret, briteblox_get_error_string(briteblox));
        retval =  EXIT_FAILURE;
        goto do_deinit;
    }

    printf("Number of BRITEBLOX devices found: %d\n", ret);

    i = 0;
    for (curdev = devlist; curdev != NULL; i++)
    {
        printf("Checking device: %d\n", i);
        if ((ret = briteblox_usb_get_strings(briteblox, curdev->dev, manufacturer, 128, description, 128, NULL, 0)) < 0)
        {
            fprintf(stderr, "briteblox_usb_get_strings failed: %d (%s)\n", ret, briteblox_get_error_string(briteblox));
            retval = EXIT_FAILURE;
            goto done;
        }
        printf("Manufacturer: %s, Description: %s\n\n", manufacturer, description);
        curdev = curdev->next;
    }
done:
    briteblox_list_free(&devlist);
do_deinit:
    briteblox_free(briteblox);

    return retval;
}
