/***************************************************************************
                          briteblox.c  -  description
                             -------------------
    begin                : Fri Apr 4 2003
    copyright            : (C) 2003-2014 by Intra2net AG and the libbriteblox developers
    email                : opensource@intra2net.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License           *
 *   version 2.1 as published by the Free Software Foundation;             *
 *                                                                         *
 ***************************************************************************/

/**
    \mainpage libbriteblox API documentation

    Library to talk to BRITEBLOX chips. You find the latest versions of libbriteblox at
    http://www.intra2net.com/en/developer/libbriteblox/

    The library is easy to use. Have a look at this short example:
    \include simple.c

    More examples can be found in the "examples" directory.
*/
/** \addtogroup libbriteblox */
/* @{ */

#include <libusb.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "briteblox_i.h"
#include "briteblox.h"
#include "briteblox_version_i.h"

#define briteblox_error_return(code, str) do {  \
        if ( briteblox )                        \
            briteblox->error_str = str;         \
        else                               \
            fprintf(stderr, str);          \
        return code;                       \
   } while(0);

#define briteblox_error_return_free_device_list(code, str, devs) do {    \
        libusb_free_device_list(devs,1);   \
        briteblox->error_str = str;             \
        return code;                       \
   } while(0);


/**
    Internal function to close usb device pointer.
    Sets briteblox->usb_dev to NULL.
    \internal

    \param briteblox pointer to briteblox_context

    \retval none
*/
static void briteblox_usb_close_internal (struct briteblox_context *briteblox)
{
    if (briteblox && briteblox->usb_dev)
    {
        libusb_close (briteblox->usb_dev);
        briteblox->usb_dev = NULL;
        if(briteblox->eeprom)
            briteblox->eeprom->initialized_for_connected_device = 0;
    }
}

/**
    Initializes a briteblox_context.

    \param briteblox pointer to briteblox_context

    \retval  0: all fine
    \retval -1: couldn't allocate read buffer
    \retval -2: couldn't allocate struct  buffer
    \retval -3: libusb_init() failed

    \remark This should be called before all functions
*/
int briteblox_init(struct briteblox_context *briteblox)
{
    struct briteblox_eeprom* eeprom = (struct briteblox_eeprom *)malloc(sizeof(struct briteblox_eeprom));
    briteblox->usb_ctx = NULL;
    briteblox->usb_dev = NULL;
    briteblox->usb_read_timeout = 5000;
    briteblox->usb_write_timeout = 5000;

    briteblox->type = TYPE_BM;    /* chip type */
    briteblox->baudrate = -1;
    briteblox->bitbang_enabled = 0;  /* 0: normal mode 1: any of the bitbang modes enabled */

    briteblox->readbuffer = NULL;
    briteblox->readbuffer_offset = 0;
    briteblox->readbuffer_remaining = 0;
    briteblox->writebuffer_chunksize = 4096;
    briteblox->max_packet_size = 0;
    briteblox->error_str = NULL;
    briteblox->module_detach_mode = AUTO_DETACH_SIO_MODULE;

    if (libusb_init(&briteblox->usb_ctx) < 0)
        briteblox_error_return(-3, "libusb_init() failed");

    briteblox_set_interface(briteblox, INTERFACE_ANY);
    briteblox->bitbang_mode = 1; /* when bitbang is enabled this holds the number of the mode  */

    if (eeprom == 0)
        briteblox_error_return(-2, "Can't malloc struct briteblox_eeprom");
    memset(eeprom, 0, sizeof(struct briteblox_eeprom));
    briteblox->eeprom = eeprom;

    /* All fine. Now allocate the readbuffer */
    return briteblox_read_data_set_chunksize(briteblox, 4096);
}

/**
    Allocate and initialize a new briteblox_context

    \return a pointer to a new briteblox_context, or NULL on failure
*/
struct briteblox_context *briteblox_new(void)
{
    struct briteblox_context * briteblox = (struct briteblox_context *)malloc(sizeof(struct briteblox_context));

    if (briteblox == NULL)
    {
        return NULL;
    }

    if (briteblox_init(briteblox) != 0)
    {
        free(briteblox);
        return NULL;
    }

    return briteblox;
}

/**
    Open selected channels on a chip, otherwise use first channel.

    \param briteblox pointer to briteblox_context
    \param interface Interface to use for FT2232C/2232H/4232H chips.

    \retval  0: all fine
    \retval -1: unknown interface
    \retval -2: USB device unavailable
    \retval -3: Device already open, interface can't be set in that state
*/
int briteblox_set_interface(struct briteblox_context *briteblox, enum briteblox_interface interface)
{
    if (briteblox == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    if (briteblox->usb_dev != NULL)
    {
        int check_interface = interface;
        if (check_interface == INTERFACE_ANY)
            check_interface = INTERFACE_A;

        if (briteblox->index != check_interface)
            briteblox_error_return(-3, "Interface can not be changed on an already open device");
    }

    switch (interface)
    {
        case INTERFACE_ANY:
        case INTERFACE_A:
            briteblox->interface = 0;
            briteblox->index     = INTERFACE_A;
            briteblox->in_ep     = 0x02;
            briteblox->out_ep    = 0x81;
            break;
        case INTERFACE_B:
            briteblox->interface = 1;
            briteblox->index     = INTERFACE_B;
            briteblox->in_ep     = 0x04;
            briteblox->out_ep    = 0x83;
            break;
        case INTERFACE_C:
            briteblox->interface = 2;
            briteblox->index     = INTERFACE_C;
            briteblox->in_ep     = 0x06;
            briteblox->out_ep    = 0x85;
            break;
        case INTERFACE_D:
            briteblox->interface = 3;
            briteblox->index     = INTERFACE_D;
            briteblox->in_ep     = 0x08;
            briteblox->out_ep    = 0x87;
            break;
        default:
            briteblox_error_return(-1, "Unknown interface");
    }
    return 0;
}

/**
    Deinitializes a briteblox_context.

    \param briteblox pointer to briteblox_context
*/
void briteblox_deinit(struct briteblox_context *briteblox)
{
    if (briteblox == NULL)
        return;

    briteblox_usb_close_internal (briteblox);

    if (briteblox->readbuffer != NULL)
    {
        free(briteblox->readbuffer);
        briteblox->readbuffer = NULL;
    }

    if (briteblox->eeprom != NULL)
    {
        if (briteblox->eeprom->manufacturer != 0)
        {
            free(briteblox->eeprom->manufacturer);
            briteblox->eeprom->manufacturer = 0;
        }
        if (briteblox->eeprom->product != 0)
        {
            free(briteblox->eeprom->product);
            briteblox->eeprom->product = 0;
        }
        if (briteblox->eeprom->serial != 0)
        {
            free(briteblox->eeprom->serial);
            briteblox->eeprom->serial = 0;
        }
        free(briteblox->eeprom);
        briteblox->eeprom = NULL;
    }

    if (briteblox->usb_ctx)
    {
        libusb_exit(briteblox->usb_ctx);
        briteblox->usb_ctx = NULL;
    }
}

/**
    Deinitialize and free an briteblox_context.

    \param briteblox pointer to briteblox_context
*/
void briteblox_free(struct briteblox_context *briteblox)
{
    briteblox_deinit(briteblox);
    free(briteblox);
}

/**
    Use an already open libusb device.

    \param briteblox pointer to briteblox_context
    \param usb libusb libusb_device_handle to use
*/
void briteblox_set_usbdev (struct briteblox_context *briteblox, libusb_device_handle *usb)
{
    if (briteblox == NULL)
        return;

    briteblox->usb_dev = usb;
}

/**
 * @brief Get libbriteblox library version
 *
 * @return briteblox_version_info Library version information
 **/
struct briteblox_version_info briteblox_get_library_version(void)
{
    struct briteblox_version_info ver;

    ver.major = BRITEBLOX_MAJOR_VERSION;
    ver.minor = BRITEBLOX_MINOR_VERSION;
    ver.micro = BRITEBLOX_MICRO_VERSION;
    ver.version_str = BRITEBLOX_VERSION_STRING;
    ver.snapshot_str = BRITEBLOX_SNAPSHOT_VERSION;

    return ver;
}

/**
    Finds all briteblox devices with given VID:PID on the usb bus. Creates a new
    briteblox_device_list which needs to be deallocated by briteblox_list_free() after
    use.  With VID:PID 0:0, search for the default devices
    (0x403:0x6001, 0x403:0x6010, 0x403:0x6011, 0x403:0x6014)

    \param briteblox pointer to briteblox_context
    \param devlist Pointer where to store list of found devices
    \param vendor Vendor ID to search for
    \param product Product ID to search for

    \retval >0: number of devices found
    \retval -3: out of memory
    \retval -5: libusb_get_device_list() failed
    \retval -6: libusb_get_device_descriptor() failed
*/
int briteblox_usb_find_all(struct briteblox_context *briteblox, struct briteblox_device_list **devlist, int vendor, int product)
{
    struct briteblox_device_list **curdev;
    libusb_device *dev;
    libusb_device **devs;
    int count = 0;
    int i = 0;

    if (libusb_get_device_list(briteblox->usb_ctx, &devs) < 0)
        briteblox_error_return(-5, "libusb_get_device_list() failed");

    curdev = devlist;
    *curdev = NULL;

    while ((dev = devs[i++]) != NULL)
    {
        struct libusb_device_descriptor desc;

        if (libusb_get_device_descriptor(dev, &desc) < 0)
            briteblox_error_return_free_device_list(-6, "libusb_get_device_descriptor() failed", devs);

        if (((vendor != 0 && product != 0) &&
                desc.idVendor == vendor && desc.idProduct == product) ||
                ((vendor == 0 && product == 0) &&
                 (desc.idVendor == 0x403) && (desc.idProduct == 0x6001 || desc.idProduct == 0x6010
                                              || desc.idProduct == 0x6011 || desc.idProduct == 0x6014
						|| desc.idProduct==0x7AD0)))
        {
            *curdev = (struct briteblox_device_list*)malloc(sizeof(struct briteblox_device_list));
            if (!*curdev)
                briteblox_error_return_free_device_list(-3, "out of memory", devs);

            (*curdev)->next = NULL;
            (*curdev)->dev = dev;
            libusb_ref_device(dev);
            curdev = &(*curdev)->next;
            count++;
        }
    }
    libusb_free_device_list(devs,1);
    return count;
}

/**
    Frees a usb device list.

    \param devlist USB device list created by briteblox_usb_find_all()
*/
void briteblox_list_free(struct briteblox_device_list **devlist)
{
    struct briteblox_device_list *curdev, *next;

    for (curdev = *devlist; curdev != NULL;)
    {
        next = curdev->next;
        libusb_unref_device(curdev->dev);
        free(curdev);
        curdev = next;
    }

    *devlist = NULL;
}

/**
    Frees a usb device list.

    \param devlist USB device list created by briteblox_usb_find_all()
*/
void briteblox_list_free2(struct briteblox_device_list *devlist)
{
    briteblox_list_free(&devlist);
}

/**
    Return device ID strings from the usb device.

    The parameters manufacturer, description and serial may be NULL
    or pointer to buffers to store the fetched strings.

    \note Use this function only in combination with briteblox_usb_find_all()
          as it closes the internal "usb_dev" after use.

    \param briteblox pointer to briteblox_context
    \param dev libusb usb_dev to use
    \param manufacturer Store manufacturer string here if not NULL
    \param mnf_len Buffer size of manufacturer string
    \param description Store product description string here if not NULL
    \param desc_len Buffer size of product description string
    \param serial Store serial string here if not NULL
    \param serial_len Buffer size of serial string

    \retval   0: all fine
    \retval  -1: wrong arguments
    \retval  -4: unable to open device
    \retval  -7: get product manufacturer failed
    \retval  -8: get product description failed
    \retval  -9: get serial number failed
    \retval -11: libusb_get_device_descriptor() failed
*/
int briteblox_usb_get_strings(struct briteblox_context * briteblox, struct libusb_device * dev,
                         char * manufacturer, int mnf_len, char * description, int desc_len, char * serial, int serial_len)
{
    struct libusb_device_descriptor desc;

    if ((briteblox==NULL) || (dev==NULL))
        return -1;

    if (libusb_open(dev, &briteblox->usb_dev) < 0)
        briteblox_error_return(-4, "libusb_open() failed");

    if (libusb_get_device_descriptor(dev, &desc) < 0)
        briteblox_error_return(-11, "libusb_get_device_descriptor() failed");

    if (manufacturer != NULL)
    {
        if (libusb_get_string_descriptor_ascii(briteblox->usb_dev, desc.iManufacturer, (unsigned char *)manufacturer, mnf_len) < 0)
        {
            briteblox_usb_close_internal (briteblox);
            briteblox_error_return(-7, "libusb_get_string_descriptor_ascii() failed");
        }
    }

    if (description != NULL)
    {
        if (libusb_get_string_descriptor_ascii(briteblox->usb_dev, desc.iProduct, (unsigned char *)description, desc_len) < 0)
        {
            briteblox_usb_close_internal (briteblox);
            briteblox_error_return(-8, "libusb_get_string_descriptor_ascii() failed");
        }
    }

    if (serial != NULL)
    {
        if (libusb_get_string_descriptor_ascii(briteblox->usb_dev, desc.iSerialNumber, (unsigned char *)serial, serial_len) < 0)
        {
            briteblox_usb_close_internal (briteblox);
            briteblox_error_return(-9, "libusb_get_string_descriptor_ascii() failed");
        }
    }

    briteblox_usb_close_internal (briteblox);

    return 0;
}

/**
 * Internal function to determine the maximum packet size.
 * \param briteblox pointer to briteblox_context
 * \param dev libusb usb_dev to use
 * \retval Maximum packet size for this device
 */
static unsigned int _briteblox_determine_max_packet_size(struct briteblox_context *briteblox, libusb_device *dev)
{
    struct libusb_device_descriptor desc;
    struct libusb_config_descriptor *config0;
    unsigned int packet_size;

    // Sanity check
    if (briteblox == NULL || dev == NULL)
        return 64;

    // Determine maximum packet size. Init with default value.
    // New hi-speed devices from BRITEBLOX use a packet size of 512 bytes
    // but could be connected to a normal speed USB hub -> 64 bytes packet size.
    if (briteblox->type == TYPE_2232H || briteblox->type == TYPE_4232H || briteblox->type == TYPE_232H || briteblox->type == TYPE_230X)
        packet_size = 512;
    else
        packet_size = 64;

    if (libusb_get_device_descriptor(dev, &desc) < 0)
        return packet_size;

    if (libusb_get_config_descriptor(dev, 0, &config0) < 0)
        return packet_size;

    if (desc.bNumConfigurations > 0)
    {
        if (briteblox->interface < config0->bNumInterfaces)
        {
            struct libusb_interface interface = config0->interface[briteblox->interface];
            if (interface.num_altsetting > 0)
            {
                struct libusb_interface_descriptor descriptor = interface.altsetting[0];
                if (descriptor.bNumEndpoints > 0)
                {
                    packet_size = descriptor.endpoint[0].wMaxPacketSize;
                }
            }
        }
    }

    libusb_free_config_descriptor (config0);
    return packet_size;
}

/**
    Opens a briteblox device given by an usb_device.

    \param briteblox pointer to briteblox_context
    \param dev libusb usb_dev to use

    \retval  0: all fine
    \retval -3: unable to config device
    \retval -4: unable to open device
    \retval -5: unable to claim device
    \retval -6: reset failed
    \retval -7: set baudrate failed
    \retval -8: briteblox context invalid
    \retval -9: libusb_get_device_descriptor() failed
    \retval -10: libusb_get_config_descriptor() failed
    \retval -11: libusb_detach_kernel_driver() failed
    \retval -12: libusb_get_configuration() failed
*/
int briteblox_usb_open_dev(struct briteblox_context *briteblox, libusb_device *dev)
{
    struct libusb_device_descriptor desc;
    struct libusb_config_descriptor *config0;
    int cfg, cfg0, detach_errno = 0;

    if (briteblox == NULL)
        briteblox_error_return(-8, "briteblox context invalid");

    if (libusb_open(dev, &briteblox->usb_dev) < 0)
        briteblox_error_return(-4, "libusb_open() failed");

    if (libusb_get_device_descriptor(dev, &desc) < 0)
        briteblox_error_return(-9, "libusb_get_device_descriptor() failed");

    if (libusb_get_config_descriptor(dev, 0, &config0) < 0)
        briteblox_error_return(-10, "libusb_get_config_descriptor() failed");
    cfg0 = config0->bConfigurationValue;
    libusb_free_config_descriptor (config0);

    // Try to detach briteblox_sio kernel module.
    //
    // The return code is kept in a separate variable and only parsed
    // if usb_set_configuration() or usb_claim_interface() fails as the
    // detach operation might be denied and everything still works fine.
    // Likely scenario is a static briteblox_sio kernel module.
    if (briteblox->module_detach_mode == AUTO_DETACH_SIO_MODULE)
    {
        if (libusb_detach_kernel_driver(briteblox->usb_dev, briteblox->interface) !=0)
            detach_errno = errno;
    }

    if (libusb_get_configuration (briteblox->usb_dev, &cfg) < 0)
        briteblox_error_return(-12, "libusb_get_configuration () failed");
    // set configuration (needed especially for windows)
    // tolerate EBUSY: one device with one configuration, but two interfaces
    //    and libbriteblox sessions to both interfaces (e.g. FT2232)
    if (desc.bNumConfigurations > 0 && cfg != cfg0)
    {
        if (libusb_set_configuration(briteblox->usb_dev, cfg0) < 0)
        {
            briteblox_usb_close_internal (briteblox);
            if (detach_errno == EPERM)
            {
                briteblox_error_return(-8, "inappropriate permissions on device!");
            }
            else
            {
                briteblox_error_return(-3, "unable to set usb configuration. Make sure the default BRITEBLOX driver is not in use");
            }
        }
    }

    if (libusb_claim_interface(briteblox->usb_dev, briteblox->interface) < 0)
    {
        briteblox_usb_close_internal (briteblox);
        if (detach_errno == EPERM)
        {
            briteblox_error_return(-8, "inappropriate permissions on device!");
        }
        else
        {
            briteblox_error_return(-5, "unable to claim usb device. Make sure the default BRITEBLOX driver is not in use");
        }
    }

    if (briteblox_usb_reset (briteblox) != 0)
    {
        briteblox_usb_close_internal (briteblox);
        briteblox_error_return(-6, "briteblox_usb_reset failed");
    }

    // Try to guess chip type
    // Bug in the BM type chips: bcdDevice is 0x200 for serial == 0
    if (desc.bcdDevice == 0x400 || (desc.bcdDevice == 0x200
                                    && desc.iSerialNumber == 0))
        briteblox->type = TYPE_BM;
    else if (desc.bcdDevice == 0x200)
        briteblox->type = TYPE_AM;
    else if (desc.bcdDevice == 0x500)
        briteblox->type = TYPE_2232C;
    else if (desc.bcdDevice == 0x600)
        briteblox->type = TYPE_R;
    else if (desc.bcdDevice == 0x700)
        briteblox->type = TYPE_2232H;
    else if (desc.bcdDevice == 0x800)
        briteblox->type = TYPE_4232H;
    else if (desc.bcdDevice == 0x900)
        briteblox->type = TYPE_232H;
    else if (desc.bcdDevice == 0x1000)
        briteblox->type = TYPE_230X;

    // Determine maximum packet size
    briteblox->max_packet_size = _briteblox_determine_max_packet_size(briteblox, dev);

    if (briteblox_set_baudrate (briteblox, 9600) != 0)
    {
        briteblox_usb_close_internal (briteblox);
        briteblox_error_return(-7, "set baudrate failed");
    }

    briteblox_error_return(0, "all fine");
}

/**
    Opens the first device with a given vendor and product ids.

    \param briteblox pointer to briteblox_context
    \param vendor Vendor ID
    \param product Product ID

    \retval same as briteblox_usb_open_desc()
*/
int briteblox_usb_open(struct briteblox_context *briteblox, int vendor, int product)
{
    return briteblox_usb_open_desc(briteblox, vendor, product, NULL, NULL);
}

/**
    Opens the first device with a given, vendor id, product id,
    description and serial.

    \param briteblox pointer to briteblox_context
    \param vendor Vendor ID
    \param product Product ID
    \param description Description to search for. Use NULL if not needed.
    \param serial Serial to search for. Use NULL if not needed.

    \retval  0: all fine
    \retval -3: usb device not found
    \retval -4: unable to open device
    \retval -5: unable to claim device
    \retval -6: reset failed
    \retval -7: set baudrate failed
    \retval -8: get product description failed
    \retval -9: get serial number failed
    \retval -12: libusb_get_device_list() failed
    \retval -13: libusb_get_device_descriptor() failed
*/
int briteblox_usb_open_desc(struct briteblox_context *briteblox, int vendor, int product,
                       const char* description, const char* serial)
{
    return briteblox_usb_open_desc_index(briteblox,vendor,product,description,serial,0);
}

/**
    Opens the index-th device with a given, vendor id, product id,
    description and serial.

    \param briteblox pointer to briteblox_context
    \param vendor Vendor ID
    \param product Product ID
    \param description Description to search for. Use NULL if not needed.
    \param serial Serial to search for. Use NULL if not needed.
    \param index Number of matching device to open if there are more than one, starts with 0.

    \retval  0: all fine
    \retval -1: usb_find_busses() failed
    \retval -2: usb_find_devices() failed
    \retval -3: usb device not found
    \retval -4: unable to open device
    \retval -5: unable to claim device
    \retval -6: reset failed
    \retval -7: set baudrate failed
    \retval -8: get product description failed
    \retval -9: get serial number failed
    \retval -10: unable to close device
    \retval -11: briteblox context invalid
*/
int briteblox_usb_open_desc_index(struct briteblox_context *briteblox, int vendor, int product,
                             const char* description, const char* serial, unsigned int index)
{
    libusb_device *dev;
    libusb_device **devs;
    char string[256];
    int i = 0;

    if (briteblox == NULL)
        briteblox_error_return(-11, "briteblox context invalid");

    if (libusb_get_device_list(briteblox->usb_ctx, &devs) < 0)
        briteblox_error_return(-12, "libusb_get_device_list() failed");

    while ((dev = devs[i++]) != NULL)
    {
        struct libusb_device_descriptor desc;
        int res;

        if (libusb_get_device_descriptor(dev, &desc) < 0)
            briteblox_error_return_free_device_list(-13, "libusb_get_device_descriptor() failed", devs);

        if (desc.idVendor == vendor && desc.idProduct == product)
        {
            if (libusb_open(dev, &briteblox->usb_dev) < 0)
                briteblox_error_return_free_device_list(-4, "usb_open() failed", devs);

            if (description != NULL)
            {
                if (libusb_get_string_descriptor_ascii(briteblox->usb_dev, desc.iProduct, (unsigned char *)string, sizeof(string)) < 0)
                {
                    briteblox_usb_close_internal (briteblox);
                    briteblox_error_return_free_device_list(-8, "unable to fetch product description", devs);
                }
                if (strncmp(string, description, sizeof(string)) != 0)
                {
                    briteblox_usb_close_internal (briteblox);
                    continue;
                }
            }
            if (serial != NULL)
            {
                if (libusb_get_string_descriptor_ascii(briteblox->usb_dev, desc.iSerialNumber, (unsigned char *)string, sizeof(string)) < 0)
                {
                    briteblox_usb_close_internal (briteblox);
                    briteblox_error_return_free_device_list(-9, "unable to fetch serial number", devs);
                }
                if (strncmp(string, serial, sizeof(string)) != 0)
                {
                    briteblox_usb_close_internal (briteblox);
                    continue;
                }
            }

            briteblox_usb_close_internal (briteblox);

            if (index > 0)
            {
                index--;
                continue;
            }

            res = briteblox_usb_open_dev(briteblox, dev);
            libusb_free_device_list(devs,1);
            return res;
        }
    }

    // device not found
    briteblox_error_return_free_device_list(-3, "device not found", devs);
}

/**
    Opens the briteblox-device described by a description-string.
    Intended to be used for parsing a device-description given as commandline argument.

    \param briteblox pointer to briteblox_context
    \param description NULL-terminated description-string, using this format:
        \li <tt>d:\<devicenode></tt> path of bus and device-node (e.g. "003/001") within usb device tree (usually at /proc/bus/usb/)
        \li <tt>i:\<vendor>:\<product></tt> first device with given vendor and product id, ids can be decimal, octal (preceded by "0") or hex (preceded by "0x")
        \li <tt>i:\<vendor>:\<product>:\<index></tt> as above with index being the number of the device (starting with 0) if there are more than one
        \li <tt>s:\<vendor>:\<product>:\<serial></tt> first device with given vendor id, product id and serial string

    \note The description format may be extended in later versions.

    \retval  0: all fine
    \retval -2: libusb_get_device_list() failed
    \retval -3: usb device not found
    \retval -4: unable to open device
    \retval -5: unable to claim device
    \retval -6: reset failed
    \retval -7: set baudrate failed
    \retval -8: get product description failed
    \retval -9: get serial number failed
    \retval -10: unable to close device
    \retval -11: illegal description format
    \retval -12: briteblox context invalid
*/
int briteblox_usb_open_string(struct briteblox_context *briteblox, const char* description)
{
    if (briteblox == NULL)
        briteblox_error_return(-12, "briteblox context invalid");

    if (description[0] == 0 || description[1] != ':')
        briteblox_error_return(-11, "illegal description format");

    if (description[0] == 'd')
    {
        libusb_device *dev;
        libusb_device **devs;
        unsigned int bus_number, device_address;
        int i = 0;

        if (libusb_get_device_list(briteblox->usb_ctx, &devs) < 0)
            briteblox_error_return(-2, "libusb_get_device_list() failed");

        /* XXX: This doesn't handle symlinks/odd paths/etc... */
        if (sscanf (description + 2, "%u/%u", &bus_number, &device_address) != 2)
            briteblox_error_return_free_device_list(-11, "illegal description format", devs);

        while ((dev = devs[i++]) != NULL)
        {
            int ret;
            if (bus_number == libusb_get_bus_number (dev)
                    && device_address == libusb_get_device_address (dev))
            {
                ret = briteblox_usb_open_dev(briteblox, dev);
                libusb_free_device_list(devs,1);
                return ret;
            }
        }

        // device not found
        briteblox_error_return_free_device_list(-3, "device not found", devs);
    }
    else if (description[0] == 'i' || description[0] == 's')
    {
        unsigned int vendor;
        unsigned int product;
        unsigned int index=0;
        const char *serial=NULL;
        const char *startp, *endp;

        errno=0;
        startp=description+2;
        vendor=strtoul((char*)startp,(char**)&endp,0);
        if (*endp != ':' || endp == startp || errno != 0)
            briteblox_error_return(-11, "illegal description format");

        startp=endp+1;
        product=strtoul((char*)startp,(char**)&endp,0);
        if (endp == startp || errno != 0)
            briteblox_error_return(-11, "illegal description format");

        if (description[0] == 'i' && *endp != 0)
        {
            /* optional index field in i-mode */
            if (*endp != ':')
                briteblox_error_return(-11, "illegal description format");

            startp=endp+1;
            index=strtoul((char*)startp,(char**)&endp,0);
            if (*endp != 0 || endp == startp || errno != 0)
                briteblox_error_return(-11, "illegal description format");
        }
        if (description[0] == 's')
        {
            if (*endp != ':')
                briteblox_error_return(-11, "illegal description format");

            /* rest of the description is the serial */
            serial=endp+1;
        }

        return briteblox_usb_open_desc_index(briteblox, vendor, product, NULL, serial, index);
    }
    else
    {
        briteblox_error_return(-11, "illegal description format");
    }
}

/**
    Resets the briteblox device.

    \param briteblox pointer to briteblox_context

    \retval  0: all fine
    \retval -1: BRITEBLOX reset failed
    \retval -2: USB device unavailable
*/
int briteblox_usb_reset(struct briteblox_context *briteblox)
{
    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_OUT_REQTYPE,
                                SIO_RESET_REQUEST, SIO_RESET_SIO,
                                briteblox->index, NULL, 0, briteblox->usb_write_timeout) < 0)
        briteblox_error_return(-1,"BRITEBLOX reset failed");

    // Invalidate data in the readbuffer
    briteblox->readbuffer_offset = 0;
    briteblox->readbuffer_remaining = 0;

    return 0;
}

