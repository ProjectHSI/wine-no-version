/*
 * MIDI driver for OSS (PE-side)
 *
 * Copyright 1994       Martin Ayotte
 * Copyright 1998       Luiz Otavio L. Zorzella (init procedures)
 * Copyright 1998, 1999 Eric POUECH
 * Copyright 2022       Huw Davies
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/* TODO:
 *    + use better instrument definition for OPL/2 (midiPatch.c) or
 *      use existing instrument definition (from playmidi or kmid)
 *      with a .winerc option 
 *    + have a look at OPL/3 ?
 *    + implement asynchronous playback of MidiHdr
 *    + implement STREAM'ed MidiHdr (question: how shall we share the
 *      code between the midiStream functions in MMSYSTEM/WINMM and
 *      the code for the low level driver)
 *    + use a more accurate read mechanism than the one of snooping on
 *      timers (like select on fd)
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <sys/soundcard.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winternl.h"
#include "mmddk.h"
#include "audioclient.h"

#include "wine/debug.h"
#include "wine/unixlib.h"

#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(midi);

static WINE_MIDIIN *MidiInDev;

/* this is the total number of MIDI out devices found */
static	int 		MIDM_NumDevs = 0;

static	int		numStartedMidiIn = 0;

static int end_thread;
static HANDLE hThread;

/*======================================================================*
 *                  Low level MIDI implementation			*
 *======================================================================*/

static int midiOpenSeq(void);
static int midiCloseSeq(int);

static int MIDI_loadcount;
/**************************************************************************
 * 			OSS_MidiInit				[internal]
 *
 * Initializes the MIDI devices information variables
 */
static LRESULT OSS_MidiInit(void)
{
    struct midi_init_params params;
    UINT err;

    TRACE("(%i)\n", MIDI_loadcount);
    if (MIDI_loadcount++)
        return 1;

    TRACE("Initializing the MIDI variables.\n");

    params.err = &err;
    OSS_CALL(midi_init, &params);

    if (!err)
    {
        MidiInDev = params.srcs;
        MIDM_NumDevs = params.num_srcs;
    }
    return err;
}

/**************************************************************************
 * 			OSS_MidiExit				[internal]
 *
 * Release the MIDI devices information variables
 */
static LRESULT OSS_MidiExit(void)
{
    TRACE("(%i)\n", MIDI_loadcount);

    if (--MIDI_loadcount)
        return 1;

    MidiInDev = NULL;
    MIDM_NumDevs = 0;

    return 0;
}

static void in_buffer_lock(void)
{
    OSS_CALL(midi_in_lock, ULongToPtr(1));
}

static void in_buffer_unlock(void)
{
    OSS_CALL(midi_in_lock, ULongToPtr(0));
}

static void notify_client(struct notify_context *notify)
{
    TRACE("dev_id = %d msg = %d param1 = %04lX param2 = %04lX\n",
          notify->dev_id, notify->msg, notify->param_1, notify->param_2);

    DriverCallback(notify->callback, notify->flags, notify->device, notify->msg,
                   notify->instance, notify->param_1, notify->param_2);
}

/**************************************************************************
 * 			MIDI_NotifyClient			[internal]
 */
static void MIDI_NotifyClient(UINT wDevID, WORD wMsg,
			      DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    DWORD_PTR 		dwCallBack;
    UINT 		uFlags;
    HANDLE		hDev;
    DWORD_PTR 		dwInstance;

    TRACE("wDevID = %04X wMsg = %d dwParm1 = %04lX dwParam2 = %04lX\n",
	  wDevID, wMsg, dwParam1, dwParam2);

    switch (wMsg) {
    case MIM_OPEN:
    case MIM_CLOSE:
    case MIM_DATA:
    case MIM_LONGDATA:
    case MIM_ERROR:
    case MIM_LONGERROR:
    case MIM_MOREDATA:
	if (wDevID > MIDM_NumDevs) return;

	dwCallBack = MidiInDev[wDevID].midiDesc.dwCallback;
	uFlags = MidiInDev[wDevID].wFlags;
	hDev = MidiInDev[wDevID].midiDesc.hMidi;
	dwInstance = MidiInDev[wDevID].midiDesc.dwInstance;
	break;
    default:
	ERR("Unsupported MSW-MIDI message %u\n", wMsg);
	return;
    }

    DriverCallback(dwCallBack, uFlags, hDev, wMsg, dwInstance, dwParam1, dwParam2);
}

/**************************************************************************
 * 			midiOpenSeq				[internal]
 */
static int midiOpenSeq(void)
{
    struct midi_seq_open_params params;

    params.close = 0;
    params.fd = -1;
    OSS_CALL(midi_seq_open, &params);

    return params.fd;
}

/**************************************************************************
 * 			midiCloseSeq				[internal]
 */
