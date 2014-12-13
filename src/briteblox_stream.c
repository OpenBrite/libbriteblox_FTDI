/***************************************************************************
                          briteblox_stream.c  -  description
                             -------------------
    copyright            : (C) 2009 Micah Dowty 2010 Uwe Bonnes
    email                : opensource@intra2net.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License           *
 *   version 2.1 as published by the Free Software Foundation;             *
 *                                                                         *
 ***************************************************************************/

/* Adapted from
 * fastbriteblox.c - A minimal BRITEBLOX FT232H interface for which supports bit-bang
 *              mode, but focuses on very high-performance support for
 *              synchronous FIFO mode. Requires libusb-1.0
 *
 * Copyright (C) 2009 Micah Dowty
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <libusb.h>

#include "briteblox.h"

typedef struct
{
    BRITEBLOXStreamCallback *callback;
    void *userdata;
    int packetsize;
    int activity;
    int result;
    BRITEBLOXProgressInfo progress;
} BRITEBLOXStreamState;

/* Handle callbacks
 *
 * With Exit request, free memory and release the transfer
 *
 * state->result is only set when some error happens
 */
static void
briteblox_readstream_cb(struct libusb_transfer *transfer)
{
    BRITEBLOXStreamState *state = transfer->user_data;
    int packet_size = state->packetsize;

    state->activity++;
    if (transfer->status == LIBUSB_TRANSFER_COMPLETED)
    {
        int i;
        uint8_t *ptr = transfer->buffer;
        int length = transfer->actual_length;
        int numPackets = (length + packet_size - 1) / packet_size;
        int res = 0;

        for (i = 0; i < numPackets; i++)
        {
            int payloadLen;
            int packetLen = length;

            if (packetLen > packet_size)
                packetLen = packet_size;

            payloadLen = packetLen - 2;
            state->progress.current.totalBytes += payloadLen;

            res = state->callback(ptr + 2, payloadLen,
                                  NULL, state->userdata);

            ptr += packetLen;
            length -= packetLen;
        }
        if (res)
        {
            free(transfer->buffer);
            libusb_free_transfer(transfer);
        }
        else
        {
            transfer->status = -1;
            state->result = libusb_submit_transfer(transfer);
        }
    }
    else
    {
        fprintf(stderr, "unknown status %d\n",transfer->status);
        state->result = LIBUSB_ERROR_IO;
    }
}

/**
   Helper function to calculate (unix) time differences

   \param a timeval
   \param b timeval
*/
static double
TimevalDiff(const struct timeval *a, const struct timeval *b)
{
    return (a->tv_sec - b->tv_sec) + 1e-6 * (a->tv_usec - b->tv_usec);
}

/**
    Streaming reading of data from the device

    Use asynchronous transfers in libusb-1.0 for high-performance
    streaming of data from a device interface back to the PC. This
    function continuously transfers data until either an error occurs
    or the callback returns a nonzero value. This function returns
    a libusb error code or the callback's return value.

    For every contiguous block of received data, the callback will
    be invoked.

    \param  briteblox pointer to briteblox_context
    \param  callback to user supplied function for one block of data
    \param  userdata
    \param  packetsPerTransfer number of packets per transfer
    \param  numTransfers Number of transfers per callback

*/

int
briteblox_readstream(struct briteblox_context *briteblox,
                BRITEBLOXStreamCallback *callback, void *userdata,
                int packetsPerTransfer, int numTransfers)
{
    struct libusb_transfer **transfers;
    BRITEBLOXStreamState state = { callback, userdata, briteblox->max_packet_size, 1 };
    int bufferSize = packetsPerTransfer * briteblox->max_packet_size;
    int xferIndex;
    int err = 0;

    /* Only FT2232H and FT232H know about the synchronous FIFO Mode*/
    if ((briteblox->type != TYPE_2232H) && (briteblox->type != TYPE_232H))
    {
        fprintf(stderr,"Device doesn't support synchronous FIFO mode\n");
        return 1;
    }

    /* We don't know in what state we are, switch to reset*/
    if (briteblox_set_bitmode(briteblox,  0xff, BITMODE_RESET) < 0)
    {
        fprintf(stderr,"Can't reset mode\n");
        return 1;
    }

    /* Purge anything remaining in the buffers*/
    if (briteblox_usb_purge_buffers(briteblox) < 0)
    {
        fprintf(stderr,"Can't Purge\n");
        return 1;
    }

    /*
     * Set up all transfers
     */

    transfers = calloc(numTransfers, sizeof *transfers);
    if (!transfers)
    {
        err = LIBUSB_ERROR_NO_MEM;
        goto cleanup;
    }

    for (xferIndex = 0; xferIndex < numTransfers; xferIndex++)
    {
        struct libusb_transfer *transfer;

        transfer = libusb_alloc_transfer(0);
        transfers[xferIndex] = transfer;
        if (!transfer)
        {
            err = LIBUSB_ERROR_NO_MEM;
            goto cleanup;
        }

        libusb_fill_bulk_transfer(transfer, briteblox->usb_dev, briteblox->out_ep,
                                  malloc(bufferSize), bufferSize,
                                  briteblox_readstream_cb,
                                  &state, 0);

        if (!transfer->buffer)
        {
            err = LIBUSB_ERROR_NO_MEM;
            goto cleanup;
        }

        transfer->status = -1;
        err = libusb_submit_transfer(transfer);
        if (err)
            goto cleanup;
    }

    /* Start the transfers only when everything has been set up.
     * Otherwise the transfers start stuttering and the PC not
     * fetching data for several to several ten milliseconds
     * and we skip blocks
     */
    if (briteblox_set_bitmode(briteblox,  0xff, BITMODE_SYNCFF) < 0)
    {
        fprintf(stderr,"Can't set synchronous fifo mode: %s\n",
                briteblox_get_error_string(briteblox));
        goto cleanup;
    }

    /*
     * Run the transfers, and periodically assess progress.
     */

    gettimeofday(&state.progress.first.time, NULL);

    do
    {
        BRITEBLOXProgressInfo  *progress = &state.progress;
        const double progressInterval = 1.0;
        struct timeval timeout = { 0, briteblox->usb_read_timeout };
        struct timeval now;

        int err = libusb_handle_events_timeout(briteblox->usb_ctx, &timeout);
        if (err ==  LIBUSB_ERROR_INTERRUPTED)
            /* restart interrupted events */
            err = libusb_handle_events_timeout(briteblox->usb_ctx, &timeout);
        if (!state.result)
        {
            state.result = err;
        }
        if (state.activity == 0)
            state.result = 1;
        else
            state.activity = 0;

        // If enough time has elapsed, update the progress
        gettimeofday(&now, NULL);
        if (TimevalDiff(&now, &progress->current.time) >= progressInterval)
        {
            progress->current.time = now;
            progress->totalTime = TimevalDiff(&progress->current.time,
                                              &progress->first.time);

            if (progress->prev.totalBytes)
            {
                // We have enough information to calculate rates

                double currentTime;

                currentTime = TimevalDiff(&progress->current.time,
                                          &progress->prev.time);

                progress->totalRate =
                    progress->current.totalBytes /progress->totalTime;
                progress->currentRate =
                    (progress->current.totalBytes -
                     progress->prev.totalBytes) / currentTime;
            }

            state.callback(NULL, 0, progress, state.userdata);
            progress->prev = progress->current;

        }
    } while (!state.result);

    /*
     * Cancel any outstanding transfers, and free memory.
     */

cleanup:
    fprintf(stderr, "cleanup\n");
    if (transfers)
        free(transfers);
    if (err)
        return err;
    else
        return state.result;
}

