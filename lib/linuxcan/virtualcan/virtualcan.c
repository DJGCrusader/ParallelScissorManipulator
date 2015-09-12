/*
**                Copyright 2012 by Kvaser AB, M�lndal, Sweden
**                        http://www.kvaser.com
**
** This software is dual licensed under the following two licenses:
** BSD-new and GPLv2. You may use either one. See the included
** COPYING file for details.
**
** License: BSD-new
** ===============================================================================
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of the <organization> nor the
**       names of its contributors may be used to endorse or promote products
**       derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
** DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
** LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
** ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**
** License: GPLv2
** ===============================================================================
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
** ---------------------------------------------------------------------------
**/

//--------------------------------------------------
// NOTE! module_versioning HAVE to be included first
#include "module_versioning.h"
//--------------------------------------------------

//
// Kvaser CAN driver virtual hardware specific parts
// virtual functions
//

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
#   include <asm/system.h>
#endif
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <linux/pci.h>


// Kvaser definitions
#include "VCanOsIf.h"
#include "virtualcan.h"
#include "osif_kernel.h"
#include "osif_functions_kernel.h"
#include "queue.h"
#include "hwnames.h"
#include "vcan_ioctl.h"


MODULE_DESCRIPTION("VirtualCAN CAN module.");

//
// If you do not define VIRTUAL_DEBUG at all, all the debug code will be
// left out.  If you compile with VIRTUAL_DEBUG=0, the debug code will
// be present but disabled -- but it can then be enabled for specific
// modules at load time with a 'debug_level=#' option to insmod.
// i.e. >insmod kvvirtual debug_level=#
//

#ifdef VIRTUAL_DEBUG
static int debug_level = VIRTUAL_DEBUG;
    MODULE_PARM_DESC(debug_level, "virtual debug level");
    module_param(debug_level, int, 0644);
#   define DEBUGPRINT(n, args...) if (debug_level>=(n)) printk("<" #n ">" args)
#else
#   define DEBUGPRINT(n, args...) if ((n) == 1) printk("<" #n ">" args)
#endif

//======================================================================
// HW function pointers
//======================================================================

static int INIT virtualInitAllDevices(void);
static int virtualSetBusParams (VCanChanData *vChd, VCanBusParams *par);
static int virtualGetBusParams (VCanChanData *vChd, VCanBusParams *par);
static int virtualSetOutputMode (VCanChanData *vChd, int silent);
static int virtualSetTranceiverMode (VCanChanData *vChd, int linemode, int resnet);
static int virtualBusOn (VCanChanData *vChd);
static int virtualBusOff (VCanChanData *vChd);
static int virtualGetTxErr(VCanChanData *vChd);
static int virtualGetRxErr(VCanChanData *vChd);
static int virtualTxAvailable (VCanChanData *vChd);
static int EXIT virtualCloseAllDevices(void);
static int virtualProcRead (struct seq_file* m, void* v);
static int virtualRequestChipState (VCanChanData *vChd);
static unsigned long virtualRxQLen(VCanChanData *vChd);
static unsigned long virtualTxQLen(VCanChanData *vChd); 
static void virtualRequestSend (VCanCardData *vCard, VCanChanData *vChan);

static int virtualTransmitMessage (VCanChanData *vChd, CAN_MSG *m);


VCanHWInterface hwIf = {
    .initAllDevices     = virtualInitAllDevices,
    .setBusParams       = virtualSetBusParams,
    .getBusParams       = virtualGetBusParams,
    .setOutputMode      = virtualSetOutputMode,
    .setTranceiverMode  = virtualSetTranceiverMode,
    .busOn              = virtualBusOn,
    .busOff             = virtualBusOff,
    .txAvailable        = virtualTxAvailable,
    .procRead           = virtualProcRead,
    .closeAllDevices    = virtualCloseAllDevices,
    .getTime            = vCanTime,
    .flushSendBuffer    = vCanFlushSendBuffer,
    .getTxErr           = virtualGetTxErr,
    .getRxErr           = virtualGetRxErr,
    .rxQLen             = virtualRxQLen,
    .txQLen             = virtualTxQLen,
    .requestChipState   = virtualRequestChipState,
    .requestSend        = virtualRequestSend
};


// prototype:
static int virtualSend (void *void_chanData);



//======================================================================
// Wrapper for common function
//======================================================================
static unsigned long getTime(VCanCardData *vCard)
{
  unsigned long time;

  hwIf.getTime(vCard, &time);

  return time;
}