static int midiCloseSeq(int fd)
{
    struct midi_seq_open_params params;

    params.close = 1;
    params.fd = fd;
    OSS_CALL(midi_seq_open, &params);

    return 0;
}

/**************************************************************************
 * 			midReceiveChar				[internal]
 */
static void midReceiveChar(WORD wDevID, unsigned char value, DWORD dwTime)
{
    DWORD		toSend = 0;

    TRACE("Adding %02xh to %d[%d]\n", value, wDevID, MidiInDev[wDevID].incLen);

    if (wDevID >= MIDM_NumDevs) {
	WARN("bad devID\n");
	return;
    }
    if (MidiInDev[wDevID].state <= 0) {
	TRACE("disabled or input not started, thrown away\n");
	return;
    }

    if (MidiInDev[wDevID].state & 2) { /* system exclusive */
	LPMIDIHDR	lpMidiHdr;
        BOOL            sbfb = FALSE;

	in_buffer_lock();
	if ((lpMidiHdr = MidiInDev[wDevID].lpQueueHdr) != NULL) {
	    LPBYTE	lpData = (LPBYTE) lpMidiHdr->lpData;

	    lpData[lpMidiHdr->dwBytesRecorded++] = value;
	    if (lpMidiHdr->dwBytesRecorded == lpMidiHdr->dwBufferLength) {
		sbfb = TRUE;
	    }
	}
	if (value == 0xF7) { /* then end */
	    MidiInDev[wDevID].state &= ~2;
	    sbfb = TRUE;
	}
	if (sbfb && lpMidiHdr != NULL) {
	    lpMidiHdr = MidiInDev[wDevID].lpQueueHdr;
	    MidiInDev[wDevID].lpQueueHdr = lpMidiHdr->lpNext;
	    lpMidiHdr->dwFlags &= ~MHDR_INQUEUE;
	    lpMidiHdr->dwFlags |= MHDR_DONE;
	    MIDI_NotifyClient(wDevID, MIM_LONGDATA, (DWORD_PTR)lpMidiHdr, dwTime);
	}
	in_buffer_unlock();
	return;
    }

#define IS_CMD(_x)	(((_x) & 0x80) == 0x80)
#define IS_SYS_CMD(_x)	(((_x) & 0xF0) == 0xF0)

    if (!IS_CMD(value) && MidiInDev[wDevID].incLen == 0) { /* try to reuse old cmd */
	if (IS_CMD(MidiInDev[wDevID].incPrev) && !IS_SYS_CMD(MidiInDev[wDevID].incPrev)) {
	    MidiInDev[wDevID].incoming[0] = MidiInDev[wDevID].incPrev;
	    MidiInDev[wDevID].incLen = 1;
	    TRACE("Reusing old command %02xh\n", MidiInDev[wDevID].incPrev);
	} else {
	    FIXME("error for midi-in, should generate MIM_ERROR notification:"
		  " prev=%02Xh, incLen=%02Xh\n",
		  MidiInDev[wDevID].incPrev, MidiInDev[wDevID].incLen);
	    return;
	}
    }
    MidiInDev[wDevID].incoming[(int)(MidiInDev[wDevID].incLen++)] = value;
    if (MidiInDev[wDevID].incLen == 1 && !IS_SYS_CMD(MidiInDev[wDevID].incoming[0])) {
	/* store new cmd, just in case */
	MidiInDev[wDevID].incPrev = MidiInDev[wDevID].incoming[0];
    }

#undef IS_CMD
#undef IS_SYS_CMD

    switch (MidiInDev[wDevID].incoming[0] & 0xF0) {
    case MIDI_NOTEOFF:
    case MIDI_NOTEON:
    case MIDI_KEY_PRESSURE:
    case MIDI_CTL_CHANGE:
    case MIDI_PITCH_BEND:
	if (MidiInDev[wDevID].incLen == 3) {
	    toSend = (MidiInDev[wDevID].incoming[2] << 16) |
		(MidiInDev[wDevID].incoming[1] <<  8) |
		(MidiInDev[wDevID].incoming[0] <<  0);
	}
	break;
    case MIDI_PGM_CHANGE:
    case MIDI_CHN_PRESSURE:
	if (MidiInDev[wDevID].incLen == 2) {
	    toSend = (MidiInDev[wDevID].incoming[1] <<  8) |
		(MidiInDev[wDevID].incoming[0] <<  0);
	}
	break;
    case MIDI_SYSTEM_PREFIX:
	if (MidiInDev[wDevID].incoming[0] == 0xF0) {
	    MidiInDev[wDevID].state |= 2;
	    MidiInDev[wDevID].incLen = 0;
	} else {
	    if (MidiInDev[wDevID].incLen == 1) {
		toSend = (MidiInDev[wDevID].incoming[0] <<  0);
	    }
	}
	break;
    default:
	WARN("This shouldn't happen (%02X)\n", MidiInDev[wDevID].incoming[0]);
    }
    if (toSend != 0) {
	TRACE("Sending event %08x\n", toSend);
	MidiInDev[wDevID].incLen =	0;
	dwTime -= MidiInDev[wDevID].startTime;
	MIDI_NotifyClient(wDevID, MIM_DATA, toSend, dwTime);
    }
}