/**
    Clears the read buffer on the chip and the internal read buffer.

    \param briteblox pointer to briteblox_context

    \retval  0: all fine
    \retval -1: read buffer purge failed
    \retval -2: USB device unavailable
*/
int briteblox_usb_purge_rx_buffer(struct briteblox_context *briteblox)
{
    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_OUT_REQTYPE,
                                SIO_RESET_REQUEST, SIO_RESET_PURGE_RX,
                                briteblox->index, NULL, 0, briteblox->usb_write_timeout) < 0)
        briteblox_error_return(-1, "BRITEBLOX purge of RX buffer failed");

    // Invalidate data in the readbuffer
    briteblox->readbuffer_offset = 0;
    briteblox->readbuffer_remaining = 0;

    return 0;
}

/**
    Clears the write buffer on the chip.

    \param briteblox pointer to briteblox_context

    \retval  0: all fine
    \retval -1: write buffer purge failed
    \retval -2: USB device unavailable
*/
int briteblox_usb_purge_tx_buffer(struct briteblox_context *briteblox)
{
    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_OUT_REQTYPE,
                                SIO_RESET_REQUEST, SIO_RESET_PURGE_TX,
                                briteblox->index, NULL, 0, briteblox->usb_write_timeout) < 0)
        briteblox_error_return(-1, "BRITEBLOX purge of TX buffer failed");

    return 0;
}

/**
    Clears the buffers on the chip and the internal read buffer.

    \param briteblox pointer to briteblox_context

    \retval  0: all fine
    \retval -1: read buffer purge failed
    \retval -2: write buffer purge failed
    \retval -3: USB device unavailable
*/
int briteblox_usb_purge_buffers(struct briteblox_context *briteblox)
{
    int result;

    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-3, "USB device unavailable");

    result = briteblox_usb_purge_rx_buffer(briteblox);
    if (result < 0)
        return -1;

    result = briteblox_usb_purge_tx_buffer(briteblox);
    if (result < 0)
        return -2;

    return 0;
}



/**
    Closes the briteblox device. Call briteblox_deinit() if you're cleaning up.

    \param briteblox pointer to briteblox_context

    \retval  0: all fine
    \retval -1: usb_release failed
    \retval -3: briteblox context invalid
*/
int briteblox_usb_close(struct briteblox_context *briteblox)
{
    int rtn = 0;

    if (briteblox == NULL)
        briteblox_error_return(-3, "briteblox context invalid");

    if (briteblox->usb_dev != NULL)
        if (libusb_release_interface(briteblox->usb_dev, briteblox->interface) < 0)
            rtn = -1;

    briteblox_usb_close_internal (briteblox);

    return rtn;
}

/*  briteblox_to_clkbits_AM For the AM device, convert a requested baudrate
                    to encoded divisor and the achievable baudrate
    Function is only used internally
    \internal

    See AN120
   clk/1   -> 0
   clk/1.5 -> 1
   clk/2   -> 2
   From /2, 0.125/ 0.25 and 0.5 steps may be taken
   The fractional part has frac_code encoding
*/
static int briteblox_to_clkbits_AM(int baudrate, unsigned long *encoded_divisor)

{
    static const char frac_code[8] = {0, 3, 2, 4, 1, 5, 6, 7};
    static const char am_adjust_up[8] = {0, 0, 0, 1, 0, 3, 2, 1};
    static const char am_adjust_dn[8] = {0, 0, 0, 1, 0, 1, 2, 3};
    int divisor, best_divisor, best_baud, best_baud_diff;
    divisor = 24000000 / baudrate;
    int i;

    // Round down to supported fraction (AM only)
    divisor -= am_adjust_dn[divisor & 7];

    // Try this divisor and the one above it (because division rounds down)
    best_divisor = 0;
    best_baud = 0;
    best_baud_diff = 0;
    for (i = 0; i < 2; i++)
    {
        int try_divisor = divisor + i;
        int baud_estimate;
        int baud_diff;

        // Round up to supported divisor value
        if (try_divisor <= 8)
        {
            // Round up to minimum supported divisor
            try_divisor = 8;
        }
        else if (divisor < 16)
        {
            // AM doesn't support divisors 9 through 15 inclusive
            try_divisor = 16;
        }
        else
        {
            // Round up to supported fraction (AM only)
            try_divisor += am_adjust_up[try_divisor & 7];
            if (try_divisor > 0x1FFF8)
            {
                // Round down to maximum supported divisor value (for AM)
                try_divisor = 0x1FFF8;
            }
        }
        // Get estimated baud rate (to nearest integer)
        baud_estimate = (24000000 + (try_divisor / 2)) / try_divisor;
        // Get absolute difference from requested baud rate
        if (baud_estimate < baudrate)
        {
            baud_diff = baudrate - baud_estimate;
        }
        else
        {
            baud_diff = baud_estimate - baudrate;
        }
        if (i == 0 || baud_diff < best_baud_diff)
        {
            // Closest to requested baud rate so far
            best_divisor = try_divisor;
            best_baud = baud_estimate;
            best_baud_diff = baud_diff;
            if (baud_diff == 0)
            {
                // Spot on! No point trying
                break;
            }
        }
    }
    // Encode the best divisor value
    *encoded_divisor = (best_divisor >> 3) | (frac_code[best_divisor & 7] << 14);
    // Deal with special cases for encoded value
    if (*encoded_divisor == 1)
    {
        *encoded_divisor = 0;    // 3000000 baud
    }
    else if (*encoded_divisor == 0x4001)
    {
        *encoded_divisor = 1;    // 2000000 baud (BM only)
    }
    return best_baud;
}

/*  briteblox_to_clkbits Convert a requested baudrate for a given system clock  and predivisor
                    to encoded divisor and the achievable baudrate
    Function is only used internally
    \internal

    See AN120
   clk/1   -> 0
   clk/1.5 -> 1
   clk/2   -> 2
   From /2, 0.125 steps may be taken.
   The fractional part has frac_code encoding

   value[13:0] of value is the divisor
   index[9] mean 12 MHz Base(120 MHz/10) rate versus 3 MHz (48 MHz/16) else

   H Type have all features above with
   {index[8],value[15:14]} is the encoded subdivisor

   FT232R, FT2232 and FT232BM have no option for 12 MHz and with
   {index[0],value[15:14]} is the encoded subdivisor

   AM Type chips have only four fractional subdivisors at value[15:14]
   for subdivisors 0, 0.5, 0.25, 0.125
*/
static int briteblox_to_clkbits(int baudrate, unsigned int clk, int clk_div, unsigned long *encoded_divisor)
{
    static const char frac_code[8] = {0, 3, 2, 4, 1, 5, 6, 7};
    int best_baud = 0;
    int divisor, best_divisor;
    if (baudrate >=  clk/clk_div)
    {
        *encoded_divisor = 0;
        best_baud = clk/clk_div;
    }
    else if (baudrate >=  clk/(clk_div + clk_div/2))
    {
        *encoded_divisor = 1;
        best_baud = clk/(clk_div + clk_div/2);
    }
    else if (baudrate >=  clk/(2*clk_div))
    {
        *encoded_divisor = 2;
        best_baud = clk/(2*clk_div);
    }
    else
    {
        /* We divide by 16 to have 3 fractional bits and one bit for rounding */
        divisor = clk*16/clk_div / baudrate;
        if (divisor & 1) /* Decide if to round up or down*/
            best_divisor = divisor /2 +1;
        else
            best_divisor = divisor/2;
        if(best_divisor > 0x20000)
            best_divisor = 0x1ffff;
        best_baud = clk*16/clk_div/best_divisor;
        if (best_baud & 1) /* Decide if to round up or down*/
            best_baud = best_baud /2 +1;
        else
            best_baud = best_baud /2;
        *encoded_divisor = (best_divisor >> 3) | (frac_code[best_divisor & 0x7] << 14);
    }
    return best_baud;
}
/**
    briteblox_convert_baudrate returns nearest supported baud rate to that requested.
    Function is only used internally
    \internal
*/
static int briteblox_convert_baudrate(int baudrate, struct briteblox_context *briteblox,
                                 unsigned short *value, unsigned short *index)
{
    int best_baud;
    unsigned long encoded_divisor;

    if (baudrate <= 0)
    {
        // Return error
        return -1;
    }

#define H_CLK 120000000
#define C_CLK  48000000
    if ((briteblox->type == TYPE_2232H) || (briteblox->type == TYPE_4232H) || (briteblox->type == TYPE_232H) || (briteblox->type == TYPE_230X))
    {
        if(baudrate*10 > H_CLK /0x3fff)
        {
            /* On H Devices, use 12 000 000 Baudrate when possible
               We have a 14 bit divisor, a 1 bit divisor switch (10 or 16)
               three fractional bits and a 120 MHz clock
               Assume AN_120 "Sub-integer divisors between 0 and 2 are not allowed" holds for
               DIV/10 CLK too, so /1, /1.5 and /2 can be handled the same*/
            best_baud = briteblox_to_clkbits(baudrate, H_CLK, 10, &encoded_divisor);
            encoded_divisor |= 0x20000; /* switch on CLK/10*/
        }
        else
            best_baud = briteblox_to_clkbits(baudrate, C_CLK, 16, &encoded_divisor);
    }
    else if ((briteblox->type == TYPE_BM) || (briteblox->type == TYPE_2232C) || (briteblox->type == TYPE_R ))
    {
        best_baud = briteblox_to_clkbits(baudrate, C_CLK, 16, &encoded_divisor);
    }
    else
    {
        best_baud = briteblox_to_clkbits_AM(baudrate, &encoded_divisor);
    }
    // Split into "value" and "index" values
    *value = (unsigned short)(encoded_divisor & 0xFFFF);
    if (briteblox->type == TYPE_2232H || briteblox->type == TYPE_4232H || briteblox->type == TYPE_232H || briteblox->type == TYPE_230X)
    {
        *index = (unsigned short)(encoded_divisor >> 8);
        *index &= 0xFF00;
        *index |= briteblox->index;
    }
    else
        *index = (unsigned short)(encoded_divisor >> 16);

    // Return the nearest baud rate
    return best_baud;
}

