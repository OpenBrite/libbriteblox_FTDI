/***************************************************************************
                          briteblox.cpp  -  C++ wraper for libbriteblox
                             -------------------
    begin                : Mon Oct 13 2008
    copyright            : (C) 2008-2014 by Marek Vavruša / libbriteblox developers
    email                : opensource@intra2net.com and marek@vavrusa.com
 ***************************************************************************/
/*
Copyright (C) 2008-2014 by Marek Vavruša / libbriteblox developers

The software in this package is distributed under the GNU General
Public License version 2 (with a special exception described below).

A copy of GNU General Public License (GPL) is included in this distribution,
in the file COPYING.GPL.

As a special exception, if other files instantiate templates or use macros
or inline functions from this file, or you compile this file and link it
with other works to produce a work based on this file, this file
does not by itself cause the resulting work to be covered
by the GNU General Public License.

However the source code for this file must still be made available
in accordance with section (3) of the GNU General Public License.

This exception does not invalidate any other reasons why a work based
on this file might be covered by the GNU General Public License.
*/
#include <libusb.h>
#include "briteblox.hpp"
#include "briteblox_i.h"
#include "briteblox.h"

namespace BriteBlox
{

class Context::Private
{
public:
    Private()
            : open(false), briteblox(0), dev(0)
    {
        briteblox = briteblox_new();
    }

    ~Private()
    {
        if (open)
            briteblox_usb_close(briteblox);

        briteblox_free(briteblox);
    }

    bool open;

    struct briteblox_context* briteblox;
    struct libusb_device* dev;

