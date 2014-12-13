/***************************************************************************
                             main.c  -  description
                           -------------------
    begin                : Mon Apr  7 12:05:22 CEST 2003
    copyright            : (C) 2003-2014 by Intra2net AG and the libbriteblox developers
    email                : opensource@intra2net.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *
 *                                                                         *
 ***************************************************************************/

/*
 TODO:
    - Merge Uwe's eeprom tool. Current features:
        - Init eeprom defaults based upon eeprom type
        - Read -> Already there
        - Write -> Already there
        - Erase -> Already there
        - Decode on stdout
        - Ability to find device by PID/VID, product name or serial

 TODO nice-to-have:
    - Out-of-the-box compatibility with BRITEBLOX's eeprom tool configuration files
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <confuse.h>
#include <libusb.h>
#include <briteblox.h>
#include <briteblox_eeprom_version.h>

static int str_to_cbus(char *str, int max_allowed)
{
#define MAX_OPTION 14
    const char* options[MAX_OPTION] =
    {
        "TXDEN", "PWREN", "RXLED", "TXLED", "TXRXLED", "SLEEP",
        "CLK48", "CLK24", "CLK12", "CLK6",
        "IO_MODE", "BITBANG_WR", "BITBANG_RD", "SPECIAL"
    };
    int i;
    max_allowed += 1;
    if (max_allowed > MAX_OPTION) max_allowed = MAX_OPTION;
    for (i=0; i<max_allowed; i++)
    {
        if (!(strcmp(options[i], str)))
        {
            return i;
        }
    }
    printf("WARNING: Invalid cbus option '%s'\n", str);
    return 0;
}

/**
 * @brief Set eeprom value
 *
 * \param briteblox pointer to briteblox_context
 * \param value_name Enum of the value to set
 * \param value Value to set
 *
 * Function will abort the program on error
 **/
static void eeprom_set_value(struct briteblox_context *briteblox, enum briteblox_eeprom_value value_name, int value)
{
    if (briteblox_set_eeprom_value(briteblox, value_name, value) < 0)
    {
        printf("Unable to set eeprom value %d: %s. Aborting\n", value_name, briteblox_get_error_string(briteblox));
        exit (-1);
    }
}

/**
 * @brief Get eeprom value
 *
 * \param briteblox pointer to briteblox_context
 * \param value_name Enum of the value to get
 * \param value Value to get
 *
 * Function will abort the program on error
 **/
static void eeprom_get_value(struct briteblox_context *briteblox, enum briteblox_eeprom_value value_name, int *value)
{
    if (briteblox_get_eeprom_value(briteblox, value_name, value) < 0)
    {
        printf("Unable to get eeprom value %d: %s. Aborting\n", value_name, briteblox_get_error_string(briteblox));
        exit (-1);
    }
}