/**
 * @brief Wrapper function to export briteblox_convert_baudrate() to the unit test
 * Do not use, it's only for the unit test framework
 **/
int convert_baudrate_UT_export(int baudrate, struct briteblox_context *briteblox,
                               unsigned short *value, unsigned short *index)
{
    return briteblox_convert_baudrate(baudrate, briteblox, value, index);
}

/**
    Sets the chip baud rate

    \param briteblox pointer to briteblox_context
    \param baudrate baud rate to set

    \retval  0: all fine
    \retval -1: invalid baudrate
    \retval -2: setting baudrate failed
    \retval -3: USB device unavailable
*/
int briteblox_set_baudrate(struct briteblox_context *briteblox, int baudrate)
{
    unsigned short value, index;
    int actual_baudrate;

    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-3, "USB device unavailable");

    if (briteblox->bitbang_enabled)
    {
        baudrate = baudrate*4;
    }

    actual_baudrate = briteblox_convert_baudrate(baudrate, briteblox, &value, &index);
    if (actual_baudrate <= 0)
        briteblox_error_return (-1, "Silly baudrate <= 0.");

    // Check within tolerance (about 5%)
    if ((actual_baudrate * 2 < baudrate /* Catch overflows */ )
            || ((actual_baudrate < baudrate)
                ? (actual_baudrate * 21 < baudrate * 20)
                : (baudrate * 21 < actual_baudrate * 20)))
        briteblox_error_return (-1, "Unsupported baudrate. Note: bitbang baudrates are automatically multiplied by 4");

    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_OUT_REQTYPE,
                                SIO_SET_BAUDRATE_REQUEST, value,
                                index, NULL, 0, briteblox->usb_write_timeout) < 0)
        briteblox_error_return (-2, "Setting new baudrate failed");

    briteblox->baudrate = baudrate;
    return 0;
}

/**
    Set (RS232) line characteristics.
    The break type can only be set via briteblox_set_line_property2()
    and defaults to "off".

    \param briteblox pointer to briteblox_context
    \param bits Number of bits
    \param sbit Number of stop bits
    \param parity Parity mode

    \retval  0: all fine
    \retval -1: Setting line property failed
*/
int briteblox_set_line_property(struct briteblox_context *briteblox, enum briteblox_bits_type bits,
                           enum briteblox_stopbits_type sbit, enum briteblox_parity_type parity)
{
    return briteblox_set_line_property2(briteblox, bits, sbit, parity, BREAK_OFF);
}

/**
    Set (RS232) line characteristics

    \param briteblox pointer to briteblox_context
    \param bits Number of bits
    \param sbit Number of stop bits
    \param parity Parity mode
    \param break_type Break type

    \retval  0: all fine
    \retval -1: Setting line property failed
    \retval -2: USB device unavailable
*/
int briteblox_set_line_property2(struct briteblox_context *briteblox, enum briteblox_bits_type bits,
                            enum briteblox_stopbits_type sbit, enum briteblox_parity_type parity,
                            enum briteblox_break_type break_type)
{
    unsigned short value = bits;

    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    switch (parity)
    {
        case NONE:
            value |= (0x00 << 8);
            break;
        case ODD:
            value |= (0x01 << 8);
            break;
        case EVEN:
            value |= (0x02 << 8);
            break;
        case MARK:
            value |= (0x03 << 8);
            break;
        case SPACE:
            value |= (0x04 << 8);
            break;
    }

    switch (sbit)
    {
        case STOP_BIT_1:
            value |= (0x00 << 11);
            break;
        case STOP_BIT_15:
            value |= (0x01 << 11);
            break;
        case STOP_BIT_2:
            value |= (0x02 << 11);
            break;
    }

    switch (break_type)
    {
        case BREAK_OFF:
            value |= (0x00 << 14);
            break;
        case BREAK_ON:
            value |= (0x01 << 14);
            break;
    }

    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_OUT_REQTYPE,
                                SIO_SET_DATA_REQUEST, value,
                                briteblox->index, NULL, 0, briteblox->usb_write_timeout) < 0)
        briteblox_error_return (-1, "Setting new line property failed");

    return 0;
}

/**
    Writes data in chunks (see briteblox_write_data_set_chunksize()) to the chip

    \param briteblox pointer to briteblox_context
    \param buf Buffer with the data
    \param size Size of the buffer

    \retval -666: USB device unavailable
    \retval <0: error code from usb_bulk_write()
    \retval >0: number of bytes written
*/
int briteblox_write_data(struct briteblox_context *briteblox, const unsigned char *buf, int size)
{
    int offset = 0;
    int actual_length;

    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-666, "USB device unavailable");

    while (offset < size)
    {
        int write_size = briteblox->writebuffer_chunksize;

        if (offset+write_size > size)
            write_size = size-offset;

        if (libusb_bulk_transfer(briteblox->usb_dev, briteblox->in_ep, (unsigned char *)buf+offset, write_size, &actual_length, briteblox->usb_write_timeout) < 0)
            briteblox_error_return(-1, "usb bulk write failed");

        offset += actual_length;
    }

    return offset;
}

static void briteblox_read_data_cb(struct libusb_transfer *transfer)
{
    struct briteblox_transfer_control *tc = (struct briteblox_transfer_control *) transfer->user_data;
    struct briteblox_context *briteblox = tc->briteblox;
    int packet_size, actual_length, num_of_chunks, chunk_remains, i, ret;

    packet_size = briteblox->max_packet_size;

    actual_length = transfer->actual_length;

    if (actual_length > 2)
    {
        // skip BRITEBLOX status bytes.
        // Maybe stored in the future to enable modem use
        num_of_chunks = actual_length / packet_size;
        chunk_remains = actual_length % packet_size;
        //printf("actual_length = %X, num_of_chunks = %X, chunk_remains = %X, readbuffer_offset = %X\n", actual_length, num_of_chunks, chunk_remains, briteblox->readbuffer_offset);

        briteblox->readbuffer_offset += 2;
        actual_length -= 2;

        if (actual_length > packet_size - 2)
        {
            for (i = 1; i < num_of_chunks; i++)
                memmove (briteblox->readbuffer+briteblox->readbuffer_offset+(packet_size - 2)*i,
                         briteblox->readbuffer+briteblox->readbuffer_offset+packet_size*i,
                         packet_size - 2);
            if (chunk_remains > 2)
            {
                memmove (briteblox->readbuffer+briteblox->readbuffer_offset+(packet_size - 2)*i,
                         briteblox->readbuffer+briteblox->readbuffer_offset+packet_size*i,
                         chunk_remains-2);
                actual_length -= 2*num_of_chunks;
            }
            else
                actual_length -= 2*(num_of_chunks-1)+chunk_remains;
        }

        if (actual_length > 0)
        {
            // data still fits in buf?
            if (tc->offset + actual_length <= tc->size)
            {
                memcpy (tc->buf + tc->offset, briteblox->readbuffer + briteblox->readbuffer_offset, actual_length);
                //printf("buf[0] = %X, buf[1] = %X\n", buf[0], buf[1]);
                tc->offset += actual_length;

                briteblox->readbuffer_offset = 0;
                briteblox->readbuffer_remaining = 0;

                /* Did we read exactly the right amount of bytes? */
                if (tc->offset == tc->size)
                {
                    //printf("read_data exact rem %d offset %d\n",
                    //briteblox->readbuffer_remaining, offset);
                    tc->completed = 1;
                    return;
                }
            }
            else
            {
                // only copy part of the data or size <= readbuffer_chunksize
                int part_size = tc->size - tc->offset;
                memcpy (tc->buf + tc->offset, briteblox->readbuffer + briteblox->readbuffer_offset, part_size);
                tc->offset += part_size;

                briteblox->readbuffer_offset += part_size;
                briteblox->readbuffer_remaining = actual_length - part_size;

                /* printf("Returning part: %d - size: %d - offset: %d - actual_length: %d - remaining: %d\n",
                part_size, size, offset, actual_length, briteblox->readbuffer_remaining); */
                tc->completed = 1;
                return;
            }
        }
    }
    ret = libusb_submit_transfer (transfer);
    if (ret < 0)
        tc->completed = 1;
}


static void briteblox_write_data_cb(struct libusb_transfer *transfer)
{
    struct briteblox_transfer_control *tc = (struct briteblox_transfer_control *) transfer->user_data;
    struct briteblox_context *briteblox = tc->briteblox;

    tc->offset += transfer->actual_length;

    if (tc->offset == tc->size)
    {
        tc->completed = 1;
    }
    else
    {
        int write_size = briteblox->writebuffer_chunksize;
        int ret;

        if (tc->offset + write_size > tc->size)
            write_size = tc->size - tc->offset;

        transfer->length = write_size;
        transfer->buffer = tc->buf + tc->offset;
        ret = libusb_submit_transfer (transfer);
        if (ret < 0)
            tc->completed = 1;
    }
}


/**
    Writes data to the chip. Does not wait for completion of the transfer
    nor does it make sure that the transfer was successful.

    Use libusb 1.0 asynchronous API.

    \param briteblox pointer to briteblox_context
    \param buf Buffer with the data
    \param size Size of the buffer

    \retval NULL: Some error happens when submit transfer
    \retval !NULL: Pointer to a briteblox_transfer_control
*/

struct briteblox_transfer_control *briteblox_write_data_submit(struct briteblox_context *briteblox, unsigned char *buf, int size)
{
    struct briteblox_transfer_control *tc;
    struct libusb_transfer *transfer;
    int write_size, ret;

    if (briteblox == NULL || briteblox->usb_dev == NULL)
        return NULL;

    tc = (struct briteblox_transfer_control *) malloc (sizeof (*tc));
    if (!tc)
        return NULL;

    transfer = libusb_alloc_transfer(0);
    if (!transfer)
    {
        free(tc);
        return NULL;
    }

    tc->briteblox = briteblox;
    tc->completed = 0;
    tc->buf = buf;
    tc->size = size;
    tc->offset = 0;

    if (size < (int)briteblox->writebuffer_chunksize)
        write_size = size;
    else
        write_size = briteblox->writebuffer_chunksize;

    libusb_fill_bulk_transfer(transfer, briteblox->usb_dev, briteblox->in_ep, buf,
                              write_size, briteblox_write_data_cb, tc,
                              briteblox->usb_write_timeout);
    transfer->type = LIBUSB_TRANSFER_TYPE_BULK;

    ret = libusb_submit_transfer(transfer);
    if (ret < 0)
    {
        libusb_free_transfer(transfer);
        free(tc);
        return NULL;
    }
    tc->transfer = transfer;

    return tc;
}

/**
    Reads data from the chip. Does not wait for completion of the transfer
    nor does it make sure that the transfer was successful.

    Use libusb 1.0 asynchronous API.

    \param briteblox pointer to briteblox_context
    \param buf Buffer with the data
    \param size Size of the buffer

    \retval NULL: Some error happens when submit transfer
    \retval !NULL: Pointer to a briteblox_transfer_control
*/

struct briteblox_transfer_control *briteblox_read_data_submit(struct briteblox_context *briteblox, unsigned char *buf, int size)
{
    struct briteblox_transfer_control *tc;
    struct libusb_transfer *transfer;
    int ret;

    if (briteblox == NULL || briteblox->usb_dev == NULL)
        return NULL;

    tc = (struct briteblox_transfer_control *) malloc (sizeof (*tc));
    if (!tc)
        return NULL;

    tc->briteblox = briteblox;
    tc->buf = buf;
    tc->size = size;

    if (size <= (int)briteblox->readbuffer_remaining)
    {
        memcpy (buf, briteblox->readbuffer+briteblox->readbuffer_offset, size);

        // Fix offsets
        briteblox->readbuffer_remaining -= size;
        briteblox->readbuffer_offset += size;

        /* printf("Returning bytes from buffer: %d - remaining: %d\n", size, briteblox->readbuffer_remaining); */

        tc->completed = 1;
        tc->offset = size;
        tc->transfer = NULL;
        return tc;
    }

    tc->completed = 0;
    if (briteblox->readbuffer_remaining != 0)
    {
        memcpy (buf, briteblox->readbuffer+briteblox->readbuffer_offset, briteblox->readbuffer_remaining);

        tc->offset = briteblox->readbuffer_remaining;
    }
    else
        tc->offset = 0;

    transfer = libusb_alloc_transfer(0);
    if (!transfer)
    {
        free (tc);
        return NULL;
    }

    briteblox->readbuffer_remaining = 0;
    briteblox->readbuffer_offset = 0;

    libusb_fill_bulk_transfer(transfer, briteblox->usb_dev, briteblox->out_ep, briteblox->readbuffer, briteblox->readbuffer_chunksize, briteblox_read_data_cb, tc, briteblox->usb_read_timeout);
    transfer->type = LIBUSB_TRANSFER_TYPE_BULK;

    ret = libusb_submit_transfer(transfer);
    if (ret < 0)
    {
        libusb_free_transfer(transfer);
        free (tc);
        return NULL;
    }
    tc->transfer = transfer;

    return tc;
}

/**
    Wait for completion of the transfer.

    Use libusb 1.0 asynchronous API.

    \param tc pointer to briteblox_transfer_control

    \retval < 0: Some error happens
    \retval >= 0: Data size transferred
*/

int briteblox_transfer_data_done(struct briteblox_transfer_control *tc)
{
    int ret;

    while (!tc->completed)
    {
        ret = libusb_handle_events(tc->briteblox->usb_ctx);
        if (ret < 0)
        {
            if (ret == LIBUSB_ERROR_INTERRUPTED)
                continue;
            libusb_cancel_transfer(tc->transfer);
            while (!tc->completed)
                if (libusb_handle_events(tc->briteblox->usb_ctx) < 0)
                    break;
            libusb_free_transfer(tc->transfer);
            free (tc);
            return ret;
        }
    }

    ret = tc->offset;
    /**
     * tc->transfer could be NULL if "(size <= briteblox->readbuffer_remaining)"
     * at briteblox_read_data_submit(). Therefore, we need to check it here.
     **/
    if (tc->transfer)
    {
        if (tc->transfer->status != LIBUSB_TRANSFER_COMPLETED)
            ret = -1;
        libusb_free_transfer(tc->transfer);
    }
    free(tc);
    return ret;
}

/**
    Configure write buffer chunk size.
    Default is 4096.

    \param briteblox pointer to briteblox_context
    \param chunksize Chunk size

    \retval 0: all fine
    \retval -1: briteblox context invalid
*/
int briteblox_write_data_set_chunksize(struct briteblox_context *briteblox, unsigned int chunksize)
{
    if (briteblox == NULL)
        briteblox_error_return(-1, "briteblox context invalid");

    briteblox->writebuffer_chunksize = chunksize;
    return 0;
}

/**
    Get write buffer chunk size.

    \param briteblox pointer to briteblox_context
    \param chunksize Pointer to store chunk size in

    \retval 0: all fine
    \retval -1: briteblox context invalid
*/
int briteblox_write_data_get_chunksize(struct briteblox_context *briteblox, unsigned int *chunksize)
{
    if (briteblox == NULL)
        briteblox_error_return(-1, "briteblox context invalid");

    *chunksize = briteblox->writebuffer_chunksize;
    return 0;
}

