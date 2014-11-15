/* LIBBRITEBLOX EEPROM access example

   This program is distributed under the GPL, version 2
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <briteblox.h>

int read_decode_eeprom(struct briteblox_context *briteblox)
{
    int i, j, f;
    int value;
    int size;
    unsigned char buf[256];

    f = briteblox_read_eeprom(briteblox);
    if (f < 0)
    {
        fprintf(stderr, "briteblox_read_eeprom: %d (%s)\n",
                f, briteblox_get_error_string(briteblox));
        return -1;
    }


    briteblox_get_eeprom_value(briteblox, CHIP_SIZE, & value);
    if (value <0)
    {
        fprintf(stderr, "No EEPROM found or EEPROM empty\n");
        fprintf(stderr, "On empty EEPROM, use -w option to write default values\n");
        return -1;
    }
    fprintf(stderr, "Chip type %d briteblox_eeprom_size: %d\n", briteblox->type, value);
    if (briteblox->type == TYPE_R)
        size = 0xa0;
    else
        size = value;
    briteblox_get_eeprom_buf(briteblox, buf, size);
    for (i=0; i < size; i += 16)
    {
        fprintf(stdout,"0x%03x:", i);

        for (j = 0; j< 8; j++)
            fprintf(stdout," %02x", buf[i+j]);
        fprintf(stdout," ");
        for (; j< 16; j++)
            fprintf(stdout," %02x", buf[i+j]);
        fprintf(stdout," ");
        for (j = 0; j< 8; j++)
            fprintf(stdout,"%c", isprint(buf[i+j])?buf[i+j]:'.');
        fprintf(stdout," ");
        for (; j< 16; j++)
            fprintf(stdout,"%c", isprint(buf[i+j])?buf[i+j]:'.');
        fprintf(stdout,"\n");
    }

    f = briteblox_eeprom_decode(briteblox, 1);
    if (f < 0)
    {
        fprintf(stderr, "briteblox_eeprom_decode: %d (%s)\n",
                f, briteblox_get_error_string(briteblox));
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    struct briteblox_context *briteblox;
    int f, i;
    int vid = 0;
    int pid = 0;
    char const *desc    = 0;
    char const *serial  = 0;
    int erase = 0;
    int use_defaults = 0;
    int large_chip = 0;
    int do_write = 0;
    int retval = 0;
    int value;

    if ((briteblox = briteblox_new()) == 0)
    {
        fprintf(stderr, "Failed to allocate briteblox structure :%s \n",
                briteblox_get_error_string(briteblox));
        return EXIT_FAILURE;
    }

    while ((i = getopt(argc, argv, "d::ev:p:l:P:S:w")) != -1)
    {
        switch (i)
        {
            case 'd':
                use_defaults = 1;
                if (optarg)
                    large_chip = 0x66;
                break;
            case 'e':
                erase = 1;
                break;
            case 'v':
                vid = strtoul(optarg, NULL, 0);
                break;
            case 'p':
                pid = strtoul(optarg, NULL, 0);
                break;
            case 'P':
                desc = optarg;
                break;
            case 'S':
                serial = optarg;
                break;
            case 'w':
                do_write  = 1;
                break;
            default:
                fprintf(stderr, "usage: %s [options]\n", *argv);
                fprintf(stderr, "\t-d[num] Work with default valuesfor 128 Byte "
                        "EEPROM or for 256 Byte EEPROM if some [num] is given\n");
                fprintf(stderr, "\t-w write\n");
                fprintf(stderr, "\t-e erase\n");
                fprintf(stderr, "\t-v verbose decoding\n");
                fprintf(stderr, "\t-p <number> Search for device with PID == number\n");
                fprintf(stderr, "\t-v <number> Search for device with VID == number\n");
                fprintf(stderr, "\t-P <string? Search for device with given "
                        "product description\n");
                fprintf(stderr, "\t-S <string? Search for device with given "
                        "serial number\n");
                retval = -1;
                goto done;
        }
    }

    // Select first interface
    briteblox_set_interface(briteblox, INTERFACE_ANY);

    if (!vid && !pid && desc == NULL && serial == NULL)
    {
        struct briteblox_device_list *devlist, *curdev;
        int res;
        if ((res = briteblox_usb_find_all(briteblox, &devlist, 0, 0)) < 0)
        {
            fprintf(stderr, "No BRITEBLOX with default VID/PID found\n");
            retval =  EXIT_FAILURE;
            goto do_deinit;
        }
        if (res > 1)
        {
            int i = 1;
            fprintf(stderr, "%d BRITEBLOX devices found: Only Readout on EEPROM done. ",res);
            fprintf(stderr, "Use VID/PID/desc/serial to select device\n");
            for (curdev = devlist; curdev != NULL; curdev= curdev->next, i++)
            {
                f = briteblox_usb_open_dev(briteblox,  curdev->dev);
                if (f<0)
                {
                    fprintf(stderr, "Unable to open device %d: (%s)",
                            i, briteblox_get_error_string(briteblox));
                    continue;
                }
                fprintf(stderr, "Decoded values of device %d:\n", i);
                read_decode_eeprom(briteblox);
                briteblox_usb_close(briteblox);
            }
            briteblox_list_free(&devlist);
            retval = EXIT_SUCCESS;
            goto do_deinit;
        }
        else if (res == 1)
        {
            f = briteblox_usb_open_dev(briteblox,  devlist[0].dev);
            if (f<0)
            {
                fprintf(stderr, "Unable to open device %d: (%s)",
                        i, briteblox_get_error_string(briteblox));
            }
        }
        else
        {
            fprintf(stderr, "No devices found\n");
            f = 0;
        }
        briteblox_list_free(&devlist);
    }
    else
    {
        // Open device
        f = briteblox_usb_open_desc(briteblox, vid, pid, desc, serial);
        if (f < 0)
        {
            fprintf(stderr, "Device VID 0x%04x PID 0x%04x", vid, pid);
            if (desc)
                fprintf(stderr, " Desc %s", desc);
            if (serial)
                fprintf(stderr, " Serial %s", serial);
            fprintf(stderr, "\n");
            fprintf(stderr, "unable to open briteblox device: %d (%s)\n",
                    f, briteblox_get_error_string(briteblox));
            
            retval = -1;
            goto done;
        }
    }
    if (erase)
    {
        f = briteblox_erase_eeprom(briteblox); /* needed to determine EEPROM chip type */
        if (f < 0)
        {
            fprintf(stderr, "Erase failed: %s",
                    briteblox_get_error_string(briteblox));
            retval =  -2;
            goto done;
        }
        if (briteblox_get_eeprom_value(briteblox, CHIP_TYPE, & value) <0)
        {
            fprintf(stderr, "briteblox_get_eeprom_value: %d (%s)\n",
                    f, briteblox_get_error_string(briteblox));
        }
        if (value == -1)
            fprintf(stderr, "No EEPROM\n");
        else if (value == 0)
            fprintf(stderr, "Internal EEPROM\n");
        else
            fprintf(stderr, "Found 93x%02x\n", value);
        retval = 0;
        goto done;
    }

    if (use_defaults)
    {
        briteblox_eeprom_initdefaults(briteblox, NULL, NULL, NULL);
        if (briteblox_set_eeprom_value(briteblox, MAX_POWER, 500) <0)
        {
            fprintf(stderr, "briteblox_set_eeprom_value: %d (%s)\n",
                    f, briteblox_get_error_string(briteblox));
        }
        if (large_chip)
            if (briteblox_set_eeprom_value(briteblox, CHIP_TYPE, 0x66) <0)
            {
                fprintf(stderr, "briteblox_set_eeprom_value: %d (%s)\n",
                        f, briteblox_get_error_string(briteblox));
            }
        f=(briteblox_eeprom_build(briteblox));
        if (f < 0)
        {
            fprintf(stderr, "briteblox_eeprom_build: %d (%s)\n",
                    f, briteblox_get_error_string(briteblox));
            retval = -1;
            goto done;
        }
    }
    else if (do_write)
    {
        briteblox_eeprom_initdefaults(briteblox, NULL, NULL, NULL);
        f = briteblox_erase_eeprom(briteblox);
        if (briteblox_set_eeprom_value(briteblox, MAX_POWER, 500) <0)
        {
            fprintf(stderr, "briteblox_set_eeprom_value: %d (%s)\n",
                    f, briteblox_get_error_string(briteblox));
        }
        f = briteblox_erase_eeprom(briteblox);/* needed to determine EEPROM chip type */
        if (briteblox_get_eeprom_value(briteblox, CHIP_TYPE, & value) <0)
        {
            fprintf(stderr, "briteblox_get_eeprom_value: %d (%s)\n",
                    f, briteblox_get_error_string(briteblox));
        }
        if (value == -1)
            fprintf(stderr, "No EEPROM\n");
        else if (value == 0)
            fprintf(stderr, "Internal EEPROM\n");
        else
            fprintf(stderr, "Found 93x%02x\n", value);
        f=(briteblox_eeprom_build(briteblox));
        if (f < 0)
        {
            fprintf(stderr, "Erase failed: %s",
                    briteblox_get_error_string(briteblox));
            retval = -2;
            goto done;
        }
        f = briteblox_write_eeprom(briteblox);
        {
            fprintf(stderr, "briteblox_eeprom_decode: %d (%s)\n",
                    f, briteblox_get_error_string(briteblox));
            retval = 1;
            goto done;
        }
    }
    retval = read_decode_eeprom(briteblox);
done:
    briteblox_usb_close(briteblox);
do_deinit:
    briteblox_free(briteblox);
    return retval;
}