//======================================================================
// /proc read function
//======================================================================
static int virtualProcRead (struct seq_file* m, void* v)
{
    seq_printf(m, "\ntotal channels %d\n",
                   NR_CHANNELS * NR_VIRTUAL_DEV);

    return 0;
}

//======================================================================
//  Can we send now?
//======================================================================
static int virtualTxAvailable (VCanChanData *vChd)
{
    virtualChanData *hChd = vChd->hwChanData;

    return (atomic_read(&hChd->outstanding_tx) < VIRTUAL_MAX_OUTSTANDING);
} // virtualTxAvailable


//======================================================================
// Find out some info about the H/W
//======================================================================
static int virtualProbe (VCanCardData *vCd)
{
    int i;

    vCd->nrChannels = NR_CHANNELS;
    DEBUGPRINT(1, "Kvaser virtual with %d channels found\n", vCd->nrChannels);

    for (i = 0; i < vCd->nrChannels; ++i) {
        VCanChanData *vChd = vCd->chanData[i];
        vChd->channel      = i;
        vChd->transType    = VCAN_TRANSCEIVER_TYPE_251;
        memset(vChd->ean, 0, sizeof(vChd->ean));
        vChd->serialLow    = 0;
        vChd->serialHigh   = 0;

        vChd->lineMode     = VCAN_TRANSCEIVER_LINEMODE_NORMAL;
    }
    vCd->cardPresent = 1;

    // qqq This should be per channel!
    vCd->capabilities = VCAN_CHANNEL_CAP_EXTENDED_CAN         |
                        VCAN_CHANNEL_CAP_VIRTUAL              |
                        VCAN_CHANNEL_CAP_TXREQUEST            |
                        VCAN_CHANNEL_CAP_TXACKNOWLEDGE;

    vCd->hw_type      = HWTYPE_VIRTUAL;

    return 0;
} // virtualProbe


//======================================================================
// Enable bus error interrupts, and reset the
// counters which keep track of the error rate
//======================================================================
static void virtualResetErrorCounter (VCanChanData *vChd)
{
    vChd->errorCount = 0;

    vChd->errorTime = getTime(vChd->vCard);
} // virtualResetErrorCounter


//======================================================================
//  Set bit timing
//======================================================================
static int virtualSetBusParams (VCanChanData *vChd, VCanBusParams *par)
{
    virtualChanData *virtualChan = (virtualChanData *)vChd->hwChanData;
    // Save busparams
    virtualChan->busparams.freq  = par->freq;
    virtualChan->busparams.sjw   = par->sjw;
    virtualChan->busparams.tseg1 = par->tseg1;
    virtualChan->busparams.tseg2 = par->tseg2;
    virtualChan->busparams.samp3 = par->samp3;

    return 0;
} // virtualSetBusParams


//======================================================================
//  Get bit timing
//======================================================================
static int virtualGetBusParams (VCanChanData *vChd, VCanBusParams *par)
{
    virtualChanData *virtualChan = (virtualChanData *)vChd->hwChanData;
    // Read saved busparams
    par->freq  = virtualChan->busparams.freq;
    par->sjw   = virtualChan->busparams.sjw;
    par->tseg1 = virtualChan->busparams.tseg1;
    par->tseg2 = virtualChan->busparams.tseg2;
    par->samp3 = virtualChan->busparams.samp3;

    return 0;
} // virtualGetBusParams


//======================================================================
//  Set silent or normal mode
//======================================================================
static int virtualSetOutputMode (VCanChanData *vChd, int silent)
{
    virtualChanData *virtualChan = vChd->hwChanData;
    VCanCardData    *vCard       = vChd->vCard;
    int             i;

    if (virtualChan->silentmode && (silent == 1)) {
        virtualChan->silentmode = silent;

        // Try sending all unsent messages on all channels,
        // except the specified channel
        for (i = 0; i < vCard->nrChannels; i++) {
            // See if other chans have stuff to send,
            // now that we are not alone anymore...
             if ((vCard->chanData[i] != NULL) && (vCard->chanData[i] != vChd) &&
                 (vCard->chanData[i]->isOnBus)) {
                //DEBUGPRINT(1, "virtualsend from bus on. %d\n", vChd->channel);
                virtualSend((void *)vCard->chanData[i]);
            }
        }
    }
    else {
      virtualChan->silentmode = silent;
    }

    return 0;
} // virtualSetOutputMode


//======================================================================
//  Line mode
//======================================================================
static int virtualSetTranceiverMode (VCanChanData *vChd, int linemode, int resnet)
{
    // qqq what to do here?
    return 0;
} // virtualSetTranceiverMode


//======================================================================
//  Query chip status
//======================================================================
static int virtualRequestChipState (VCanChanData *vChd)
{
    // qqq what here?
    return 0;
} // virtualRequestChipState