/**
    Reads data in chunks (see briteblox_read_data_set_chunksize()) from the chip.

    Automatically strips the two modem status bytes transfered during every read.

    \param briteblox pointer to briteblox_context
    \param buf Buffer to store data in
    \param size Size of the buffer

    \retval -666: USB device unavailable
    \retval <0: error code from libusb_bulk_transfer()
    \retval  0: no data was available
    \retval >0: number of bytes read

*/
int briteblox_read_data(struct briteblox_context *briteblox, unsigned char *buf, int size)
{
    int offset = 0, ret, i, num_of_chunks, chunk_remains;
    int packet_size = briteblox->max_packet_size;
    int actual_length = 1;

    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-666, "USB device unavailable");

    // Packet size sanity check (avoid division by zero)
    if (packet_size == 0)
        briteblox_error_return(-1, "max_packet_size is bogus (zero)");

    // everything we want is still in the readbuffer?
    if (size <= (int)briteblox->readbuffer_remaining)
    {
        memcpy (buf, briteblox->readbuffer+briteblox->readbuffer_offset, size);

        // Fix offsets
        briteblox->readbuffer_remaining -= size;
        briteblox->readbuffer_offset += size;

        /* printf("Returning bytes from buffer: %d - remaining: %d\n", size, briteblox->readbuffer_remaining); */

        return size;
    }
    // something still in the readbuffer, but not enough to satisfy 'size'?
    if (briteblox->readbuffer_remaining != 0)
    {
        memcpy (buf, briteblox->readbuffer+briteblox->readbuffer_offset, briteblox->readbuffer_remaining);

        // Fix offset
        offset += briteblox->readbuffer_remaining;
    }
    // do the actual USB read
    while (offset < size && actual_length > 0)
    {
        briteblox->readbuffer_remaining = 0;
        briteblox->readbuffer_offset = 0;
        /* returns how much received */
        ret = libusb_bulk_transfer (briteblox->usb_dev, briteblox->out_ep, briteblox->readbuffer, briteblox->readbuffer_chunksize, &actual_length, briteblox->usb_read_timeout);
        if (ret < 0)
            briteblox_error_return(ret, "usb bulk read failed");

        if (actual_length > 2)
        {
            // skip BRITEBLOX status bytes.
            // Maybe stored in the future to enable modem use
            num_of_chunks = actual_length / packet_size;
            chunk_remains = actual_length % packet_size;
            //printf("actual_length = %X, num_of_chunks = %X, chunk_remains = %X, readbuffer_offset = %X\n", actual_length, num_of_chunks, chunk_remains, briteblox->readbuffer_offset);

            briteblox->readbuffer_offset += 2;
            actual_length -= 2;

            if (actual_length > packet_size - 2)
            {
                for (i = 1; i < num_of_chunks; i++)
                    memmove (briteblox->readbuffer+briteblox->readbuffer_offset+(packet_size - 2)*i,
                             briteblox->readbuffer+briteblox->readbuffer_offset+packet_size*i,
                             packet_size - 2);
                if (chunk_remains > 2)
                {
                    memmove (briteblox->readbuffer+briteblox->readbuffer_offset+(packet_size - 2)*i,
                             briteblox->readbuffer+briteblox->readbuffer_offset+packet_size*i,
                             chunk_remains-2);
                    actual_length -= 2*num_of_chunks;
                }
                else
                    actual_length -= 2*(num_of_chunks-1)+chunk_remains;
            }
        }
        else if (actual_length <= 2)
        {
            // no more data to read?
            return offset;
        }
        if (actual_length > 0)
        {
            // data still fits in buf?
            if (offset+actual_length <= size)
            {
                memcpy (buf+offset, briteblox->readbuffer+briteblox->readbuffer_offset, actual_length);
                //printf("buf[0] = %X, buf[1] = %X\n", buf[0], buf[1]);
                offset += actual_length;

                /* Did we read exactly the right amount of bytes? */
                if (offset == size)
                    //printf("read_data exact rem %d offset %d\n",
                    //briteblox->readbuffer_remaining, offset);
                    return offset;
            }
            else
            {
                // only copy part of the data or size <= readbuffer_chunksize
                int part_size = size-offset;
                memcpy (buf+offset, briteblox->readbuffer+briteblox->readbuffer_offset, part_size);

                briteblox->readbuffer_offset += part_size;
                briteblox->readbuffer_remaining = actual_length-part_size;
                offset += part_size;

                /* printf("Returning part: %d - size: %d - offset: %d - actual_length: %d - remaining: %d\n",
                part_size, size, offset, actual_length, briteblox->readbuffer_remaining); */

                return offset;
            }
        }
    }
    // never reached
    return -127;
}

/**
    Configure read buffer chunk size.
    Default is 4096.

    Automatically reallocates the buffer.

    \param briteblox pointer to briteblox_context
    \param chunksize Chunk size

    \retval 0: all fine
    \retval -1: briteblox context invalid
*/
int briteblox_read_data_set_chunksize(struct briteblox_context *briteblox, unsigned int chunksize)
{
    unsigned char *new_buf;

    if (briteblox == NULL)
        briteblox_error_return(-1, "briteblox context invalid");

    // Invalidate all remaining data
    briteblox->readbuffer_offset = 0;
    briteblox->readbuffer_remaining = 0;
#ifdef __linux__
    /* We can't set readbuffer_chunksize larger than MAX_BULK_BUFFER_LENGTH,
       which is defined in libusb-1.0.  Otherwise, each USB read request will
       be divided into multiple URBs.  This will cause issues on Linux kernel
       older than 2.6.32.  */
    if (chunksize > 16384)
        chunksize = 16384;
#endif

    if ((new_buf = (unsigned char *)realloc(briteblox->readbuffer, chunksize)) == NULL)
        briteblox_error_return(-1, "out of memory for readbuffer");

    briteblox->readbuffer = new_buf;
    briteblox->readbuffer_chunksize = chunksize;

    return 0;
}

/**
    Get read buffer chunk size.

    \param briteblox pointer to briteblox_context
    \param chunksize Pointer to store chunk size in

    \retval 0: all fine
    \retval -1: BRITEBLOX context invalid
*/
int briteblox_read_data_get_chunksize(struct briteblox_context *briteblox, unsigned int *chunksize)
{
    if (briteblox == NULL)
        briteblox_error_return(-1, "BRITEBLOX context invalid");

    *chunksize = briteblox->readbuffer_chunksize;
    return 0;
}

/**
    Enable/disable bitbang modes.

    \param briteblox pointer to briteblox_context
    \param bitmask Bitmask to configure lines.
           HIGH/ON value configures a line as output.
    \param mode Bitbang mode: use the values defined in \ref briteblox_mpsse_mode

    \retval  0: all fine
    \retval -1: can't enable bitbang mode
    \retval -2: USB device unavailable
*/
int briteblox_set_bitmode(struct briteblox_context *briteblox, unsigned char bitmask, unsigned char mode)
{
    unsigned short usb_val;

    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    usb_val = bitmask; // low byte: bitmask
    usb_val |= (mode << 8);
    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_OUT_REQTYPE, SIO_SET_BITMODE_REQUEST, usb_val, briteblox->index, NULL, 0, briteblox->usb_write_timeout) < 0)
        briteblox_error_return(-1, "unable to configure bitbang mode. Perhaps not a BM/2232C type chip?");

    briteblox->bitbang_mode = mode;
    briteblox->bitbang_enabled = (mode == BITMODE_RESET) ? 0 : 1;
    return 0;
}

/**
    Disable bitbang mode.

    \param briteblox pointer to briteblox_context

    \retval  0: all fine
    \retval -1: can't disable bitbang mode
    \retval -2: USB device unavailable
*/
int briteblox_disable_bitbang(struct briteblox_context *briteblox)
{
    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_OUT_REQTYPE, SIO_SET_BITMODE_REQUEST, 0, briteblox->index, NULL, 0, briteblox->usb_write_timeout) < 0)
        briteblox_error_return(-1, "unable to leave bitbang mode. Perhaps not a BM type chip?");

    briteblox->bitbang_enabled = 0;
    return 0;
}


/**
    Directly read pin state, circumventing the read buffer. Useful for bitbang mode.

    \param briteblox pointer to briteblox_context
    \param pins Pointer to store pins into

    \retval  0: all fine
    \retval -1: read pins failed
    \retval -2: USB device unavailable
*/
int briteblox_read_pins(struct briteblox_context *briteblox, unsigned char *pins)
{
    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_IN_REQTYPE, SIO_READ_PINS_REQUEST, 0, briteblox->index, (unsigned char *)pins, 1, briteblox->usb_read_timeout) != 1)
        briteblox_error_return(-1, "read pins failed");

    return 0;
}

/**
    Set latency timer

    The BRITEBLOX chip keeps data in the internal buffer for a specific
    amount of time if the buffer is not full yet to decrease
    load on the usb bus.

    \param briteblox pointer to briteblox_context
    \param latency Value between 1 and 255

    \retval  0: all fine
    \retval -1: latency out of range
    \retval -2: unable to set latency timer
    \retval -3: USB device unavailable
*/
int briteblox_set_latency_timer(struct briteblox_context *briteblox, unsigned char latency)
{
    unsigned short usb_val;

    if (latency < 1)
        briteblox_error_return(-1, "latency out of range. Only valid for 1-255");

    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-3, "USB device unavailable");

    usb_val = latency;
    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_OUT_REQTYPE, SIO_SET_LATENCY_TIMER_REQUEST, usb_val, briteblox->index, NULL, 0, briteblox->usb_write_timeout) < 0)
        briteblox_error_return(-2, "unable to set latency timer");

    return 0;
}

/**
    Get latency timer

    \param briteblox pointer to briteblox_context
    \param latency Pointer to store latency value in

    \retval  0: all fine
    \retval -1: unable to get latency timer
    \retval -2: USB device unavailable
*/
int briteblox_get_latency_timer(struct briteblox_context *briteblox, unsigned char *latency)
{
    unsigned short usb_val;

    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_IN_REQTYPE, SIO_GET_LATENCY_TIMER_REQUEST, 0, briteblox->index, (unsigned char *)&usb_val, 1, briteblox->usb_read_timeout) != 1)
        briteblox_error_return(-1, "reading latency timer failed");

    *latency = (unsigned char)usb_val;
    return 0;
}

/**
    Poll modem status information

    This function allows the retrieve the two status bytes of the device.
    The device sends these bytes also as a header for each read access
    where they are discarded by briteblox_read_data(). The chip generates
    the two stripped status bytes in the absence of data every 40 ms.

    Layout of the first byte:
    - B0..B3 - must be 0
    - B4       Clear to send (CTS)
                 0 = inactive
                 1 = active
    - B5       Data set ready (DTS)
                 0 = inactive
                 1 = active
    - B6       Ring indicator (RI)
                 0 = inactive
                 1 = active
    - B7       Receive line signal detect (RLSD)
                 0 = inactive
                 1 = active

    Layout of the second byte:
    - B0       Data ready (DR)
    - B1       Overrun error (OE)
    - B2       Parity error (PE)
    - B3       Framing error (FE)
    - B4       Break interrupt (BI)
    - B5       Transmitter holding register (THRE)
    - B6       Transmitter empty (TEMT)
    - B7       Error in RCVR FIFO

    \param briteblox pointer to briteblox_context
    \param status Pointer to store status information in. Must be two bytes.

    \retval  0: all fine
    \retval -1: unable to retrieve status information
    \retval -2: USB device unavailable
*/
int briteblox_poll_modem_status(struct briteblox_context *briteblox, unsigned short *status)
{
    char usb_val[2];

    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_IN_REQTYPE, SIO_POLL_MODEM_STATUS_REQUEST, 0, briteblox->index, (unsigned char *)usb_val, 2, briteblox->usb_read_timeout) != 2)
        briteblox_error_return(-1, "getting modem status failed");

    *status = (usb_val[1] << 8) | (usb_val[0] & 0xFF);

    return 0;
}

/**
    Set flowcontrol for briteblox chip

    \param briteblox pointer to briteblox_context
    \param flowctrl flow control to use. should be
           SIO_DISABLE_FLOW_CTRL, SIO_RTS_CTS_HS, SIO_DTR_DSR_HS or SIO_XON_XOFF_HS

    \retval  0: all fine
    \retval -1: set flow control failed
    \retval -2: USB device unavailable
*/
int briteblox_setflowctrl(struct briteblox_context *briteblox, int flowctrl)
{
    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_OUT_REQTYPE,
                                SIO_SET_FLOW_CTRL_REQUEST, 0, (flowctrl | briteblox->index),
                                NULL, 0, briteblox->usb_write_timeout) < 0)
        briteblox_error_return(-1, "set flow control failed");

    return 0;
}

/**
    Set dtr line

    \param briteblox pointer to briteblox_context
    \param state state to set line to (1 or 0)

    \retval  0: all fine
    \retval -1: set dtr failed
    \retval -2: USB device unavailable
*/
int briteblox_setdtr(struct briteblox_context *briteblox, int state)
{
    unsigned short usb_val;

    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    if (state)
        usb_val = SIO_SET_DTR_HIGH;
    else
        usb_val = SIO_SET_DTR_LOW;

    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_OUT_REQTYPE,
                                SIO_SET_MODEM_CTRL_REQUEST, usb_val, briteblox->index,
                                NULL, 0, briteblox->usb_write_timeout) < 0)
        briteblox_error_return(-1, "set dtr failed");

    return 0;
}

/**
    Set rts line

    \param briteblox pointer to briteblox_context
    \param state state to set line to (1 or 0)

    \retval  0: all fine
    \retval -1: set rts failed
    \retval -2: USB device unavailable
*/
int briteblox_setrts(struct briteblox_context *briteblox, int state)
{
    unsigned short usb_val;

    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    if (state)
        usb_val = SIO_SET_RTS_HIGH;
    else
        usb_val = SIO_SET_RTS_LOW;

    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_OUT_REQTYPE,
                                SIO_SET_MODEM_CTRL_REQUEST, usb_val, briteblox->index,
                                NULL, 0, briteblox->usb_write_timeout) < 0)
        briteblox_error_return(-1, "set of rts failed");

    return 0;
}

/**
    Set dtr and rts line in one pass

    \param briteblox pointer to briteblox_context
    \param dtr  DTR state to set line to (1 or 0)
    \param rts  RTS state to set line to (1 or 0)

    \retval  0: all fine
    \retval -1: set dtr/rts failed
    \retval -2: USB device unavailable
 */
int briteblox_setdtr_rts(struct briteblox_context *briteblox, int dtr, int rts)
{
    unsigned short usb_val;

    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    if (dtr)
        usb_val = SIO_SET_DTR_HIGH;
    else
        usb_val = SIO_SET_DTR_LOW;

    if (rts)
        usb_val |= SIO_SET_RTS_HIGH;
    else
        usb_val |= SIO_SET_RTS_LOW;

    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_OUT_REQTYPE,
                                SIO_SET_MODEM_CTRL_REQUEST, usb_val, briteblox->index,
                                NULL, 0, briteblox->usb_write_timeout) < 0)
        briteblox_error_return(-1, "set of rts/dtr failed");

    return 0;
}

/**
    Set the special event character

    \param briteblox pointer to briteblox_context
    \param eventch Event character
    \param enable 0 to disable the event character, non-zero otherwise

    \retval  0: all fine
    \retval -1: unable to set event character
    \retval -2: USB device unavailable
*/
int briteblox_set_event_char(struct briteblox_context *briteblox,
                        unsigned char eventch, unsigned char enable)
{
    unsigned short usb_val;

    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    usb_val = eventch;
    if (enable)
        usb_val |= 1 << 8;

    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_OUT_REQTYPE, SIO_SET_EVENT_CHAR_REQUEST, usb_val, briteblox->index, NULL, 0, briteblox->usb_write_timeout) < 0)
        briteblox_error_return(-1, "setting event character failed");

    return 0;
}

/**
    Set error character

    \param briteblox pointer to briteblox_context
    \param errorch Error character
    \param enable 0 to disable the error character, non-zero otherwise

    \retval  0: all fine
    \retval -1: unable to set error character
    \retval -2: USB device unavailable
*/
int briteblox_set_error_char(struct briteblox_context *briteblox,
                        unsigned char errorch, unsigned char enable)
{
    unsigned short usb_val;

    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    usb_val = errorch;
    if (enable)
        usb_val |= 1 << 8;

    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_OUT_REQTYPE, SIO_SET_ERROR_CHAR_REQUEST, usb_val, briteblox->index, NULL, 0, briteblox->usb_write_timeout) < 0)
        briteblox_error_return(-1, "setting error character failed");

    return 0;
}

/**
    Init eeprom with default values for the connected device
    \param briteblox pointer to briteblox_context
    \param manufacturer String to use as Manufacturer
    \param product String to use as Product description
    \param serial String to use as Serial number description

    \retval  0: all fine
    \retval -1: No struct briteblox_context
    \retval -2: No struct briteblox_eeprom
    \retval -3: No connected device or device not yet opened
*/
int briteblox_eeprom_initdefaults(struct briteblox_context *briteblox, char * manufacturer,
                             char * product, char * serial)
{
    struct briteblox_eeprom *eeprom;

    if (briteblox == NULL)
        briteblox_error_return(-1, "No struct briteblox_context");

    if (briteblox->eeprom == NULL)
        briteblox_error_return(-2,"No struct briteblox_eeprom");

    eeprom = briteblox->eeprom;
    memset(eeprom, 0, sizeof(struct briteblox_eeprom));

    if (briteblox->usb_dev == NULL)
        briteblox_error_return(-3, "No connected device or device not yet opened");

    eeprom->vendor_id = 0x0403;
    eeprom->use_serial = 1;
    if ((briteblox->type == TYPE_AM) || (briteblox->type == TYPE_BM) ||
            (briteblox->type == TYPE_R))
        eeprom->product_id = 0x6001;
    else if (briteblox->type == TYPE_4232H)
        eeprom->product_id = 0x6011;
    else if (briteblox->type == TYPE_232H)
        eeprom->product_id = 0x6014;
    else if (briteblox->type == TYPE_230X)
        eeprom->product_id = 0x6015;
    else
        eeprom->product_id = 0x6010;

    if (briteblox->type == TYPE_AM)
        eeprom->usb_version = 0x0101;
    else
        eeprom->usb_version = 0x0200;
    eeprom->max_power = 100;

    if (eeprom->manufacturer)
        free (eeprom->manufacturer);
    eeprom->manufacturer = NULL;
    if (manufacturer)
    {
        eeprom->manufacturer = malloc(strlen(manufacturer)+1);
        if (eeprom->manufacturer)
            strcpy(eeprom->manufacturer, manufacturer);
    }

    if (eeprom->product)
        free (eeprom->product);
    eeprom->product = NULL;
    if(product)
    {
        eeprom->product = malloc(strlen(product)+1);
        if (eeprom->product)
            strcpy(eeprom->product, product);
    }
    else
    {
        const char* default_product;
        switch(briteblox->type)
        {
            case TYPE_AM:    default_product = "AM"; break;
            case TYPE_BM:    default_product = "BM"; break;
            case TYPE_2232C: default_product = "Dual RS232"; break;
            case TYPE_R:     default_product = "FT232R USB UART"; break;
            case TYPE_2232H: default_product = "Dual RS232-HS"; break;
            case TYPE_4232H: default_product = "FT4232H"; break;
            case TYPE_232H:  default_product = "Single-RS232-HS"; break;
            case TYPE_230X:  default_product = "FT230X Basic UART"; break;
            default:
                briteblox_error_return(-3, "Unknown chip type");
        }
        eeprom->product = malloc(strlen(default_product) +1);
        if (eeprom->product)
            strcpy(eeprom->product, default_product);
    }

    if (eeprom->serial)
        free (eeprom->serial);
    eeprom->serial = NULL;
    if (serial)
    {
        eeprom->serial = malloc(strlen(serial)+1);
        if (eeprom->serial)
            strcpy(eeprom->serial, serial);
    }

    if (briteblox->type == TYPE_R)
    {
        eeprom->max_power = 90;
        eeprom->size = 0x80;
        eeprom->cbus_function[0] = CBUS_TXLED;
        eeprom->cbus_function[1] = CBUS_RXLED;
        eeprom->cbus_function[2] = CBUS_TXDEN;
        eeprom->cbus_function[3] = CBUS_PWREN;
        eeprom->cbus_function[4] = CBUS_SLEEP;
    }
    else if (briteblox->type == TYPE_230X)
    {
        eeprom->max_power = 90;
        eeprom->size = 0x100;
        eeprom->cbus_function[0] = CBUSH_TXDEN;
        eeprom->cbus_function[1] = CBUSH_RXLED;
        eeprom->cbus_function[2] = CBUSH_TXLED;
        eeprom->cbus_function[3] = CBUSH_SLEEP;
    }
    else
    {
        if(briteblox->type == TYPE_232H)
        {
            int i;
            for (i=0; i<10; i++)
                eeprom->cbus_function[i] = CBUSH_TRISTATE;
        }
        eeprom->size = -1;
    }
    switch (briteblox->type)
    {
        case TYPE_AM:
            eeprom->release_number = 0x0200;
            break;
        case TYPE_BM:
            eeprom->release_number = 0x0400;
            break;
        case TYPE_2232C:
            eeprom->release_number = 0x0500;
            break;
        case TYPE_R:
            eeprom->release_number = 0x0600;
            break;
        case TYPE_2232H:
            eeprom->release_number = 0x0700;
            break;
        case TYPE_4232H:
            eeprom->release_number = 0x0800;
            break;
        case TYPE_232H:
            eeprom->release_number = 0x0900;
            break;
        case TYPE_230X:
            eeprom->release_number = 0x1000;
            break;
        default:
            eeprom->release_number = 0x00;
    }
    return 0;
}