static DWORD WINAPI midRecThread(void *arg)
{
    int fd = (int)(INT_PTR)arg;
    unsigned char buffer[256];
    int len, idx;
    DWORD dwTime;
    struct pollfd pfd;

    TRACE("Thread startup\n");

    pfd.fd = fd;
    pfd.events = POLLIN;
    
    while(!end_thread) {
	TRACE("Thread loop\n");

	/* Check if an event is present */
	if (poll(&pfd, 1, 250) <= 0)
	    continue;
	
	len = read(fd, buffer, sizeof(buffer));
	TRACE("Received %d bytes\n", len);

	if (len < 0) continue;
	if ((len % 4) != 0) {
	    WARN("Bad length %d, errno %d (%s)\n", len, errno, strerror(errno));
	    continue;
	}

	dwTime = GetTickCount();
	
	for (idx = 0; idx < len; ) {
	    if (buffer[idx] & 0x80) {
		TRACE(
		      "Reading<8> %02x %02x %02x %02x %02x %02x %02x %02x\n",
		      buffer[idx + 0], buffer[idx + 1],
		      buffer[idx + 2], buffer[idx + 3],
		      buffer[idx + 4], buffer[idx + 5],
		      buffer[idx + 6], buffer[idx + 7]);
		      idx += 8;
	    } else {
		switch (buffer[idx + 0]) {
		case SEQ_WAIT:
		case SEQ_ECHO:
		    break;
		case SEQ_MIDIPUTC:
		    midReceiveChar(buffer[idx + 2], buffer[idx + 1], dwTime);
		    break;
		default:
		    TRACE("Unsupported event %d\n", buffer[idx + 0]);
		    break;
		}
		idx += 4;
	    }
	}
    }
    return 0;
}

/**************************************************************************
 * 			midOpen					[internal]
 */