int main(int argc, char *argv[])
{
    /*
    configuration options
    */
    cfg_opt_t opts[] =
    {
        CFG_INT("vendor_id", 0, 0),
        CFG_INT("product_id", 0, 0),
        CFG_BOOL("self_powered", cfg_true, 0),
        CFG_BOOL("remote_wakeup", cfg_true, 0),
        CFG_BOOL("in_is_isochronous", cfg_false, 0),
        CFG_BOOL("out_is_isochronous", cfg_false, 0),
        CFG_BOOL("suspend_pull_downs", cfg_false, 0),
        CFG_BOOL("use_serial", cfg_false, 0),
        CFG_BOOL("change_usb_version", cfg_false, 0),
        CFG_INT("usb_version", 0, 0),
        CFG_INT("default_pid", 0x7AD0, 0),
        CFG_INT("max_power", 0, 0),
        CFG_STR("manufacturer", "Acme Inc.", 0),
        CFG_STR("product", "USB Serial Converter", 0),
        CFG_STR("serial", "08-15", 0),
        CFG_INT("eeprom_type", 0x00, 0),
        CFG_STR("filename", "", 0),
        CFG_BOOL("flash_raw", cfg_false, 0),
        CFG_BOOL("high_current", cfg_false, 0),
        CFG_STR_LIST("cbus0", "{TXDEN,PWREN,RXLED,TXLED,TXRXLED,SLEEP,CLK48,CLK24,CLK12,CLK6,IO_MODE,BITBANG_WR,BITBANG_RD,SPECIAL}", 0),
        CFG_STR_LIST("cbus1", "{TXDEN,PWREN,RXLED,TXLED,TXRXLED,SLEEP,CLK48,CLK24,CLK12,CLK6,IO_MODE,BITBANG_WR,BITBANG_RD,SPECIAL}", 0),
        CFG_STR_LIST("cbus2", "{TXDEN,PWREN,RXLED,TXLED,TXRXLED,SLEEP,CLK48,CLK24,CLK12,CLK6,IO_MODE,BITBANG_WR,BITBANG_RD,SPECIAL}", 0),
        CFG_STR_LIST("cbus3", "{TXDEN,PWREN,RXLED,TXLED,TXRXLED,SLEEP,CLK48,CLK24,CLK12,CLK6,IO_MODE,BITBANG_WR,BITBANG_RD,SPECIAL}", 0),
        CFG_STR_LIST("cbus4", "{TXDEN,PWRON,RXLED,TXLED,TX_RX_LED,SLEEP,CLK48,CLK24,CLK12,CLK6}", 0),
        CFG_BOOL("invert_txd", cfg_false, 0),
        CFG_BOOL("invert_rxd", cfg_false, 0),
        CFG_BOOL("invert_rts", cfg_false, 0),
        CFG_BOOL("invert_cts", cfg_false, 0),
        CFG_BOOL("invert_dtr", cfg_false, 0),
        CFG_BOOL("invert_dsr", cfg_false, 0),
        CFG_BOOL("invert_dcd", cfg_false, 0),
        CFG_BOOL("invert_ri", cfg_false, 0),
        CFG_END()
    };
    cfg_t *cfg;

    /*
    normal variables
    */
    int _read = 0, _erase = 0, _flash = 0;

    const int max_eeprom_size = 256;
    int my_eeprom_size = 0;
    unsigned char *eeprom_buf = NULL;
    char *filename;
    int size_check;
    int i, argc_filename;
    FILE *fp;

    struct briteblox_context *briteblox = NULL;

    printf("\nBRITEBLOX eeprom generator v%s\n", EEPROM_VERSION_STRING);
    printf ("(c) Intra2net AG and the libbriteblox developers <opensource@intra2net.com>\n");

    if (argc != 2 && argc != 3)
    {
        printf("Syntax: %s [commands] config-file\n", argv[0]);
        printf("Valid commands:\n");
        printf("--read-eeprom  Read eeprom and write to -filename- from config-file\n");
        printf("--erase-eeprom  Erase eeprom\n");
        printf("--flash-eeprom  Flash eeprom\n");
        exit (-1);
    }

    if (argc == 3)
    {
        if (strcmp(argv[1], "--read-eeprom") == 0)
            _read = 1;
        else if (strcmp(argv[1], "--erase-eeprom") == 0)
            _erase = 1;
        else if (strcmp(argv[1], "--flash-eeprom") == 0)
            _flash = 1;
        else
        {
            printf ("Can't open configuration file\n");
            exit (-1);
        }
        argc_filename = 2;
    }
    else
    {
        argc_filename = 1;
    }

    if ((fp = fopen(argv[argc_filename], "r")) == NULL)
    {
        printf ("Can't open configuration file\n");
        exit (-1);
    }
    fclose (fp);

    cfg = cfg_init(opts, 0);
    cfg_parse(cfg, argv[argc_filename]);
    filename = cfg_getstr(cfg, "filename");

    if (cfg_getbool(cfg, "self_powered") && cfg_getint(cfg, "max_power") > 0)
        printf("Hint: Self powered devices should have a max_power setting of 0.\n");

    if ((briteblox = briteblox_new()) == 0)
    {
        fprintf(stderr, "Failed to allocate briteblox structure :%s \n",
                briteblox_get_error_string(briteblox));
        return EXIT_FAILURE;
    }

    if (_read > 0 || _erase > 0 || _flash > 0)
    {
        int vendor_id = cfg_getint(cfg, "vendor_id");
        int product_id = cfg_getint(cfg, "product_id");

        i = briteblox_usb_open(briteblox, vendor_id, product_id);

        if (i != 0)
        {
            int default_pid = cfg_getint(cfg, "default_pid");
            printf("Unable to find BRITEBLOX devices under given vendor/product id: 0x%X/0x%X\n", vendor_id, product_id);
            printf("Error code: %d (%s)\n", i, briteblox_get_error_string(briteblox));
            printf("Retrying with default BRITEBLOX pid=%#04x.\n", default_pid);

            i = briteblox_usb_open(briteblox, 0x0403, default_pid);
            if (i != 0)
            {
                printf("Error: %s\n", briteblox->error_str);
                exit (-1);
            }
        }
    }
    briteblox_eeprom_initdefaults (briteblox, cfg_getstr(cfg, "manufacturer"),
                              cfg_getstr(cfg, "product"),
                              cfg_getstr(cfg, "serial"));

    printf("BRITEBLOX read eeprom: %d\n", briteblox_read_eeprom(briteblox));
    eeprom_get_value(briteblox, CHIP_SIZE, &my_eeprom_size);
    printf("EEPROM size: %d\n", my_eeprom_size);

    if (_read > 0)
    {
        briteblox_eeprom_decode(briteblox, 0 /* debug: 1 */);

        eeprom_buf = malloc(my_eeprom_size);
        briteblox_get_eeprom_buf(briteblox, eeprom_buf, my_eeprom_size);

        if (eeprom_buf == NULL)
        {
            fprintf(stderr, "Malloc failed, aborting\n");
            goto cleanup;
        }
        if (filename != NULL && strlen(filename) > 0)
        {

            FILE *fp = fopen (filename, "wb");
            fwrite (eeprom_buf, 1, my_eeprom_size, fp);
            fclose (fp);
        }
        else
        {
            printf("Warning: Not writing eeprom, you must supply a valid filename\n");
        }

        goto cleanup;
    }

    eeprom_set_value(briteblox, VENDOR_ID, cfg_getint(cfg, "vendor_id"));
    eeprom_set_value(briteblox, PRODUCT_ID, cfg_getint(cfg, "product_id"));

    eeprom_set_value(briteblox, SELF_POWERED, cfg_getbool(cfg, "self_powered"));
    eeprom_set_value(briteblox, REMOTE_WAKEUP, cfg_getbool(cfg, "remote_wakeup"));
    eeprom_set_value(briteblox, MAX_POWER, cfg_getint(cfg, "max_power"));

    eeprom_set_value(briteblox, IN_IS_ISOCHRONOUS, cfg_getbool(cfg, "in_is_isochronous"));
    eeprom_set_value(briteblox, OUT_IS_ISOCHRONOUS, cfg_getbool(cfg, "out_is_isochronous"));
    eeprom_set_value(briteblox, SUSPEND_PULL_DOWNS, cfg_getbool(cfg, "suspend_pull_downs"));

    eeprom_set_value(briteblox, USE_SERIAL, cfg_getbool(cfg, "use_serial"));
    eeprom_set_value(briteblox, USE_USB_VERSION, cfg_getbool(cfg, "change_usb_version"));
    eeprom_set_value(briteblox, USB_VERSION, cfg_getint(cfg, "usb_version"));
    eeprom_set_value(briteblox, CHIP_TYPE, cfg_getint(cfg, "eeprom_type"));

    eeprom_set_value(briteblox, HIGH_CURRENT, cfg_getbool(cfg, "high_current"));
    eeprom_set_value(briteblox, CBUS_FUNCTION_0, str_to_cbus(cfg_getstr(cfg, "cbus0"), 13));
    eeprom_set_value(briteblox, CBUS_FUNCTION_1, str_to_cbus(cfg_getstr(cfg, "cbus1"), 13));
    eeprom_set_value(briteblox, CBUS_FUNCTION_2, str_to_cbus(cfg_getstr(cfg, "cbus2"), 13));
    eeprom_set_value(briteblox, CBUS_FUNCTION_3, str_to_cbus(cfg_getstr(cfg, "cbus3"), 13));
    eeprom_set_value(briteblox, CBUS_FUNCTION_4, str_to_cbus(cfg_getstr(cfg, "cbus4"), 9));
    int invert = 0;
    if (cfg_getbool(cfg, "invert_rxd")) invert |= INVERT_RXD;
    if (cfg_getbool(cfg, "invert_txd")) invert |= INVERT_TXD;
    if (cfg_getbool(cfg, "invert_rts")) invert |= INVERT_RTS;
    if (cfg_getbool(cfg, "invert_cts")) invert |= INVERT_CTS;
    if (cfg_getbool(cfg, "invert_dtr")) invert |= INVERT_DTR;
    if (cfg_getbool(cfg, "invert_dsr")) invert |= INVERT_DSR;
    if (cfg_getbool(cfg, "invert_dcd")) invert |= INVERT_DCD;
    if (cfg_getbool(cfg, "invert_ri")) invert |= INVERT_RI;
    eeprom_set_value(briteblox, INVERT, invert);

    eeprom_set_value(briteblox, CHANNEL_A_DRIVER, DRIVER_VCP);
    eeprom_set_value(briteblox, CHANNEL_B_DRIVER, DRIVER_VCP);
    eeprom_set_value(briteblox, CHANNEL_C_DRIVER, DRIVER_VCP);
    eeprom_set_value(briteblox, CHANNEL_D_DRIVER, DRIVER_VCP);
    eeprom_set_value(briteblox, CHANNEL_A_RS485, 0);
    eeprom_set_value(briteblox, CHANNEL_B_RS485, 0);
    eeprom_set_value(briteblox, CHANNEL_C_RS485, 0);
    eeprom_set_value(briteblox, CHANNEL_D_RS485, 0);

    if (_erase > 0)
    {
        printf("BRITEBLOX erase eeprom: %d\n", briteblox_erase_eeprom(briteblox));
    }

    size_check = briteblox_eeprom_build(briteblox);
    eeprom_get_value(briteblox, CHIP_SIZE, &my_eeprom_size);

    if (size_check == -1)
    {
        printf ("Sorry, the eeprom can only contain 128 bytes (100 bytes for your strings).\n");
        printf ("You need to short your string by: %d bytes\n", size_check);
        goto cleanup;
    }
    else if (size_check < 0)
    {
        printf ("briteblox_eeprom_build(): error: %d\n", size_check);
    }
    else
    {
        printf ("Used eeprom space: %d bytes\n", my_eeprom_size-size_check);
    }

    if (_flash > 0)
    {
        if (cfg_getbool(cfg, "flash_raw"))
        {
            if (filename != NULL && strlen(filename) > 0)
            {
                eeprom_buf = malloc(max_eeprom_size);
                FILE *fp = fopen(filename, "rb");
                if (fp == NULL)
                {
                    printf ("Can't open eeprom file %s.\n", filename);
                    exit (-1);
                }
                my_eeprom_size = fread(eeprom_buf, 1, max_eeprom_size, fp);
                fclose(fp);
                if (my_eeprom_size < 128)
                {
                    printf ("Can't read eeprom file %s.\n", filename);
                    exit (-1);
                }

                briteblox_set_eeprom_buf(briteblox, eeprom_buf, my_eeprom_size);
            }
        }
        printf ("BRITEBLOX write eeprom: %d\n", briteblox_write_eeprom(briteblox));
        libusb_reset_device(briteblox->usb_dev);
    }

    // Write to file?
    if (filename != NULL && strlen(filename) > 0 && !cfg_getbool(cfg, "flash_raw"))
    {
        fp = fopen(filename, "w");
        if (fp == NULL)
        {
            printf ("Can't write eeprom file.\n");
            exit (-1);
        }
        else
            printf ("Writing to file: %s\n", filename);

        if (eeprom_buf == NULL)
            eeprom_buf = malloc(my_eeprom_size);
        briteblox_get_eeprom_buf(briteblox, eeprom_buf, my_eeprom_size);

        fwrite(eeprom_buf, my_eeprom_size, 1, fp);
        fclose(fp);
    }

cleanup:
    if (eeprom_buf)
        free(eeprom_buf);
    if (_read > 0 || _erase > 0 || _flash > 0)
    {
        printf("BRITEBLOX close: %d\n", briteblox_usb_close(briteblox));
    }

    briteblox_deinit (briteblox);
    briteblox_free (briteblox);

    cfg_free(cfg);

    printf("\n");
    return 0;
}