int briteblox_eeprom_set_strings(struct briteblox_context *briteblox, char * manufacturer,
                            char * product, char * serial)
{
    struct briteblox_eeprom *eeprom;

    if (briteblox == NULL)
        briteblox_error_return(-1, "No struct briteblox_context");

    if (briteblox->eeprom == NULL)
        briteblox_error_return(-2,"No struct briteblox_eeprom");

    eeprom = briteblox->eeprom;

    if (briteblox->usb_dev == NULL)
        briteblox_error_return(-3, "No connected device or device not yet opened");

    if (manufacturer)
    {
        if (eeprom->manufacturer)
            free (eeprom->manufacturer);
        eeprom->manufacturer = malloc(strlen(manufacturer)+1);
        if (eeprom->manufacturer)
            strcpy(eeprom->manufacturer, manufacturer);
    }

    if(product)
    {
        if (eeprom->product)
            free (eeprom->product);
        eeprom->product = malloc(strlen(product)+1);
        if (eeprom->product)
            strcpy(eeprom->product, product);
    }

    if (serial)
    {
        if (eeprom->serial)
            free (eeprom->serial);
        eeprom->serial = malloc(strlen(serial)+1);
        if (eeprom->serial)
        {
            strcpy(eeprom->serial, serial);
            eeprom->use_serial = 1;
        }
    }
    return 0;
}


/*FTD2XX doesn't check for values not fitting in the ACBUS Signal oprtions*/
void set_ft232h_cbus(struct briteblox_eeprom *eeprom, unsigned char * output)
{
    int i;
    for(i=0; i<5; i++)
    {
        int mode_low, mode_high;
        if (eeprom->cbus_function[2*i]> CBUSH_CLK7_5)
            mode_low = CBUSH_TRISTATE;
        else
            mode_low = eeprom->cbus_function[2*i];
        if (eeprom->cbus_function[2*i+1]> CBUSH_CLK7_5)
            mode_high = CBUSH_TRISTATE;
        else
            mode_high = eeprom->cbus_function[2*i+1];

        output[0x18+i] = (mode_high <<4) | mode_low;
    }
}
/* Return the bits for the encoded EEPROM Structure of a requested Mode
 *
 */
static unsigned char type2bit(unsigned char type, enum briteblox_chip_type chip)
{
    switch (chip)
    {
        case TYPE_2232H:
        case TYPE_2232C:
        {
            switch (type)
            {
                case CHANNEL_IS_UART: return 0;
                case CHANNEL_IS_FIFO: return 0x01;
                case CHANNEL_IS_OPTO: return 0x02;
                case CHANNEL_IS_CPU : return 0x04;
                default: return 0;
            }
        }
        case TYPE_232H:
        {
            switch (type)
            {
                case CHANNEL_IS_UART   : return 0;
                case CHANNEL_IS_FIFO   : return 0x01;
                case CHANNEL_IS_OPTO   : return 0x02;
                case CHANNEL_IS_CPU    : return 0x04;
                case CHANNEL_IS_FT1284 : return 0x08;
                default: return 0;
            }
        }
        case TYPE_230X: /* FT230X is only UART */
        default: return 0;
    }
    return 0;
}

/**
    Build binary buffer from briteblox_eeprom structure.
    Output is suitable for briteblox_write_eeprom().

    \param briteblox pointer to briteblox_context

    \retval >=0: size of eeprom user area in bytes
    \retval -1: eeprom size (128 bytes) exceeded by custom strings
    \retval -2: Invalid eeprom or briteblox pointer
    \retval -3: Invalid cbus function setting     (FIXME: Not in the code?)
    \retval -4: Chip doesn't support invert       (FIXME: Not in the code?)
    \retval -5: Chip doesn't support high current drive         (FIXME: Not in the code?)
    \retval -6: No connected EEPROM or EEPROM Type unknown
*/
int briteblox_eeprom_build(struct briteblox_context *briteblox)
{
    unsigned char i, j, eeprom_size_mask;
    unsigned short checksum, value;
    unsigned char manufacturer_size = 0, product_size = 0, serial_size = 0;
    int user_area_size;
    struct briteblox_eeprom *eeprom;
    unsigned char * output;

    if (briteblox == NULL)
        briteblox_error_return(-2,"No context");
    if (briteblox->eeprom == NULL)
        briteblox_error_return(-2,"No eeprom structure");

    eeprom= briteblox->eeprom;
    output = eeprom->buf;

    if (eeprom->chip == -1)
        briteblox_error_return(-6,"No connected EEPROM or EEPROM type unknown");

    if (eeprom->size == -1)
    {
        if ((eeprom->chip == 0x56) || (eeprom->chip == 0x66))
            eeprom->size = 0x100;
        else
            eeprom->size = 0x80;
    }

    if (eeprom->manufacturer != NULL)
        manufacturer_size = strlen(eeprom->manufacturer);
    if (eeprom->product != NULL)
        product_size = strlen(eeprom->product);
    if (eeprom->serial != NULL)
        serial_size = strlen(eeprom->serial);

    // eeprom size check
    switch (briteblox->type)
    {
        case TYPE_AM:
        case TYPE_BM:
            user_area_size = 96;    // base size for strings (total of 48 characters)
            break;
        case TYPE_2232C:
            user_area_size = 90;     // two extra config bytes and 4 bytes PnP stuff
            break;
        case TYPE_R:
        case TYPE_230X:
            user_area_size = 88;     // four extra config bytes + 4 bytes PnP stuff
            break;
        case TYPE_2232H:            // six extra config bytes + 4 bytes PnP stuff
        case TYPE_4232H:
            user_area_size = 86;
            break;
        case TYPE_232H:
            user_area_size = 80;
            break;
        default:
            user_area_size = 0;
            break;
    }
    user_area_size  -= (manufacturer_size + product_size + serial_size) * 2;

    if (user_area_size < 0)
        briteblox_error_return(-1,"eeprom size exceeded");

    // empty eeprom
    if (briteblox->type == TYPE_230X)
    {
        /* FT230X have a reserved section in the middle of the MTP,
           which cannot be written to, but must be included in the checksum */
        memset(briteblox->eeprom->buf, 0, 0x80);
        memset((briteblox->eeprom->buf + 0xa0), 0, (BRITEBLOX_MAX_EEPROM_SIZE - 0xa0));
    }
    else
    {
        memset(briteblox->eeprom->buf, 0, BRITEBLOX_MAX_EEPROM_SIZE);
    }

    // Bytes and Bits set for all Types

    // Addr 02: Vendor ID
    output[0x02] = eeprom->vendor_id;
    output[0x03] = eeprom->vendor_id >> 8;

    // Addr 04: Product ID
    output[0x04] = eeprom->product_id;
    output[0x05] = eeprom->product_id >> 8;

    // Addr 06: Device release number (0400h for BM features)
    output[0x06] = eeprom->release_number;
    output[0x07] = eeprom->release_number >> 8;

    // Addr 08: Config descriptor
    // Bit 7: always 1
    // Bit 6: 1 if this device is self powered, 0 if bus powered
    // Bit 5: 1 if this device uses remote wakeup
    // Bit 4-0: reserved - 0
    j = 0x80;
    if (eeprom->self_powered)
        j |= 0x40;
    if (eeprom->remote_wakeup)
        j |= 0x20;
    output[0x08] = j;

    // Addr 09: Max power consumption: max power = value * 2 mA
    output[0x09] = eeprom->max_power / MAX_POWER_MILLIAMP_PER_UNIT;

    if ((briteblox->type != TYPE_AM) && (briteblox->type != TYPE_230X))
    {
        // Addr 0A: Chip configuration
        // Bit 7: 0 - reserved
        // Bit 6: 0 - reserved
        // Bit 5: 0 - reserved
        // Bit 4: 1 - Change USB version
        // Bit 3: 1 - Use the serial number string
        // Bit 2: 1 - Enable suspend pull downs for lower power
        // Bit 1: 1 - Out EndPoint is Isochronous
        // Bit 0: 1 - In EndPoint is Isochronous
        //
        j = 0;
        if (eeprom->in_is_isochronous)
            j = j | 1;
        if (eeprom->out_is_isochronous)
            j = j | 2;
        output[0x0A] = j;
    }

    // Dynamic content
    // Strings start at 0x94 (TYPE_AM, TYPE_BM)
    // 0x96 (TYPE_2232C), 0x98 (TYPE_R) and 0x9a (TYPE_x232H)
    // 0xa0 (TYPE_232H)
    i = 0;
    switch (briteblox->type)
    {
        case TYPE_2232H:
        case TYPE_4232H:
            i += 2;
        case TYPE_R:
            i += 2;
        case TYPE_2232C:
            i += 2;
        case TYPE_AM:
        case TYPE_BM:
            i += 0x94;
            break;
        case TYPE_232H:
        case TYPE_230X:
            i = 0xa0;
            break;
    }
    /* Wrap around 0x80 for 128 byte EEPROMS (Internale and 93x46) */
    eeprom_size_mask = eeprom->size -1;

    // Addr 0E: Offset of the manufacturer string + 0x80, calculated later
    // Addr 0F: Length of manufacturer string
    // Output manufacturer
    output[0x0E] = i;  // calculate offset
    output[i & eeprom_size_mask] = manufacturer_size*2 + 2, i++;
    output[i & eeprom_size_mask] = 0x03, i++; // type: string
    for (j = 0; j < manufacturer_size; j++)
    {
        output[i & eeprom_size_mask] = eeprom->manufacturer[j], i++;
        output[i & eeprom_size_mask] = 0x00, i++;
    }
    output[0x0F] = manufacturer_size*2 + 2;

    // Addr 10: Offset of the product string + 0x80, calculated later
    // Addr 11: Length of product string
    output[0x10] = i | 0x80;  // calculate offset
    output[i & eeprom_size_mask] = product_size*2 + 2, i++;
    output[i & eeprom_size_mask] = 0x03, i++;
    for (j = 0; j < product_size; j++)
    {
        output[i & eeprom_size_mask] = eeprom->product[j], i++;
        output[i & eeprom_size_mask] = 0x00, i++;
    }
    output[0x11] = product_size*2 + 2;

    // Addr 12: Offset of the serial string + 0x80, calculated later
    // Addr 13: Length of serial string
    output[0x12] = i | 0x80; // calculate offset
    output[i & eeprom_size_mask] = serial_size*2 + 2, i++;
    output[i & eeprom_size_mask] = 0x03, i++;
    for (j = 0; j < serial_size; j++)
    {
        output[i & eeprom_size_mask] = eeprom->serial[j], i++;
        output[i & eeprom_size_mask] = 0x00, i++;
    }

    // Legacy port name and PnP fields for FT2232 and newer chips
    if (briteblox->type > TYPE_BM)
    {
        output[i & eeprom_size_mask] = 0x02; /* as seen when written with FTD2XX */
        i++;
        output[i & eeprom_size_mask] = 0x03; /* as seen when written with FTD2XX */
        i++;
        output[i & eeprom_size_mask] = eeprom->is_not_pnp; /* as seen when written with FTD2XX */
        i++;
    }

    output[0x13] = serial_size*2 + 2;

    if (briteblox->type > TYPE_AM) /* use_serial not used in AM devices */
    {
        if (eeprom->use_serial)
            output[0x0A] |= USE_SERIAL_NUM;
        else
            output[0x0A] &= ~USE_SERIAL_NUM;
    }

    /* Bytes and Bits specific to (some) types
       Write linear, as this allows easier fixing*/
    switch (briteblox->type)
    {
        case TYPE_AM:
            break;
        case TYPE_BM:
            output[0x0C] = eeprom->usb_version & 0xff;
            output[0x0D] = (eeprom->usb_version>>8) & 0xff;
            if (eeprom->use_usb_version == USE_USB_VERSION_BIT)
                output[0x0A] |= USE_USB_VERSION_BIT;
            else
                output[0x0A] &= ~USE_USB_VERSION_BIT;

            break;
        case TYPE_2232C:

            output[0x00] = type2bit(eeprom->channel_a_type, TYPE_2232C);
            if ( eeprom->channel_a_driver == DRIVER_VCP)
                output[0x00] |= DRIVER_VCP;
            else
                output[0x00] &= ~DRIVER_VCP;

            if ( eeprom->high_current_a == HIGH_CURRENT_DRIVE)
                output[0x00] |= HIGH_CURRENT_DRIVE;
            else
                output[0x00] &= ~HIGH_CURRENT_DRIVE;

            output[0x01] = type2bit(eeprom->channel_b_type, TYPE_2232C);
            if ( eeprom->channel_b_driver == DRIVER_VCP)
                output[0x01] |= DRIVER_VCP;
            else
                output[0x01] &= ~DRIVER_VCP;

            if ( eeprom->high_current_b == HIGH_CURRENT_DRIVE)
                output[0x01] |= HIGH_CURRENT_DRIVE;
            else
                output[0x01] &= ~HIGH_CURRENT_DRIVE;

            if (eeprom->in_is_isochronous)
                output[0x0A] |= 0x1;
            else
                output[0x0A] &= ~0x1;
            if (eeprom->out_is_isochronous)
                output[0x0A] |= 0x2;
            else
                output[0x0A] &= ~0x2;
            if (eeprom->suspend_pull_downs)
                output[0x0A] |= 0x4;
            else
                output[0x0A] &= ~0x4;
            if (eeprom->use_usb_version == USE_USB_VERSION_BIT)
                output[0x0A] |= USE_USB_VERSION_BIT;
            else
                output[0x0A] &= ~USE_USB_VERSION_BIT;

            output[0x0C] = eeprom->usb_version & 0xff;
            output[0x0D] = (eeprom->usb_version>>8) & 0xff;
            output[0x14] = eeprom->chip;
            break;
        case TYPE_R:
            if (eeprom->high_current == HIGH_CURRENT_DRIVE_R)
                output[0x00] |= HIGH_CURRENT_DRIVE_R;
            output[0x01] = 0x40; /* Hard coded Endpoint Size*/

            if (eeprom->suspend_pull_downs)
                output[0x0A] |= 0x4;
            else
                output[0x0A] &= ~0x4;
            output[0x0B] = eeprom->invert;
            output[0x0C] = eeprom->usb_version & 0xff;
            output[0x0D] = (eeprom->usb_version>>8) & 0xff;

            if (eeprom->cbus_function[0] > CBUS_BB)
                output[0x14] = CBUS_TXLED;
            else
                output[0x14] = eeprom->cbus_function[0];

            if (eeprom->cbus_function[1] > CBUS_BB)
                output[0x14] |= CBUS_RXLED<<4;
            else
                output[0x14] |= eeprom->cbus_function[1]<<4;

            if (eeprom->cbus_function[2] > CBUS_BB)
                output[0x15] = CBUS_TXDEN;
            else
                output[0x15] = eeprom->cbus_function[2];

            if (eeprom->cbus_function[3] > CBUS_BB)
                output[0x15] |= CBUS_PWREN<<4;
            else
                output[0x15] |= eeprom->cbus_function[3]<<4;

            if (eeprom->cbus_function[4] > CBUS_CLK6)
                output[0x16] = CBUS_SLEEP;
            else
                output[0x16] = eeprom->cbus_function[4];
            break;
        case TYPE_2232H:
            output[0x00] = type2bit(eeprom->channel_a_type, TYPE_2232H);
            if ( eeprom->channel_a_driver == DRIVER_VCP)
                output[0x00] |= DRIVER_VCP;
            else
                output[0x00] &= ~DRIVER_VCP;

            output[0x01] = type2bit(eeprom->channel_b_type, TYPE_2232H);
            if ( eeprom->channel_b_driver == DRIVER_VCP)
                output[0x01] |= DRIVER_VCP;
            else
                output[0x01] &= ~DRIVER_VCP;
            if (eeprom->suspend_dbus7 == SUSPEND_DBUS7_BIT)
                output[0x01] |= SUSPEND_DBUS7_BIT;
            else
                output[0x01] &= ~SUSPEND_DBUS7_BIT;

            if (eeprom->suspend_pull_downs)
                output[0x0A] |= 0x4;
            else
                output[0x0A] &= ~0x4;

            if (eeprom->group0_drive > DRIVE_16MA)
                output[0x0c] |= DRIVE_16MA;
            else
                output[0x0c] |= eeprom->group0_drive;
            if (eeprom->group0_schmitt == IS_SCHMITT)
                output[0x0c] |= IS_SCHMITT;
            if (eeprom->group0_slew == SLOW_SLEW)
                output[0x0c] |= SLOW_SLEW;

            if (eeprom->group1_drive > DRIVE_16MA)
                output[0x0c] |= DRIVE_16MA<<4;
            else
                output[0x0c] |= eeprom->group1_drive<<4;
            if (eeprom->group1_schmitt == IS_SCHMITT)
                output[0x0c] |= IS_SCHMITT<<4;
            if (eeprom->group1_slew == SLOW_SLEW)
                output[0x0c] |= SLOW_SLEW<<4;

            if (eeprom->group2_drive > DRIVE_16MA)
                output[0x0d] |= DRIVE_16MA;
            else
                output[0x0d] |= eeprom->group2_drive;
            if (eeprom->group2_schmitt == IS_SCHMITT)
                output[0x0d] |= IS_SCHMITT;
            if (eeprom->group2_slew == SLOW_SLEW)
                output[0x0d] |= SLOW_SLEW;

            if (eeprom->group3_drive > DRIVE_16MA)
                output[0x0d] |= DRIVE_16MA<<4;
            else
                output[0x0d] |= eeprom->group3_drive<<4;
            if (eeprom->group3_schmitt == IS_SCHMITT)
                output[0x0d] |= IS_SCHMITT<<4;
            if (eeprom->group3_slew == SLOW_SLEW)
                output[0x0d] |= SLOW_SLEW<<4;

            output[0x18] = eeprom->chip;

            break;
        case TYPE_4232H:
            if (eeprom->channel_a_driver == DRIVER_VCP)
                output[0x00] |= DRIVER_VCP;
            else
                output[0x00] &= ~DRIVER_VCP;
            if (eeprom->channel_b_driver == DRIVER_VCP)
                output[0x01] |= DRIVER_VCP;
            else
                output[0x01] &= ~DRIVER_VCP;
            if (eeprom->channel_c_driver == DRIVER_VCP)
                output[0x00] |= (DRIVER_VCP << 4);
            else
                output[0x00] &= ~(DRIVER_VCP << 4);
            if (eeprom->channel_d_driver == DRIVER_VCP)
                output[0x01] |= (DRIVER_VCP << 4);
            else
                output[0x01] &= ~(DRIVER_VCP << 4);

            if (eeprom->suspend_pull_downs)
                output[0x0a] |= 0x4;
            else
                output[0x0a] &= ~0x4;

            if (eeprom->channel_a_rs485enable)
                output[0x0b] |= CHANNEL_IS_RS485 << 0;
            else
                output[0x0b] &= ~(CHANNEL_IS_RS485 << 0);
            if (eeprom->channel_b_rs485enable)
                output[0x0b] |= CHANNEL_IS_RS485 << 1;
            else
                output[0x0b] &= ~(CHANNEL_IS_RS485 << 1);
            if (eeprom->channel_c_rs485enable)
                output[0x0b] |= CHANNEL_IS_RS485 << 2;
            else
                output[0x0b] &= ~(CHANNEL_IS_RS485 << 2);
            if (eeprom->channel_d_rs485enable)
                output[0x0b] |= CHANNEL_IS_RS485 << 3;
            else
                output[0x0b] &= ~(CHANNEL_IS_RS485 << 3);

            if (eeprom->group0_drive > DRIVE_16MA)
                output[0x0c] |= DRIVE_16MA;
            else
                output[0x0c] |= eeprom->group0_drive;
            if (eeprom->group0_schmitt == IS_SCHMITT)
                output[0x0c] |= IS_SCHMITT;
            if (eeprom->group0_slew == SLOW_SLEW)
                output[0x0c] |= SLOW_SLEW;

            if (eeprom->group1_drive > DRIVE_16MA)
                output[0x0c] |= DRIVE_16MA<<4;
            else
                output[0x0c] |= eeprom->group1_drive<<4;
            if (eeprom->group1_schmitt == IS_SCHMITT)
                output[0x0c] |= IS_SCHMITT<<4;
            if (eeprom->group1_slew == SLOW_SLEW)
                output[0x0c] |= SLOW_SLEW<<4;

            if (eeprom->group2_drive > DRIVE_16MA)
                output[0x0d] |= DRIVE_16MA;
            else
                output[0x0d] |= eeprom->group2_drive;
            if (eeprom->group2_schmitt == IS_SCHMITT)
                output[0x0d] |= IS_SCHMITT;
            if (eeprom->group2_slew == SLOW_SLEW)
                output[0x0d] |= SLOW_SLEW;

            if (eeprom->group3_drive > DRIVE_16MA)
                output[0x0d] |= DRIVE_16MA<<4;
            else
                output[0x0d] |= eeprom->group3_drive<<4;
            if (eeprom->group3_schmitt == IS_SCHMITT)
                output[0x0d] |= IS_SCHMITT<<4;
            if (eeprom->group3_slew == SLOW_SLEW)
                output[0x0d] |= SLOW_SLEW<<4;

            output[0x18] = eeprom->chip;

            break;
        case TYPE_232H:
            output[0x00] = type2bit(eeprom->channel_a_type, TYPE_232H);
            if ( eeprom->channel_a_driver == DRIVER_VCP)
                output[0x00] |= DRIVER_VCPH;
            else
                output[0x00] &= ~DRIVER_VCPH;
            if (eeprom->powersave)
                output[0x01] |= POWER_SAVE_DISABLE_H;
            else
                output[0x01] &= ~POWER_SAVE_DISABLE_H;

            if (eeprom->suspend_pull_downs)
                output[0x0a] |= 0x4;
            else
                output[0x0a] &= ~0x4;

            if (eeprom->clock_polarity)
                output[0x01] |= FT1284_CLK_IDLE_STATE;
            else
                output[0x01] &= ~FT1284_CLK_IDLE_STATE;
            if (eeprom->data_order)
                output[0x01] |= FT1284_DATA_LSB;
            else
                output[0x01] &= ~FT1284_DATA_LSB;
            if (eeprom->flow_control)
                output[0x01] |= FT1284_FLOW_CONTROL;
            else
                output[0x01] &= ~FT1284_FLOW_CONTROL;
            if (eeprom->group0_drive > DRIVE_16MA)
                output[0x0c] |= DRIVE_16MA;
            else
                output[0x0c] |= eeprom->group0_drive;
            if (eeprom->group0_schmitt == IS_SCHMITT)
                output[0x0c] |= IS_SCHMITT;
            if (eeprom->group0_slew == SLOW_SLEW)
                output[0x0c] |= SLOW_SLEW;

            if (eeprom->group1_drive > DRIVE_16MA)
                output[0x0d] |= DRIVE_16MA;
            else
                output[0x0d] |= eeprom->group1_drive;
            if (eeprom->group1_schmitt == IS_SCHMITT)
                output[0x0d] |= IS_SCHMITT;
            if (eeprom->group1_slew == SLOW_SLEW)
                output[0x0d] |= SLOW_SLEW;

            set_ft232h_cbus(eeprom, output);

            output[0x1e] = eeprom->chip;
            fprintf(stderr,"FIXME: Build FT232H specific EEPROM settings\n");
            break;
        case TYPE_230X:
            output[0x00] = 0x80; /* Actually, leave the default value */
            output[0x0a] = 0x08; /* Enable USB Serial Number */
            output[0x0c] = (0x01) | (0x3 << 4); /* DBUS drive 4mA, CBUS drive 16mA */
            for (j = 0; j <= 6; j++)
            {
                output[0x1a + j] = eeprom->cbus_function[j];
            }
            break;
    }

    // calculate checksum
    checksum = 0xAAAA;

    for (i = 0; i < eeprom->size/2-1; i++)
    {
        if ((briteblox->type == TYPE_230X) && (i == 0x12))
        {
            /* FT230X has a user section in the MTP which is not part of the checksum */
            i = 0x40;
        }
        value = output[i*2];
        value += output[(i*2)+1] << 8;

        checksum = value^checksum;
        checksum = (checksum << 1) | (checksum >> 15);
    }

    output[eeprom->size-2] = checksum;
    output[eeprom->size-1] = checksum >> 8;

    eeprom->initialized_for_connected_device = 1;
    return user_area_size;
}
/* Decode the encoded EEPROM field for the BRITEBLOX Mode into a value for the abstracted
 * EEPROM structure
 *
 * FTD2XX doesn't allow to set multiple bits in the interface mode bitfield, and so do we
 */