//======================================================================
//  Go bus on
//======================================================================
static int virtualBusOn (VCanChanData *vChd)
{
    virtualChanData *virtualChan = vChd->hwChanData;
    VCanCardData    *vCard       = vChd->vCard;
    int i;

    vChd->isOnBus = 1;
    vChd->overrun = 0;
    atomic_set(&virtualChan->outstanding_tx, 0);
    virtualResetErrorCounter(vChd);
    vChd->chipState.state = CHIPSTAT_ERROR_ACTIVE;

    //  Try sending all unsent messages on all channels except the specified channel
    for (i = 0; i < vCard->nrChannels; i++) {
        //se if other chans have stuff to send now that we are on bus...
         if ((vCard->chanData[i] != NULL) && (vCard->chanData[i] != vChd) &&
             (vCard->chanData[i]->isOnBus)) {
            DEBUGPRINT(2, "virtualsend from bus on. %d\n", vChd->channel);
            virtualSend((void *)vCard->chanData[i]);
        }
    }

    return 0;
} // virtualBusOn


//======================================================================
//  Go bus off
//======================================================================
static int virtualBusOff (VCanChanData *vChd)
{
    vChd->isOnBus = 0;

    virtualRequestChipState(vChd);

    return 0;
} // virtualBusOff


//======================================================================
//  Read transmit error counter
//======================================================================
static int virtualGetTxErr (VCanChanData *vChd)
{
    return 0; // qqq what do we do....
}


//======================================================================
//  Read transmit error counter
//======================================================================
static int virtualGetRxErr (VCanChanData *vChd)
{
    return 0; // qqq what do we do....
}


//======================================================================
//  Read receive queue length in hardware/firmware
//======================================================================
static unsigned long virtualRxQLen (VCanChanData *vChd)
{
    // qqq Why _tx_ channel buffer?
    return queue_length(&vChd->txChanQueue);
}


//======================================================================
//  Read transmit queue length in hardware/firmware
//======================================================================
static unsigned long virtualTxQLen (VCanChanData *vChd)
{
    int qLen = 0;

    // Return zero because we don't have any hw-buffers.
    return qLen;
}


//======================================================================
// Request send
//======================================================================
static void virtualRequestSend (VCanCardData *vCard, VCanChanData *vChan)
{
    if (vChan->isOnBus) {
        if (!virtualTxAvailable(vChan)) {
            return;
        }
        virtualSend(vChan);
    }
}


//======================================================================
// send
//======================================================================
static int virtualSend (void *void_chanData)
{
    VCanChanData *chd = (VCanChanData *)void_chanData;

    // Send Messages
    while (1) {
        if (!queue_empty(&chd->txChanQueue)) {
            int queuePos = queue_front(&chd->txChanQueue);
            if (queuePos < 0) {   // Did we actually get anything from queue?
              queue_release(&chd->txChanQueue);
              continue;
            }
            if (virtualTransmitMessage(chd, &(chd->txChanBuffer[queuePos]))) {
                //DEBUGPRINT(1, "NS:%d \n", chd->channel);
                queue_release(&chd->txChanQueue);
                return -2;
            }
            queue_pop(&chd->txChanQueue);
        }
        else if (test_and_clear_bit(0, &chd->waitEmpty)) {
            os_if_wake_up_interruptible(&chd->flushQ);
            break;
        }
        else {
            queue_wakeup_on_space(&chd->txChanQueue);
            //DEBUGPRINT(1, "Nothing for ch%d \n", chd->channel);
            break;
        }
    }

    return 0;
}


//======================================================================
//  virtualTransmit
//======================================================================
static int virtualTransmitMessage (VCanChanData *vChd, CAN_MSG *m)
{
    int               i;
    VCanCardData      *vCard        = vChd->vCard;
    int               isDispatched  = 0;
    virtualChanData   *virtualChan  = NULL;

    // qqq is it ok to just pass the ptr on?
    VCAN_EVENT *e = (VCAN_EVENT *)m;

    // Fake reception
    e->tag       = V_RECEIVE_MSG;
    e->timeStamp = getTime(vCard);
    for (i = 0; i < vCard->nrChannels; i++) {
        // Distribute the msgs to all valid channels on a specific card 
        // (not ourselves though).
        if ((vCard->chanData[i] != NULL) && (vCard->chanData[i] != vChd) &&
            (vCard->chanData[i]->isOnBus)) {
            vCanDispatchEvent(vCard->chanData[i], e);
            virtualChan = vCard->chanData[i]->hwChanData;
            if (!virtualChan->silentmode) {
                isDispatched++;
            }
        }
    }

    virtualChan = vChd->hwChanData;     // Point virtualChan to sending chan
    if (!isDispatched) {
        atomic_add(1, &virtualChan->outstanding_tx);
    } else {
        atomic_add_unless(&virtualChan->outstanding_tx, -1, 0);
    }

    return isDispatched ? 0 : -1;
} // virtualTransmitMessage