    std::string vendor;
    std::string description;
    std::string serial;
};

/*! \brief Constructor.
 */
Context::Context()
        : d( new Private() )
{
}

/*! \brief Destructor.
 */
Context::~Context()
{
}

bool Context::is_open()
{
    return d->open;
}

int Context::open(int vendor, int product)
{
    // Open device
    int ret = briteblox_usb_open(d->briteblox, vendor, product);

    if (ret < 0)
       return ret;

    return get_strings_and_reopen();
}

int Context::open(int vendor, int product, const std::string& description, const std::string& serial, unsigned int index)
{
    // translate empty strings to NULL
    // -> do not use them to find the device (vs. require an empty string to be set in the EEPROM)
    const char* c_description=NULL;
    const char* c_serial=NULL;
    if (!description.empty())
        c_description=description.c_str();
    if (!serial.empty())
        c_serial=serial.c_str();

    int ret = briteblox_usb_open_desc_index(d->briteblox, vendor, product, c_description, c_serial, index);

    if (ret < 0)
       return ret;

    return get_strings_and_reopen();
}

int Context::open(const std::string& description)
{
    int ret = briteblox_usb_open_string(d->briteblox, description.c_str());

    if (ret < 0)
       return ret;

    return get_strings_and_reopen();
}

int Context::open(struct libusb_device *dev)
{
    if (dev != 0)
        d->dev = dev;

    if (d->dev == 0)
        return -1;

    return get_strings_and_reopen();
}

int Context::close()
{
    d->open = false;
    d->dev = 0;
    return briteblox_usb_close(d->briteblox);
}

int Context::reset()
{
    return briteblox_usb_reset(d->briteblox);
}

int Context::flush(int mask)
{
    int ret = 1;

    if (mask & Input)
        ret &= briteblox_usb_purge_rx_buffer(d->briteblox);
    if (mask & Output)
        ret &= briteblox_usb_purge_tx_buffer(d->briteblox);

    return ret;
}

int Context::set_interface(enum briteblox_interface interface)
{
    return briteblox_set_interface(d->briteblox, interface);
}

void Context::set_usb_device(struct libusb_device_handle *dev)
{
    briteblox_set_usbdev(d->briteblox, dev);
    d->dev = libusb_get_device(dev);
}

int Context::set_baud_rate(int baudrate)
{
    return briteblox_set_baudrate(d->briteblox, baudrate);
}

int Context::set_line_property(enum briteblox_bits_type bits, enum briteblox_stopbits_type sbit, enum briteblox_parity_type parity)
{
    return briteblox_set_line_property(d->briteblox, bits, sbit, parity);
}

int Context::set_line_property(enum briteblox_bits_type bits, enum briteblox_stopbits_type sbit, enum briteblox_parity_type parity, enum briteblox_break_type break_type)
{
    return briteblox_set_line_property2(d->briteblox, bits, sbit, parity, break_type);
}

int Context::get_usb_read_timeout() const
{
    return d->briteblox->usb_read_timeout;
}

void Context::set_usb_read_timeout(int usb_read_timeout)
{
    d->briteblox->usb_read_timeout = usb_read_timeout;
}

int Context::get_usb_write_timeout() const
{
    return d->briteblox->usb_write_timeout;
}

void Context::set_usb_write_timeout(int usb_write_timeout)
{
    d->briteblox->usb_write_timeout = usb_write_timeout;
}

int Context::read(unsigned char *buf, int size)
{
    return briteblox_read_data(d->briteblox, buf, size);
}

int Context::set_read_chunk_size(unsigned int chunksize)
{
    return briteblox_read_data_set_chunksize(d->briteblox, chunksize);
}

int Context::read_chunk_size()
{
    unsigned chunk = -1;
    if (briteblox_read_data_get_chunksize(d->briteblox, &chunk) < 0)
        return -1;

    return chunk;
}

int Context::write(unsigned char *buf, int size)
{
    return briteblox_write_data(d->briteblox, buf, size);
}

int Context::set_write_chunk_size(unsigned int chunksize)
{
    return briteblox_write_data_set_chunksize(d->briteblox, chunksize);
}

int Context::write_chunk_size()
{
    unsigned chunk = -1;
    if (briteblox_write_data_get_chunksize(d->briteblox, &chunk) < 0)
        return -1;

    return chunk;
}

int Context::set_flow_control(int flowctrl)
{
    return briteblox_setflowctrl(d->briteblox, flowctrl);
}

int Context::set_modem_control(int mask)
{
    int dtr = 0, rts = 0;

    if (mask & Dtr)
        dtr = 1;
    if (mask & Rts)
        rts = 1;

    return briteblox_setdtr_rts(d->briteblox, dtr, rts);
}

int Context::set_dtr(bool state)
{
    return briteblox_setdtr(d->briteblox, state);
}

int Context::set_rts(bool state)
{
    return briteblox_setrts(d->briteblox, state);
}

int Context::set_latency(unsigned char latency)
{
    return briteblox_set_latency_timer(d->briteblox, latency);
}

unsigned Context::latency()
{
    unsigned char latency = 0;
    briteblox_get_latency_timer(d->briteblox, &latency);
    return latency;
}

unsigned short Context::poll_modem_status()
{
    unsigned short status = 0;
    briteblox_poll_modem_status(d->briteblox, &status);
    return status;
}

int Context::set_event_char(unsigned char eventch, unsigned char enable)
{
    return briteblox_set_event_char(d->briteblox, eventch, enable);
}

int Context::set_error_char(unsigned char errorch, unsigned char enable)
{
    return briteblox_set_error_char(d->briteblox, errorch, enable);
}

int Context::set_bitmode(unsigned char bitmask, unsigned char mode)
{
    return briteblox_set_bitmode(d->briteblox, bitmask, mode);
}

int Context::set_bitmode(unsigned char bitmask, enum briteblox_mpsse_mode mode)
{
    return briteblox_set_bitmode(d->briteblox, bitmask, mode);
}

int Context::bitbang_disable()
{
    return briteblox_disable_bitbang(d->briteblox);
}

int Context::read_pins(unsigned char *pins)
{
    return briteblox_read_pins(d->briteblox, pins);
}

char* Context::error_string()
{
    return briteblox_get_error_string(d->briteblox);
}

int Context::get_strings()
{
    // Prepare buffers
    char vendor[512], desc[512], serial[512];

    int ret = briteblox_usb_get_strings(d->briteblox, d->dev, vendor, 512, desc, 512, serial, 512);

    if (ret < 0)
        return -1;

    d->vendor = vendor;
    d->description = desc;
    d->serial = serial;

    return 1;
}

int Context::get_strings_and_reopen()
{
    if ( d->dev == 0 )
    {
        d->dev = libusb_get_device(d->briteblox->usb_dev);
    }

    // Get device strings (closes device)
    int ret=get_strings();
    if (ret < 0)
    {
        d->open = 0;
        return ret;
    }

    // Reattach device
    ret = briteblox_usb_open_dev(d->briteblox, d->dev);
    d->open = (ret >= 0);

    return ret;
}

/*! \brief Device strings properties.
 */
const std::string& Context::vendor()
{
    return d->vendor;
}

/*! \brief Device strings properties.
 */
const std::string& Context::description()
{
    return d->description;
}

/*! \brief Device strings properties.
 */
const std::string& Context::serial()
{
    return d->serial;
}

void Context::set_context(struct briteblox_context* context)
{
    briteblox_free(d->briteblox);
    d->briteblox = context;
}

void Context::set_usb_device(struct libusb_device *dev)
{
    d->dev = dev;
}

struct briteblox_context* Context::context()
{
    return d->briteblox;
}

class Eeprom::Private
{
public:
    Private()
            : context(0)
    {}