static unsigned char bit2type(unsigned char bits)
{
    switch (bits)
    {
        case   0: return CHANNEL_IS_UART;
        case   1: return CHANNEL_IS_FIFO;
        case   2: return CHANNEL_IS_OPTO;
        case   4: return CHANNEL_IS_CPU;
        case   8: return CHANNEL_IS_FT1284;
        default:
            fprintf(stderr," Unexpected value %d for Hardware Interface type\n",
                    bits);
    }
    return 0;
}
/**
   Decode binary EEPROM image into an briteblox_eeprom structure.

   \param briteblox pointer to briteblox_context
   \param verbose Decode EEPROM on stdout

   \retval 0: all fine
   \retval -1: something went wrong

   FIXME: How to pass size? How to handle size field in briteblox_eeprom?
   FIXME: Strings are malloc'ed here and should be freed somewhere
*/
int briteblox_eeprom_decode(struct briteblox_context *briteblox, int verbose)
{
    unsigned char i, j;
    unsigned short checksum, eeprom_checksum, value;
    unsigned char manufacturer_size = 0, product_size = 0, serial_size = 0;
    int eeprom_size;
    struct briteblox_eeprom *eeprom;
    unsigned char *buf = NULL;

    if (briteblox == NULL)
        briteblox_error_return(-1,"No context");
    if (briteblox->eeprom == NULL)
        briteblox_error_return(-1,"No eeprom structure");

    eeprom = briteblox->eeprom;
    eeprom_size = eeprom->size;
    buf = briteblox->eeprom->buf;

    // Addr 02: Vendor ID
    eeprom->vendor_id = buf[0x02] + (buf[0x03] << 8);

    // Addr 04: Product ID
    eeprom->product_id = buf[0x04] + (buf[0x05] << 8);

    // Addr 06: Device release number
    eeprom->release_number = buf[0x06] + (buf[0x07]<<8);

    // Addr 08: Config descriptor
    // Bit 7: always 1
    // Bit 6: 1 if this device is self powered, 0 if bus powered
    // Bit 5: 1 if this device uses remote wakeup
    eeprom->self_powered = buf[0x08] & 0x40;
    eeprom->remote_wakeup = buf[0x08] & 0x20;

    // Addr 09: Max power consumption: max power = value * 2 mA
    eeprom->max_power = MAX_POWER_MILLIAMP_PER_UNIT * buf[0x09];

    // Addr 0A: Chip configuration
    // Bit 7: 0 - reserved
    // Bit 6: 0 - reserved
    // Bit 5: 0 - reserved
    // Bit 4: 1 - Change USB version on BM and 2232C
    // Bit 3: 1 - Use the serial number string
    // Bit 2: 1 - Enable suspend pull downs for lower power
    // Bit 1: 1 - Out EndPoint is Isochronous
    // Bit 0: 1 - In EndPoint is Isochronous
    //
    eeprom->in_is_isochronous  = buf[0x0A]&0x01;
    eeprom->out_is_isochronous = buf[0x0A]&0x02;
    eeprom->suspend_pull_downs = buf[0x0A]&0x04;
    eeprom->use_serial         = (buf[0x0A] & USE_SERIAL_NUM)?1:0;
    eeprom->use_usb_version    = buf[0x0A] & USE_USB_VERSION_BIT;

    // Addr 0C: USB version low byte when 0x0A
    // Addr 0D: USB version high byte when 0x0A
    eeprom->usb_version = buf[0x0C] + (buf[0x0D] << 8);

    // Addr 0E: Offset of the manufacturer string + 0x80, calculated later
    // Addr 0F: Length of manufacturer string
    manufacturer_size = buf[0x0F]/2;
    if (eeprom->manufacturer)
        free(eeprom->manufacturer);
    if (manufacturer_size > 0)
    {
        eeprom->manufacturer = malloc(manufacturer_size);
        if (eeprom->manufacturer)
        {
            // Decode manufacturer
            i = buf[0x0E] & (eeprom_size -1); // offset
            for (j=0; j<manufacturer_size-1; j++)
            {
                eeprom->manufacturer[j] = buf[2*j+i+2];
            }
            eeprom->manufacturer[j] = '\0';
        }
    }
    else eeprom->manufacturer = NULL;

    // Addr 10: Offset of the product string + 0x80, calculated later
    // Addr 11: Length of product string
    if (eeprom->product)
        free(eeprom->product);
    product_size = buf[0x11]/2;
    if (product_size > 0)
    {
        eeprom->product = malloc(product_size);
        if (eeprom->product)
        {
            // Decode product name
            i = buf[0x10] & (eeprom_size -1); // offset
            for (j=0; j<product_size-1; j++)
            {
                eeprom->product[j] = buf[2*j+i+2];
            }
            eeprom->product[j] = '\0';
        }
    }
    else eeprom->product = NULL;

    // Addr 12: Offset of the serial string + 0x80, calculated later
    // Addr 13: Length of serial string
    if (eeprom->serial)
        free(eeprom->serial);
    serial_size = buf[0x13]/2;
    if (serial_size > 0)
    {
        eeprom->serial = malloc(serial_size);
        if (eeprom->serial)
        {
            // Decode serial
            i = buf[0x12] & (eeprom_size -1); // offset
            for (j=0; j<serial_size-1; j++)
            {
                eeprom->serial[j] = buf[2*j+i+2];
            }
            eeprom->serial[j] = '\0';
        }
    }
    else eeprom->serial = NULL;

    // verify checksum
    checksum = 0xAAAA;

    for (i = 0; i < eeprom_size/2-1; i++)
    {
        if ((briteblox->type == TYPE_230X) && (i == 0x12))
        {
            /* FT230X has a user section in the MTP which is not part of the checksum */
            i = 0x40;
        }
        value = buf[i*2];
        value += buf[(i*2)+1] << 8;

        checksum = value^checksum;
        checksum = (checksum << 1) | (checksum >> 15);
    }

    eeprom_checksum = buf[eeprom_size-2] + (buf[eeprom_size-1] << 8);

    if (eeprom_checksum != checksum)
    {
        fprintf(stderr, "Checksum Error: %04x %04x\n", checksum, eeprom_checksum);
        briteblox_error_return(-1,"EEPROM checksum error");
    }

    eeprom->channel_a_type   = 0;
    if ((briteblox->type == TYPE_AM) || (briteblox->type == TYPE_BM))
    {
        eeprom->chip = -1;
    }
    else if (briteblox->type == TYPE_2232C)
    {
        eeprom->channel_a_type   = bit2type(buf[0x00] & 0x7);
        eeprom->channel_a_driver = buf[0x00] & DRIVER_VCP;
        eeprom->high_current_a   = buf[0x00] & HIGH_CURRENT_DRIVE;
        eeprom->channel_b_type   = buf[0x01] & 0x7;
        eeprom->channel_b_driver = buf[0x01] & DRIVER_VCP;
        eeprom->high_current_b   = buf[0x01] & HIGH_CURRENT_DRIVE;
        eeprom->chip = buf[0x14];
    }
    else if (briteblox->type == TYPE_R)
    {
        /* TYPE_R flags D2XX, not VCP as all others*/
        eeprom->channel_a_driver = ~buf[0x00] & DRIVER_VCP;
        eeprom->high_current     = buf[0x00] & HIGH_CURRENT_DRIVE_R;
        if ( (buf[0x01]&0x40) != 0x40)
            fprintf(stderr,
                    "TYPE_R EEPROM byte[0x01] Bit 6 unexpected Endpoint size."
                    " If this happened with the\n"
                    " EEPROM programmed by BRITEBLOX tools, please report "
                    "to libbriteblox@developer.intra2net.com\n");

        eeprom->chip = buf[0x16];
        // Addr 0B: Invert data lines
        // Works only on FT232R, not FT245R, but no way to distinguish
        eeprom->invert = buf[0x0B];
        // Addr 14: CBUS function: CBUS0, CBUS1
        // Addr 15: CBUS function: CBUS2, CBUS3
        // Addr 16: CBUS function: CBUS5
        eeprom->cbus_function[0] = buf[0x14] & 0x0f;
        eeprom->cbus_function[1] = (buf[0x14] >> 4) & 0x0f;
        eeprom->cbus_function[2] = buf[0x15] & 0x0f;
        eeprom->cbus_function[3] = (buf[0x15] >> 4) & 0x0f;
        eeprom->cbus_function[4] = buf[0x16] & 0x0f;
    }
    else if ((briteblox->type == TYPE_2232H) || (briteblox->type == TYPE_4232H))
    {
        eeprom->channel_a_driver = buf[0x00] & DRIVER_VCP;
        eeprom->channel_b_driver = buf[0x01] & DRIVER_VCP;

        if (briteblox->type == TYPE_2232H)
        {
            eeprom->channel_a_type   = bit2type(buf[0x00] & 0x7);
            eeprom->channel_b_type   = bit2type(buf[0x01] & 0x7);
            eeprom->suspend_dbus7    = buf[0x01] & SUSPEND_DBUS7_BIT;
        }
        else
        {
            eeprom->channel_c_driver = (buf[0x00] >> 4) & DRIVER_VCP;
            eeprom->channel_d_driver = (buf[0x01] >> 4) & DRIVER_VCP;
            eeprom->channel_a_rs485enable = buf[0x0b] & (CHANNEL_IS_RS485 << 0);
            eeprom->channel_b_rs485enable = buf[0x0b] & (CHANNEL_IS_RS485 << 1);
            eeprom->channel_c_rs485enable = buf[0x0b] & (CHANNEL_IS_RS485 << 2);
            eeprom->channel_d_rs485enable = buf[0x0b] & (CHANNEL_IS_RS485 << 3);
        }

        eeprom->chip = buf[0x18];
        eeprom->group0_drive   =  buf[0x0c]       & DRIVE_16MA;
        eeprom->group0_schmitt =  buf[0x0c]       & IS_SCHMITT;
        eeprom->group0_slew    =  buf[0x0c]       & SLOW_SLEW;
        eeprom->group1_drive   = (buf[0x0c] >> 4) & 0x3;
        eeprom->group1_schmitt = (buf[0x0c] >> 4) & IS_SCHMITT;
        eeprom->group1_slew    = (buf[0x0c] >> 4) & SLOW_SLEW;
        eeprom->group2_drive   =  buf[0x0d]       & DRIVE_16MA;
        eeprom->group2_schmitt =  buf[0x0d]       & IS_SCHMITT;
        eeprom->group2_slew    =  buf[0x0d]       & SLOW_SLEW;
        eeprom->group3_drive   = (buf[0x0d] >> 4) & DRIVE_16MA;
        eeprom->group3_schmitt = (buf[0x0d] >> 4) & IS_SCHMITT;
        eeprom->group3_slew    = (buf[0x0d] >> 4) & SLOW_SLEW;
    }
    else if (briteblox->type == TYPE_232H)
    {
        int i;

        eeprom->channel_a_type   = buf[0x00] & 0xf;
        eeprom->channel_a_driver = (buf[0x00] & DRIVER_VCPH)?DRIVER_VCP:0;
        eeprom->clock_polarity =  buf[0x01]       & FT1284_CLK_IDLE_STATE;
        eeprom->data_order     =  buf[0x01]       & FT1284_DATA_LSB;
        eeprom->flow_control   =  buf[0x01]       & FT1284_FLOW_CONTROL;
        eeprom->powersave      =  buf[0x01]       & POWER_SAVE_DISABLE_H;
        eeprom->group0_drive   =  buf[0x0c]       & DRIVE_16MA;
        eeprom->group0_schmitt =  buf[0x0c]       & IS_SCHMITT;
        eeprom->group0_slew    =  buf[0x0c]       & SLOW_SLEW;
        eeprom->group1_drive   =  buf[0x0d]       & DRIVE_16MA;
        eeprom->group1_schmitt =  buf[0x0d]       & IS_SCHMITT;
        eeprom->group1_slew    =  buf[0x0d]       & SLOW_SLEW;

        for(i=0; i<5; i++)
        {
            eeprom->cbus_function[2*i  ] =  buf[0x18+i] & 0x0f;
            eeprom->cbus_function[2*i+1] = (buf[0x18+i] >> 4) & 0x0f;
        }
        eeprom->chip = buf[0x1e];
        /*FIXME: Decipher more values*/
    }
    else if (briteblox->type == TYPE_230X)
    {
        for(i=0; i<4; i++)
        {
            eeprom->cbus_function[i] =  buf[0x1a + i] & 0xFF;
        }
        eeprom->group0_drive   =  buf[0x0c]       & 0x03;
        eeprom->group0_schmitt =  buf[0x0c]       & IS_SCHMITT;
        eeprom->group0_slew    =  buf[0x0c]       & SLOW_SLEW;
        eeprom->group1_drive   = (buf[0x0c] >> 4) & 0x03;
        eeprom->group1_schmitt = (buf[0x0c] >> 4) & IS_SCHMITT;
        eeprom->group1_slew    = (buf[0x0c] >> 4) & SLOW_SLEW;
    }

    if (verbose)
    {
        char *channel_mode[] = {"UART", "FIFO", "CPU", "OPTO", "FT1284"};
        fprintf(stdout, "VID:     0x%04x\n",eeprom->vendor_id);
        fprintf(stdout, "PID:     0x%04x\n",eeprom->product_id);
        fprintf(stdout, "Release: 0x%04x\n",eeprom->release_number);

        if (eeprom->self_powered)
            fprintf(stdout, "Self-Powered%s", (eeprom->remote_wakeup)?", USB Remote Wake Up\n":"\n");
        else
            fprintf(stdout, "Bus Powered: %3d mA%s", eeprom->max_power,
                    (eeprom->remote_wakeup)?" USB Remote Wake Up\n":"\n");
        if (eeprom->manufacturer)
            fprintf(stdout, "Manufacturer: %s\n",eeprom->manufacturer);
        if (eeprom->product)
            fprintf(stdout, "Product:      %s\n",eeprom->product);
        if (eeprom->serial)
            fprintf(stdout, "Serial:       %s\n",eeprom->serial);
        fprintf(stdout,     "Checksum      : %04x\n", checksum);
        if (briteblox->type == TYPE_R)
            fprintf(stdout,     "Internal EEPROM\n");
        else if (eeprom->chip >= 0x46)
            fprintf(stdout,     "Attached EEPROM: 93x%02x\n", eeprom->chip);
        if (eeprom->suspend_dbus7)
            fprintf(stdout, "Suspend on DBUS7\n");
        if (eeprom->suspend_pull_downs)
            fprintf(stdout, "Pull IO pins low during suspend\n");
        if(eeprom->powersave)
        {
            if(briteblox->type >= TYPE_232H)
                fprintf(stdout,"Enter low power state on ACBUS7\n");
        }
        if (eeprom->remote_wakeup)
            fprintf(stdout, "Enable Remote Wake Up\n");
        fprintf(stdout, "PNP: %d\n",(eeprom->is_not_pnp)?0:1);
        if (briteblox->type >= TYPE_2232C)
            fprintf(stdout,"Channel A has Mode %s%s%s\n",
                    channel_mode[eeprom->channel_a_type],
                    (eeprom->channel_a_driver)?" VCP":"",
                    (eeprom->high_current_a)?" High Current IO":"");
        if (briteblox->type >= TYPE_232H)
        {
            fprintf(stdout,"FT1284 Mode Clock is idle %s, %s first, %sFlow Control\n",
                    (eeprom->clock_polarity)?"HIGH":"LOW",
                    (eeprom->data_order)?"LSB":"MSB",
                    (eeprom->flow_control)?"":"No ");
        }
        if ((briteblox->type >= TYPE_2232C) && (briteblox->type != TYPE_R) && (briteblox->type != TYPE_232H))
            fprintf(stdout,"Channel B has Mode %s%s%s\n",
                    channel_mode[eeprom->channel_b_type],
                    (eeprom->channel_b_driver)?" VCP":"",
                    (eeprom->high_current_b)?" High Current IO":"");
        if (((briteblox->type == TYPE_BM) || (briteblox->type == TYPE_2232C)) &&
                eeprom->use_usb_version == USE_USB_VERSION_BIT)
            fprintf(stdout,"Use explicit USB Version %04x\n",eeprom->usb_version);

        if ((briteblox->type == TYPE_2232H) || (briteblox->type == TYPE_4232H))
        {
            fprintf(stdout,"%s has %d mA drive%s%s\n",
                    (briteblox->type == TYPE_2232H)?"AL":"A",
                    (eeprom->group0_drive+1) *4,
                    (eeprom->group0_schmitt)?" Schmitt Input":"",
                    (eeprom->group0_slew)?" Slow Slew":"");
            fprintf(stdout,"%s has %d mA drive%s%s\n",
                    (briteblox->type == TYPE_2232H)?"AH":"B",
                    (eeprom->group1_drive+1) *4,
                    (eeprom->group1_schmitt)?" Schmitt Input":"",
                    (eeprom->group1_slew)?" Slow Slew":"");
            fprintf(stdout,"%s has %d mA drive%s%s\n",
                    (briteblox->type == TYPE_2232H)?"BL":"C",
                    (eeprom->group2_drive+1) *4,
                    (eeprom->group2_schmitt)?" Schmitt Input":"",
                    (eeprom->group2_slew)?" Slow Slew":"");
            fprintf(stdout,"%s has %d mA drive%s%s\n",
                    (briteblox->type == TYPE_2232H)?"BH":"D",
                    (eeprom->group3_drive+1) *4,
                    (eeprom->group3_schmitt)?" Schmitt Input":"",
                    (eeprom->group3_slew)?" Slow Slew":"");
        }
        else if (briteblox->type == TYPE_232H)
        {
            int i;
            char *cbush_mux[] = {"TRISTATE","RXLED","TXLED", "TXRXLED","PWREN",
                                 "SLEEP","DRIVE_0","DRIVE_1","IOMODE","TXDEN",
                                 "CLK30","CLK15","CLK7_5"
                                };
            fprintf(stdout,"ACBUS has %d mA drive%s%s\n",
                    (eeprom->group0_drive+1) *4,
                    (eeprom->group0_schmitt)?" Schmitt Input":"",
                    (eeprom->group0_slew)?" Slow Slew":"");
            fprintf(stdout,"ADBUS has %d mA drive%s%s\n",
                    (eeprom->group1_drive+1) *4,
                    (eeprom->group1_schmitt)?" Schmitt Input":"",
                    (eeprom->group1_slew)?" Slow Slew":"");
            for (i=0; i<10; i++)
            {
                if (eeprom->cbus_function[i]<= CBUSH_CLK7_5 )
                    fprintf(stdout,"C%d Function: %s\n", i,
                            cbush_mux[eeprom->cbus_function[i]]);
            }
        }
        else if (briteblox->type == TYPE_230X)
        {
            int i;
            char *cbush_mux[] = {"TRISTATE","RXLED","TXLED", "TXRXLED","PWREN",
                                 "SLEEP","DRIVE_0","DRIVE_1","IOMODE","TXDEN",
                                 "CLK24","CLK12","CLK6","BAT_DETECT","BAT_DETECT#",
                                 "I2C_TXE#", "I2C_RXF#", "VBUS_SENSE", "BB_WR#",
                                 "BBRD#", "TIME_STAMP", "AWAKE#",
                                };
            fprintf(stdout,"IOBUS has %d mA drive%s%s\n",
                    (eeprom->group0_drive+1) *4,
                    (eeprom->group0_schmitt)?" Schmitt Input":"",
                    (eeprom->group0_slew)?" Slow Slew":"");
            fprintf(stdout,"CBUS has %d mA drive%s%s\n",
                    (eeprom->group1_drive+1) *4,
                    (eeprom->group1_schmitt)?" Schmitt Input":"",
                    (eeprom->group1_slew)?" Slow Slew":"");
            for (i=0; i<4; i++)
            {
                if (eeprom->cbus_function[i]<= CBUSH_AWAKE)
                    fprintf(stdout,"CBUS%d Function: %s\n", i, cbush_mux[eeprom->cbus_function[i]]);
            }
        }

        if (briteblox->type == TYPE_R)
        {
            char *cbus_mux[] = {"TXDEN","PWREN","RXLED", "TXLED","TX+RXLED",
                                "SLEEP","CLK48","CLK24","CLK12","CLK6",
                                "IOMODE","BB_WR","BB_RD"
                               };
            char *cbus_BB[] = {"RXF","TXE","RD", "WR"};

            if (eeprom->invert)
            {
                char *r_bits[] = {"TXD","RXD","RTS", "CTS","DTR","DSR","DCD","RI"};
                fprintf(stdout,"Inverted bits:");
                for (i=0; i<8; i++)
                    if ((eeprom->invert & (1<<i)) == (1<<i))
                        fprintf(stdout," %s",r_bits[i]);
                fprintf(stdout,"\n");
            }
            for (i=0; i<5; i++)
            {
                if (eeprom->cbus_function[i]<CBUS_BB)
                    fprintf(stdout,"C%d Function: %s\n", i,
                            cbus_mux[eeprom->cbus_function[i]]);
                else
                {
                    if (i < 4)
                        /* Running MPROG show that C0..3 have fixed function Synchronous
                           Bit Bang mode */
                        fprintf(stdout,"C%d BB Function: %s\n", i,
                                cbus_BB[i]);
                    else
                        fprintf(stdout, "Unknown CBUS mode. Might be special mode?\n");
                }
            }
        }
    }
    return 0;
}