//======================================================================
//  Initialize H/W specific data
//======================================================================
static int virtualInitData (VCanCardData *vCard)
{
    int chNr;
    vCanInitData(vCard);
    for (chNr = 0; chNr < vCard->nrChannels; chNr++) {
        virtualChanData *hChd;
        hChd = vCard->chanData[chNr]->hwChanData;
        hChd->silentmode = 0;
    }

    return 0;
}


//======================================================================
// Initialize the HW for one card
//======================================================================
static int virtualInitOne (void)
{
    // Helper struct for allocation
    typedef struct {
        VCanChanData    *dataPtrArray[MAX_CHANNELS];
        VCanChanData    vChd[MAX_CHANNELS];
        virtualChanData hChd[MAX_CHANNELS];
    } ChanHelperStruct;

    ChanHelperStruct   *chs;
    int                chNr;
    VCanCardData       *vCard;

    // Allocate data area for this card
    vCard = os_if_kernel_malloc(sizeof(VCanCardData) + sizeof(virtualCardData));
    if (!vCard) {
        goto card_alloc_err;
    }
    memset(vCard, 0, sizeof(VCanCardData) + sizeof(virtualCardData));

    // hwCardData is directly after VCanCardData
    vCard->hwCardData = vCard + 1;

    // Allocate memory for n channels
    chs = os_if_kernel_malloc(sizeof(ChanHelperStruct));
    if (!chs) {
        goto chan_alloc_err;
    }
    memset(chs, 0, sizeof(ChanHelperStruct));

    // Init array and hwChanData
    for (chNr = 0; chNr < MAX_CHANNELS; chNr++) {
        chs->dataPtrArray[chNr]    = &chs->vChd[chNr];
        chs->vChd[chNr].hwChanData = &chs->hChd[chNr];
        chs->vChd[chNr].minorNr    = -1;   // No preset minor number
    }
    vCard->chanData = chs->dataPtrArray;

    // Find out type of card i.e. N/O channels etc
    if (virtualProbe(vCard)) {
        DEBUGPRINT(1, "virtualProbe failed");
        goto probe_err;
    }

    // Init channels
    virtualInitData(vCard);

    // Insert into list of cards
    os_if_spin_lock(&canCardsLock);
    vCard->next = canCards;
    canCards    = vCard;
    os_if_spin_unlock(&canCardsLock);

    return 1;

chan_alloc_err:
probe_err:
    os_if_kernel_free(vCard);
card_alloc_err:

    return 0;
} // virtualInitOne


static void virtualRemoveOne (VCanCardData *vCard)
{
  VCanChanData *vChan;
  int chNr;

  // qqq Mark as not working somehow!

  for (chNr = 0; chNr < vCard->nrChannels; chNr++) {
    vChan = vCard->chanData[chNr];
    DEBUGPRINT(3, "Waiting for all closed on minor %d\n", vChan->minorNr);
    while (atomic_read(&vChan->fileOpenCount) > 0) {
      os_if_set_task_uninterruptible ();
      os_if_wait_for_event_timeout_simple(10);
    }
  }

  os_if_kernel_free(vCard->chanData);
  os_if_kernel_free(vCard);
}


//======================================================================
// Find and initialize all cards
//======================================================================
static int virtualInitAllDevices (void)
{
    int i;

    driverData.deviceName = DEVICE_NAME_STRING;
    for (i = 0; i < NR_VIRTUAL_DEV; i++) {
        virtualInitOne();
    }

    DEBUGPRINT(1, "Kvaser Virtual Initialized.\n");

    return 0;
} // virtualInitAllDevices


//======================================================================
// Shut down and free resources before unloading driver
//======================================================================
static int virtualCloseAllDevices (void)
{
    VCanCardData *vCard;

    while (1) {
        os_if_spin_lock(&canCardsLock);
        vCard = canCards;
        if (!vCard) {
            os_if_spin_unlock(&canCardsLock);
            break;
        }
        canCards = vCard->next;
        os_if_spin_unlock(&canCardsLock);
        virtualRemoveOne(vCard);
    }
    DEBUGPRINT(1, "Kvaser Virtual Closed.\n");

    return 0;
} // virtualCloseAllDevices