static DWORD midOpen(WORD wDevID, LPMIDIOPENDESC lpDesc, DWORD dwFlags)
{
    int fd;

    TRACE("(%04X, %p, %08X);\n", wDevID, lpDesc, dwFlags);

    if (lpDesc == NULL) {
	WARN("Invalid Parameter !\n");
	return MMSYSERR_INVALPARAM;
    }

    /* FIXME :
     *	how to check that content of lpDesc is correct ?
     */
    if (wDevID >= MIDM_NumDevs) {
	WARN("wDevID too large (%u) !\n", wDevID);
	return MMSYSERR_BADDEVICEID;
    }
    if (MidiInDev[wDevID].state == -1) {        
        WARN("device disabled\n");
        return MIDIERR_NODEVICE;
    }
    if (MidiInDev[wDevID].midiDesc.hMidi != 0) {
	WARN("device already open !\n");
	return MMSYSERR_ALLOCATED;
    }
    if ((dwFlags & MIDI_IO_STATUS) != 0) {
	WARN("No support for MIDI_IO_STATUS in dwFlags yet, ignoring it\n");
	dwFlags &= ~MIDI_IO_STATUS;
    }
    if ((dwFlags & ~CALLBACK_TYPEMASK) != 0) {
	FIXME("Bad dwFlags\n");
	return MMSYSERR_INVALFLAG;
    }

    fd = midiOpenSeq();
    if (fd < 0) {
	return MMSYSERR_ERROR;
    }

    if (numStartedMidiIn++ == 0) {
	end_thread = 0;
	hThread = CreateThread(NULL, 0, midRecThread, (void *)(INT_PTR)fd, 0, NULL);
	if (!hThread) {
	    numStartedMidiIn = 0;
	    WARN("Couldn't create thread for midi-in\n");
	    midiCloseSeq(fd);
	    return MMSYSERR_ERROR;
	}
        SetThreadPriority(hThread, THREAD_PRIORITY_TIME_CRITICAL);
	TRACE("Created thread for midi-in\n");
    }

    MidiInDev[wDevID].wFlags = HIWORD(dwFlags & CALLBACK_TYPEMASK);

    MidiInDev[wDevID].lpQueueHdr = NULL;
    MidiInDev[wDevID].midiDesc = *lpDesc;
    MidiInDev[wDevID].state = 0;
    MidiInDev[wDevID].incLen = 0;
    MidiInDev[wDevID].startTime = 0;
    MidiInDev[wDevID].fd = fd;

    MIDI_NotifyClient(wDevID, MIM_OPEN, 0L, 0L);
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 			midClose				[internal]
 */
static DWORD midClose(WORD wDevID)
{
    int		ret = MMSYSERR_NOERROR;

    TRACE("(%04X);\n", wDevID);

    if (wDevID >= MIDM_NumDevs) {
	WARN("wDevID too big (%u) !\n", wDevID);
	return MMSYSERR_BADDEVICEID;
    }
    if (MidiInDev[wDevID].midiDesc.hMidi == 0) {
	WARN("device not opened !\n");
	return MMSYSERR_ERROR;
    }
    if (MidiInDev[wDevID].lpQueueHdr != 0) {
	return MIDIERR_STILLPLAYING;
    }

    if (MidiInDev[wDevID].fd == -1) {
	WARN("ooops !\n");
	return MMSYSERR_ERROR;
    }
    if (--numStartedMidiIn == 0) {
	TRACE("Stopping thread for midi-in\n");
	end_thread = 1;
	if (WaitForSingleObject(hThread, 5000) != WAIT_OBJECT_0) {
	    WARN("Thread end not signaled, force termination\n");
	    TerminateThread(hThread, 0);
	}
    	TRACE("Stopped thread for midi-in\n");
    }
    midiCloseSeq(MidiInDev[wDevID].fd);
    MidiInDev[wDevID].fd = -1;

    MIDI_NotifyClient(wDevID, MIM_CLOSE, 0L, 0L);
    MidiInDev[wDevID].midiDesc.hMidi = 0;
    return ret;
}

/*======================================================================*
 *                  	    MIDI entry points 				*
 *======================================================================*/

/**************************************************************************
 * 			midMessage (WINEOSS.@)
 */
DWORD WINAPI OSS_midMessage(UINT wDevID, UINT wMsg, DWORD_PTR dwUser,
			    DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    struct midi_in_message_params params;
    struct notify_context notify;
    UINT err;

    TRACE("(%04X, %04X, %08lX, %08lX, %08lX);\n",
	  wDevID, wMsg, dwUser, dwParam1, dwParam2);
    switch (wMsg) {
    case DRVM_INIT:
        return OSS_MidiInit();
    case DRVM_EXIT:
        return OSS_MidiExit();
    case MIDM_OPEN:
	return midOpen(wDevID, (LPMIDIOPENDESC)dwParam1, dwParam2);
    case MIDM_CLOSE:
	return midClose(wDevID);
    }

    params.dev_id = wDevID;
    params.msg = wMsg;
    params.user = dwUser;
    params.param_1 = dwParam1;
    params.param_2 = dwParam2;
    params.err = &err;
    params.notify = &notify;

    do
    {
        OSS_CALL(midi_in_message, &params);
        if ((!err || err == ERROR_RETRY) && notify.send_notify) notify_client(&notify);
    } while (err == ERROR_RETRY);

    return err;
}

/**************************************************************************
 * 				modMessage (WINEOSS.@)
 */
DWORD WINAPI OSS_modMessage(UINT wDevID, UINT wMsg, DWORD_PTR dwUser,
			    DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    struct midi_out_message_params params;
    struct notify_context notify;
    UINT err;

    TRACE("(%04X, %04X, %08lX, %08lX, %08lX);\n",
	  wDevID, wMsg, dwUser, dwParam1, dwParam2);

    switch (wMsg) {
    case DRVM_INIT:
        return OSS_MidiInit();
    case DRVM_EXIT:
        return OSS_MidiExit();
    }

    params.dev_id = wDevID;
    params.msg = wMsg;
    params.user = dwUser;
    params.param_1 = dwParam1;
    params.param_2 = dwParam2;
    params.err = &err;
    params.notify = &notify;

    OSS_CALL(midi_out_message, &params);

    if (!err && notify.send_notify) notify_client(&notify);

    return err;
}

/**************************************************************************
 * 				DriverProc (WINEOSS.1)
 */
LRESULT CALLBACK OSS_DriverProc(DWORD_PTR dwDevID, HDRVR hDriv, UINT wMsg,
                                LPARAM dwParam1, LPARAM dwParam2)
{
     TRACE("(%08lX, %p, %08X, %08lX, %08lX)\n",
           dwDevID, hDriv, wMsg, dwParam1, dwParam2);

    switch(wMsg) {
    case DRV_LOAD:
    case DRV_FREE:
    case DRV_OPEN:
    case DRV_CLOSE:
    case DRV_ENABLE:
    case DRV_DISABLE:
    case DRV_QUERYCONFIGURE:
    case DRV_CONFIGURE:
        return 1;
    case DRV_INSTALL:
    case DRV_REMOVE:
        return DRV_SUCCESS;
    default:
	return 0;
    }
}