/**
   Get a value from the decoded EEPROM structure

   \param briteblox pointer to briteblox_context
   \param value_name Enum of the value to query
   \param value Pointer to store read value

   \retval 0: all fine
   \retval -1: Value doesn't exist
*/
int briteblox_get_eeprom_value(struct briteblox_context *briteblox, enum briteblox_eeprom_value value_name, int* value)
{
    switch (value_name)
    {
        case VENDOR_ID:
            *value = briteblox->eeprom->vendor_id;
            break;
        case PRODUCT_ID:
            *value = briteblox->eeprom->product_id;
            break;
        case RELEASE_NUMBER:
            *value = briteblox->eeprom->release_number;
            break;
        case SELF_POWERED:
            *value = briteblox->eeprom->self_powered;
            break;
        case REMOTE_WAKEUP:
            *value = briteblox->eeprom->remote_wakeup;
            break;
        case IS_NOT_PNP:
            *value = briteblox->eeprom->is_not_pnp;
            break;
        case SUSPEND_DBUS7:
            *value = briteblox->eeprom->suspend_dbus7;
            break;
        case IN_IS_ISOCHRONOUS:
            *value = briteblox->eeprom->in_is_isochronous;
            break;
        case OUT_IS_ISOCHRONOUS:
            *value = briteblox->eeprom->out_is_isochronous;
            break;
        case SUSPEND_PULL_DOWNS:
            *value = briteblox->eeprom->suspend_pull_downs;
            break;
        case USE_SERIAL:
            *value = briteblox->eeprom->use_serial;
            break;
        case USB_VERSION:
            *value = briteblox->eeprom->usb_version;
            break;
        case USE_USB_VERSION:
            *value = briteblox->eeprom->use_usb_version;
            break;
        case MAX_POWER:
            *value = briteblox->eeprom->max_power;
            break;
        case CHANNEL_A_TYPE:
            *value = briteblox->eeprom->channel_a_type;
            break;
        case CHANNEL_B_TYPE:
            *value = briteblox->eeprom->channel_b_type;
            break;
        case CHANNEL_A_DRIVER:
            *value = briteblox->eeprom->channel_a_driver;
            break;
        case CHANNEL_B_DRIVER:
            *value = briteblox->eeprom->channel_b_driver;
            break;
        case CHANNEL_C_DRIVER:
            *value = briteblox->eeprom->channel_c_driver;
            break;
        case CHANNEL_D_DRIVER:
            *value = briteblox->eeprom->channel_d_driver;
            break;
        case CHANNEL_A_RS485:
            *value = briteblox->eeprom->channel_a_rs485enable;
            break;
        case CHANNEL_B_RS485:
            *value = briteblox->eeprom->channel_b_rs485enable;
            break;
        case CHANNEL_C_RS485:
            *value = briteblox->eeprom->channel_c_rs485enable;
            break;
        case CHANNEL_D_RS485:
            *value = briteblox->eeprom->channel_d_rs485enable;
            break;
        case CBUS_FUNCTION_0:
            *value = briteblox->eeprom->cbus_function[0];
            break;
        case CBUS_FUNCTION_1:
            *value = briteblox->eeprom->cbus_function[1];
            break;
        case CBUS_FUNCTION_2:
            *value = briteblox->eeprom->cbus_function[2];
            break;
        case CBUS_FUNCTION_3:
            *value = briteblox->eeprom->cbus_function[3];
            break;
        case CBUS_FUNCTION_4:
            *value = briteblox->eeprom->cbus_function[4];
            break;
        case CBUS_FUNCTION_5:
            *value = briteblox->eeprom->cbus_function[5];
            break;
        case CBUS_FUNCTION_6:
            *value = briteblox->eeprom->cbus_function[6];
            break;
        case CBUS_FUNCTION_7:
            *value = briteblox->eeprom->cbus_function[7];
            break;
        case CBUS_FUNCTION_8:
            *value = briteblox->eeprom->cbus_function[8];
            break;
        case CBUS_FUNCTION_9:
            *value = briteblox->eeprom->cbus_function[8];
            break;
        case HIGH_CURRENT:
            *value = briteblox->eeprom->high_current;
            break;
        case HIGH_CURRENT_A:
            *value = briteblox->eeprom->high_current_a;
            break;
        case HIGH_CURRENT_B:
            *value = briteblox->eeprom->high_current_b;
            break;
        case INVERT:
            *value = briteblox->eeprom->invert;
            break;
        case GROUP0_DRIVE:
            *value = briteblox->eeprom->group0_drive;
            break;
        case GROUP0_SCHMITT:
            *value = briteblox->eeprom->group0_schmitt;
            break;
        case GROUP0_SLEW:
            *value = briteblox->eeprom->group0_slew;
            break;
        case GROUP1_DRIVE:
            *value = briteblox->eeprom->group1_drive;
            break;
        case GROUP1_SCHMITT:
            *value = briteblox->eeprom->group1_schmitt;
            break;
        case GROUP1_SLEW:
            *value = briteblox->eeprom->group1_slew;
            break;
        case GROUP2_DRIVE:
            *value = briteblox->eeprom->group2_drive;
            break;
        case GROUP2_SCHMITT:
            *value = briteblox->eeprom->group2_schmitt;
            break;
        case GROUP2_SLEW:
            *value = briteblox->eeprom->group2_slew;
            break;
        case GROUP3_DRIVE:
            *value = briteblox->eeprom->group3_drive;
            break;
        case GROUP3_SCHMITT:
            *value = briteblox->eeprom->group3_schmitt;
            break;
        case GROUP3_SLEW:
            *value = briteblox->eeprom->group3_slew;
            break;
        case POWER_SAVE:
            *value = briteblox->eeprom->powersave;
            break;
        case CLOCK_POLARITY:
            *value = briteblox->eeprom->clock_polarity;
            break;
        case DATA_ORDER:
            *value = briteblox->eeprom->data_order;
            break;
        case FLOW_CONTROL:
            *value = briteblox->eeprom->flow_control;
            break;
        case CHIP_TYPE:
            *value = briteblox->eeprom->chip;
            break;
        case CHIP_SIZE:
            *value = briteblox->eeprom->size;
            break;
        default:
            briteblox_error_return(-1, "Request for unknown EEPROM value");
    }
    return 0;
}