    struct briteblox_eeprom eeprom;
    struct briteblox_context* context;
};

Eeprom::Eeprom(Context* parent)
        : d ( new Private() )
{
    d->context = parent->context();
}

Eeprom::~Eeprom()
{
}

int Eeprom::init_defaults(char* manufacturer, char *product, char * serial)
{
    return briteblox_eeprom_initdefaults(d->context, manufacturer, product, serial);
}

int Eeprom::chip_id(unsigned int *chipid)
{
    return briteblox_read_chipid(d->context, chipid);
}

int Eeprom::build(unsigned char *output)
{
    return briteblox_eeprom_build(d->context);
}

int Eeprom::read(unsigned char *eeprom)
{
    return briteblox_read_eeprom(d->context);
}

int Eeprom::write(unsigned char *eeprom)
{
    return briteblox_write_eeprom(d->context);
}

int Eeprom::read_location(int eeprom_addr, unsigned short *eeprom_val)
{
    return briteblox_read_eeprom_location(d->context, eeprom_addr, eeprom_val);
}

int Eeprom::write_location(int eeprom_addr, unsigned short eeprom_val)
{
    return briteblox_write_eeprom_location(d->context, eeprom_addr, eeprom_val);
}

int Eeprom::erase()
{
    return briteblox_erase_eeprom(d->context);
}

class List::Private
{
public:
    Private(struct briteblox_device_list* _devlist)
            : devlist(_devlist)
    {}

    ~Private()
    {
        if(devlist)
            briteblox_list_free(&devlist);
    }

    std::list<Context> list;
    struct briteblox_device_list* devlist;
};

List::List(struct briteblox_device_list* devlist)
        : d( new Private(devlist) )
{
    if (devlist != 0)
    {
        // Iterate list
        for (; devlist != 0; devlist = devlist->next)
        {
            Context c;
            c.set_usb_device(devlist->dev);
            c.get_strings();
            d->list.push_back(c);
        }
    }
}

List::~List()
{
}

/**
* Return begin iterator for accessing the contained list elements
* @return Iterator
*/
List::iterator List::begin()
{
    return d->list.begin();
}

/**
* Return end iterator for accessing the contained list elements
* @return Iterator
*/
List::iterator List::end()
{
    return d->list.end();
}

/**
* Return begin iterator for accessing the contained list elements
* @return Const iterator
*/
List::const_iterator List::begin() const
{
    return d->list.begin();
}

/**
* Return end iterator for accessing the contained list elements
* @return Const iterator
*/
List::const_iterator List::end() const
{
    return d->list.end();
}

/**
* Return begin reverse iterator for accessing the contained list elements
* @return Reverse iterator
*/
List::reverse_iterator List::rbegin()
{
    return d->list.rbegin();
}

/**
* Return end reverse iterator for accessing the contained list elements
* @return Reverse iterator
*/
List::reverse_iterator List::rend()
{
    return d->list.rend();
}

/**
* Return begin reverse iterator for accessing the contained list elements
* @return Const reverse iterator
*/
List::const_reverse_iterator List::rbegin() const
{
    return d->list.rbegin();
}

/**
* Return end reverse iterator for accessing the contained list elements
* @return Const reverse iterator
*/
List::const_reverse_iterator List::rend() const
{
    return d->list.rend();

}

/**
* Get number of elements stored in the list
* @return Number of elements
*/
List::ListType::size_type List::size() const
{
    return d->list.size();
}

/**
* Check if list is empty
* @return True if empty, false otherwise
*/
bool List::empty() const
{
    return d->list.empty();
}

/**
 * Removes all elements. Invalidates all iterators.
 * Do it in a non-throwing way and also make
 * sure we really free the allocated memory.
 */
void List::clear()
{
    ListType().swap(d->list);

    // Free device list
    if (d->devlist)
    {
        briteblox_list_free(&d->devlist);
        d->devlist = 0;
    }
}

/**
 * Appends a copy of the element as the new last element.
 * @param element Value to copy and append
*/
void List::push_back(const Context& element)
{
    d->list.push_back(element);
}

/**
 * Adds a copy of the element as the new first element.
 * @param element Value to copy and add
*/
void List::push_front(const Context& element)
{
    d->list.push_front(element);
}

/**
 * Erase one element pointed by iterator
 * @param pos Element to erase
 * @return Position of the following element (or end())
*/
List::iterator List::erase(iterator pos)
{
    return d->list.erase(pos);
}

/**
 * Erase a range of elements
 * @param beg Begin of range
 * @param end End of range
 * @return Position of the element after the erased range (or end())
*/
List::iterator List::erase(iterator beg, iterator end)
{
    return d->list.erase(beg, end);
}

List* List::find_all(Context &context, int vendor, int product)
{
    struct briteblox_device_list* dlist = 0;
    briteblox_usb_find_all(context.context(), &dlist, vendor, product);
    return new List(dlist);
}

}