/**
   Set a value in the decoded EEPROM Structure
   No parameter checking is performed

   \param briteblox pointer to briteblox_context
   \param value_name Enum of the value to set
   \param value to set

   \retval 0: all fine
   \retval -1: Value doesn't exist
   \retval -2: Value not user settable
*/
int briteblox_set_eeprom_value(struct briteblox_context *briteblox, enum briteblox_eeprom_value value_name, int value)
{
    switch (value_name)
    {
        case VENDOR_ID:
            briteblox->eeprom->vendor_id = value;
            break;
        case PRODUCT_ID:
            briteblox->eeprom->product_id = value;
            break;
        case RELEASE_NUMBER:
            briteblox->eeprom->release_number = value;
            break;
        case SELF_POWERED:
            briteblox->eeprom->self_powered = value;
            break;
        case REMOTE_WAKEUP:
            briteblox->eeprom->remote_wakeup = value;
            break;
        case IS_NOT_PNP:
            briteblox->eeprom->is_not_pnp = value;
            break;
        case SUSPEND_DBUS7:
            briteblox->eeprom->suspend_dbus7 = value;
            break;
        case IN_IS_ISOCHRONOUS:
            briteblox->eeprom->in_is_isochronous = value;
            break;
        case OUT_IS_ISOCHRONOUS:
            briteblox->eeprom->out_is_isochronous = value;
            break;
        case SUSPEND_PULL_DOWNS:
            briteblox->eeprom->suspend_pull_downs = value;
            break;
        case USE_SERIAL:
            briteblox->eeprom->use_serial = value;
            break;
        case USB_VERSION:
            briteblox->eeprom->usb_version = value;
            break;
        case USE_USB_VERSION:
            briteblox->eeprom->use_usb_version = value;
            break;
        case MAX_POWER:
            briteblox->eeprom->max_power = value;
            break;
        case CHANNEL_A_TYPE:
            briteblox->eeprom->channel_a_type = value;
            break;
        case CHANNEL_B_TYPE:
            briteblox->eeprom->channel_b_type = value;
            break;
        case CHANNEL_A_DRIVER:
            briteblox->eeprom->channel_a_driver = value;
            break;
        case CHANNEL_B_DRIVER:
            briteblox->eeprom->channel_b_driver = value;
            break;
        case CHANNEL_C_DRIVER:
            briteblox->eeprom->channel_c_driver = value;
            break;
        case CHANNEL_D_DRIVER:
            briteblox->eeprom->channel_d_driver = value;
            break;
        case CHANNEL_A_RS485:
            briteblox->eeprom->channel_a_rs485enable = value;
            break;
        case CHANNEL_B_RS485:
            briteblox->eeprom->channel_b_rs485enable = value;
            break;
        case CHANNEL_C_RS485:
            briteblox->eeprom->channel_c_rs485enable = value;
            break;
        case CHANNEL_D_RS485:
            briteblox->eeprom->channel_d_rs485enable = value;
            break;
        case CBUS_FUNCTION_0:
            briteblox->eeprom->cbus_function[0] = value;
            break;
        case CBUS_FUNCTION_1:
            briteblox->eeprom->cbus_function[1] = value;
            break;
        case CBUS_FUNCTION_2:
            briteblox->eeprom->cbus_function[2] = value;
            break;
        case CBUS_FUNCTION_3:
            briteblox->eeprom->cbus_function[3] = value;
            break;
        case CBUS_FUNCTION_4:
            briteblox->eeprom->cbus_function[4] = value;
            break;
        case CBUS_FUNCTION_5:
            briteblox->eeprom->cbus_function[5] = value;
            break;
        case CBUS_FUNCTION_6:
            briteblox->eeprom->cbus_function[6] = value;
            break;
        case CBUS_FUNCTION_7:
            briteblox->eeprom->cbus_function[7] = value;
            break;
        case CBUS_FUNCTION_8:
            briteblox->eeprom->cbus_function[8] = value;
            break;
        case CBUS_FUNCTION_9:
            briteblox->eeprom->cbus_function[9] = value;
            break;
        case HIGH_CURRENT:
            briteblox->eeprom->high_current = value;
            break;
        case HIGH_CURRENT_A:
            briteblox->eeprom->high_current_a = value;
            break;
        case HIGH_CURRENT_B:
            briteblox->eeprom->high_current_b = value;
            break;
        case INVERT:
            briteblox->eeprom->invert = value;
            break;
        case GROUP0_DRIVE:
            briteblox->eeprom->group0_drive = value;
            break;
        case GROUP0_SCHMITT:
            briteblox->eeprom->group0_schmitt = value;
            break;
        case GROUP0_SLEW:
            briteblox->eeprom->group0_slew = value;
            break;
        case GROUP1_DRIVE:
            briteblox->eeprom->group1_drive = value;
            break;
        case GROUP1_SCHMITT:
            briteblox->eeprom->group1_schmitt = value;
            break;
        case GROUP1_SLEW:
            briteblox->eeprom->group1_slew = value;
            break;
        case GROUP2_DRIVE:
            briteblox->eeprom->group2_drive = value;
            break;
        case GROUP2_SCHMITT:
            briteblox->eeprom->group2_schmitt = value;
            break;
        case GROUP2_SLEW:
            briteblox->eeprom->group2_slew = value;
            break;
        case GROUP3_DRIVE:
            briteblox->eeprom->group3_drive = value;
            break;
        case GROUP3_SCHMITT:
            briteblox->eeprom->group3_schmitt = value;
            break;
        case GROUP3_SLEW:
            briteblox->eeprom->group3_slew = value;
            break;
        case CHIP_TYPE:
            briteblox->eeprom->chip = value;
            break;
        case POWER_SAVE:
            briteblox->eeprom->powersave = value;
            break;
        case CLOCK_POLARITY:
            briteblox->eeprom->clock_polarity = value;
            break;
        case DATA_ORDER:
            briteblox->eeprom->data_order = value;
            break;
        case FLOW_CONTROL:
            briteblox->eeprom->flow_control = value;
            break;
        case CHIP_SIZE:
            briteblox_error_return(-2, "EEPROM Value can't be changed");
        default :
            briteblox_error_return(-1, "Request to unknown EEPROM value");
    }
    briteblox->eeprom->initialized_for_connected_device = 0;
    return 0;
}

/** Get the read-only buffer to the binary EEPROM content

    \param briteblox pointer to briteblox_context
    \param buf buffer to receive EEPROM content
    \param size Size of receiving buffer

    \retval 0: All fine
    \retval -1: struct briteblox_contxt or briteblox_eeprom missing
    \retval -2: Not enough room to store eeprom
*/
int briteblox_get_eeprom_buf(struct briteblox_context *briteblox, unsigned char * buf, int size)
{
    if (!briteblox || !(briteblox->eeprom))
        briteblox_error_return(-1, "No appropriate structure");

    if (!buf || size < briteblox->eeprom->size)
        briteblox_error_return(-1, "Not enough room to store eeprom");

    // Only copy up to BRITEBLOX_MAX_EEPROM_SIZE bytes
    if (size > BRITEBLOX_MAX_EEPROM_SIZE)
        size = BRITEBLOX_MAX_EEPROM_SIZE;

    memcpy(buf, briteblox->eeprom->buf, size);

    return 0;
}

/** Set the EEPROM content from the user-supplied prefilled buffer

    \param briteblox pointer to briteblox_context
    \param buf buffer to read EEPROM content
    \param size Size of buffer

    \retval 0: All fine
    \retval -1: struct briteblox_contxt or briteblox_eeprom of buf missing
*/
int briteblox_set_eeprom_buf(struct briteblox_context *briteblox, const unsigned char * buf, int size)
{
    if (!briteblox || !(briteblox->eeprom) || !buf)
        briteblox_error_return(-1, "No appropriate structure");

    // Only copy up to BRITEBLOX_MAX_EEPROM_SIZE bytes
    if (size > BRITEBLOX_MAX_EEPROM_SIZE)
        size = BRITEBLOX_MAX_EEPROM_SIZE;

    memcpy(briteblox->eeprom->buf, buf, size);

    return 0;
}

/**
    Read eeprom location

    \param briteblox pointer to briteblox_context
    \param eeprom_addr Address of eeprom location to be read
    \param eeprom_val Pointer to store read eeprom location

    \retval  0: all fine
    \retval -1: read failed
    \retval -2: USB device unavailable
*/
int briteblox_read_eeprom_location (struct briteblox_context *briteblox, int eeprom_addr, unsigned short *eeprom_val)
{
    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_IN_REQTYPE, SIO_READ_EEPROM_REQUEST, 0, eeprom_addr, (unsigned char *)eeprom_val, 2, briteblox->usb_read_timeout) != 2)
        briteblox_error_return(-1, "reading eeprom failed");

    return 0;
}

/**
    Read eeprom

    \param briteblox pointer to briteblox_context

    \retval  0: all fine
    \retval -1: read failed
    \retval -2: USB device unavailable
*/
int briteblox_read_eeprom(struct briteblox_context *briteblox)
{
    int i;
    unsigned char *buf;

    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");
    buf = briteblox->eeprom->buf;

    for (i = 0; i < BRITEBLOX_MAX_EEPROM_SIZE/2; i++)
    {
        if (libusb_control_transfer(
                    briteblox->usb_dev, BRITEBLOX_DEVICE_IN_REQTYPE,SIO_READ_EEPROM_REQUEST, 0, i,
                    buf+(i*2), 2, briteblox->usb_read_timeout) != 2)
            briteblox_error_return(-1, "reading eeprom failed");
    }

    if (briteblox->type == TYPE_R)
        briteblox->eeprom->size = 0x80;
    /*    Guesses size of eeprom by comparing halves
          - will not work with blank eeprom */
    else if (strrchr((const char *)buf, 0xff) == ((const char *)buf +BRITEBLOX_MAX_EEPROM_SIZE -1))
        briteblox->eeprom->size = -1;
    else if (memcmp(buf,&buf[0x80],0x80) == 0)
        briteblox->eeprom->size = 0x80;
    else if (memcmp(buf,&buf[0x40],0x40) == 0)
        briteblox->eeprom->size = 0x40;
    else
        briteblox->eeprom->size = 0x100;
    return 0;
}

/*
    briteblox_read_chipid_shift does the bitshift operation needed for the BRITEBLOXChip-ID
    Function is only used internally
    \internal
*/
static unsigned char briteblox_read_chipid_shift(unsigned char value)
{
    return ((value & 1) << 1) |
           ((value & 2) << 5) |
           ((value & 4) >> 2) |
           ((value & 8) << 4) |
           ((value & 16) >> 1) |
           ((value & 32) >> 1) |
           ((value & 64) >> 4) |
           ((value & 128) >> 2);
}

/**
    Read the BRITEBLOXChip-ID from R-type devices

    \param briteblox pointer to briteblox_context
    \param chipid Pointer to store BRITEBLOXChip-ID

    \retval  0: all fine
    \retval -1: read failed
    \retval -2: USB device unavailable
*/
int briteblox_read_chipid(struct briteblox_context *briteblox, unsigned int *chipid)
{
    unsigned int a = 0, b = 0;

    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_IN_REQTYPE, SIO_READ_EEPROM_REQUEST, 0, 0x43, (unsigned char *)&a, 2, briteblox->usb_read_timeout) == 2)
    {
        a = a << 8 | a >> 8;
        if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_IN_REQTYPE, SIO_READ_EEPROM_REQUEST, 0, 0x44, (unsigned char *)&b, 2, briteblox->usb_read_timeout) == 2)
        {
            b = b << 8 | b >> 8;
            a = (a << 16) | (b & 0xFFFF);
            a = briteblox_read_chipid_shift(a) | briteblox_read_chipid_shift(a>>8)<<8
                | briteblox_read_chipid_shift(a>>16)<<16 | briteblox_read_chipid_shift(a>>24)<<24;
            *chipid = a ^ 0xa5f0f7d1;
            return 0;
        }
    }

    briteblox_error_return(-1, "read of BRITEBLOXChip-ID failed");
}

/**
    Write eeprom location

    \param briteblox pointer to briteblox_context
    \param eeprom_addr Address of eeprom location to be written
    \param eeprom_val Value to be written

    \retval  0: all fine
    \retval -1: write failed
    \retval -2: USB device unavailable
    \retval -3: Invalid access to checksum protected area below 0x80
    \retval -4: Device can't access unprotected area
    \retval -5: Reading chip type failed
*/
int briteblox_write_eeprom_location(struct briteblox_context *briteblox, int eeprom_addr,
                               unsigned short eeprom_val)
{
    int chip_type_location;
    unsigned short chip_type;

    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    if (eeprom_addr <0x80)
        briteblox_error_return(-2, "Invalid access to checksum protected area  below 0x80");


    switch (briteblox->type)
    {
        case TYPE_BM:
        case  TYPE_2232C:
            chip_type_location = 0x14;
            break;
        case TYPE_2232H:
        case TYPE_4232H:
            chip_type_location = 0x18;
            break;
        case TYPE_232H:
            chip_type_location = 0x1e;
            break;
        default:
            briteblox_error_return(-4, "Device can't access unprotected area");
    }

    if (briteblox_read_eeprom_location( briteblox, chip_type_location>>1, &chip_type))
        briteblox_error_return(-5, "Reading failed");
    fprintf(stderr," loc 0x%04x val 0x%04x\n", chip_type_location,chip_type);
    if ((chip_type & 0xff) != 0x66)
    {
        briteblox_error_return(-6, "EEPROM is not of 93x66");
    }

    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_OUT_REQTYPE,
                                SIO_WRITE_EEPROM_REQUEST, eeprom_val, eeprom_addr,
                                NULL, 0, briteblox->usb_write_timeout) != 0)
        briteblox_error_return(-1, "unable to write eeprom");

    return 0;
}

/**
    Write eeprom

    \param briteblox pointer to briteblox_context

    \retval  0: all fine
    \retval -1: read failed
    \retval -2: USB device unavailable
    \retval -3: EEPROM not initialized for the connected device;
*/
int briteblox_write_eeprom(struct briteblox_context *briteblox)
{
    unsigned short usb_val, status;
    int i, ret;
    unsigned char *eeprom;

    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    if(briteblox->eeprom->initialized_for_connected_device == 0)
        briteblox_error_return(-3, "EEPROM not initialized for the connected device");

    eeprom = briteblox->eeprom->buf;

    /* These commands were traced while running MProg */
    if ((ret = briteblox_usb_reset(briteblox)) != 0)
        return ret;
    if ((ret = briteblox_poll_modem_status(briteblox, &status)) != 0)
        return ret;
    if ((ret = briteblox_set_latency_timer(briteblox, 0x77)) != 0)
        return ret;

    for (i = 0; i < briteblox->eeprom->size/2; i++)
    {
        /* Do not try to write to reserved area */
        if ((briteblox->type == TYPE_230X) && (i == 0x40))
        {
            i = 0x50;
        }
        usb_val = eeprom[i*2];
        usb_val += eeprom[(i*2)+1] << 8;
        if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_OUT_REQTYPE,
                                    SIO_WRITE_EEPROM_REQUEST, usb_val, i,
                                    NULL, 0, briteblox->usb_write_timeout) < 0)
            briteblox_error_return(-1, "unable to write eeprom");
    }

    return 0;
}

/**
    Erase eeprom

    This is not supported on FT232R/FT245R according to the MProg manual from BRITEBLOX.

    \param briteblox pointer to briteblox_context

    \retval  0: all fine
    \retval -1: erase failed
    \retval -2: USB device unavailable
    \retval -3: Writing magic failed
    \retval -4: Read EEPROM failed
    \retval -5: Unexpected EEPROM value
*/
#define MAGIC 0x55aa
int briteblox_erase_eeprom(struct briteblox_context *briteblox)
{
    unsigned short eeprom_value;
    if (briteblox == NULL || briteblox->usb_dev == NULL)
        briteblox_error_return(-2, "USB device unavailable");

    if (briteblox->type == TYPE_R)
    {
        briteblox->eeprom->chip = 0;
        return 0;
    }

    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_OUT_REQTYPE, SIO_ERASE_EEPROM_REQUEST,
                                0, 0, NULL, 0, briteblox->usb_write_timeout) < 0)
        briteblox_error_return(-1, "unable to erase eeprom");


    /* detect chip type by writing 0x55AA as magic at word position 0xc0
       Chip is 93x46 if magic is read at word position 0x00, as wraparound happens around 0x40
       Chip is 93x56 if magic is read at word position 0x40, as wraparound happens around 0x80
       Chip is 93x66 if magic is only read at word position 0xc0*/
    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_OUT_REQTYPE,
                                SIO_WRITE_EEPROM_REQUEST, MAGIC, 0xc0,
                                NULL, 0, briteblox->usb_write_timeout) != 0)
        briteblox_error_return(-3, "Writing magic failed");
    if (briteblox_read_eeprom_location( briteblox, 0x00, &eeprom_value))
        briteblox_error_return(-4, "Reading failed");
    if (eeprom_value == MAGIC)
    {
        briteblox->eeprom->chip = 0x46;
    }
    else
    {
        if (briteblox_read_eeprom_location( briteblox, 0x40, &eeprom_value))
            briteblox_error_return(-4, "Reading failed");
        if (eeprom_value == MAGIC)
            briteblox->eeprom->chip = 0x56;
        else
        {
            if (briteblox_read_eeprom_location( briteblox, 0xc0, &eeprom_value))
                briteblox_error_return(-4, "Reading failed");
            if (eeprom_value == MAGIC)
                briteblox->eeprom->chip = 0x66;
            else
            {
                briteblox->eeprom->chip = -1;
            }
        }
    }
    if (libusb_control_transfer(briteblox->usb_dev, BRITEBLOX_DEVICE_OUT_REQTYPE, SIO_ERASE_EEPROM_REQUEST,
                                0, 0, NULL, 0, briteblox->usb_write_timeout) < 0)
        briteblox_error_return(-1, "unable to erase eeprom");
    return 0;
}

/**
    Get string representation for last error code

    \param briteblox pointer to briteblox_context

    \retval Pointer to error string
*/
char *briteblox_get_error_string (struct briteblox_context *briteblox)
{
    if (briteblox == NULL)
        return "";

    return briteblox->error_str;
}

/* @} end of doxygen libbriteblox group */
