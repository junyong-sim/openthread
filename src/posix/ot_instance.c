/*
 *  Copyright (c) 2018, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include "openthread-posix-config.h"

#include <openthread/config.h>

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

#include <openthread/cli.h>
#include <openthread/diag.h>
#include <openthread/logging.h>
#include <openthread/tasklet.h>
#include <openthread/thread.h>
#include <openthread/platform/radio.h>
#include <openthread/ot_cmd.h>

#include <common/code_utils.hpp>
#include <lib/platform/exit_code.h>
#include <openthread/openthread-system.h>
#include <openthread/platform/misc.h>

#define MULTIPLE_INSTANCE_MAX 10

static otInstance *gInstance = NULL;
static pthread_t gThreadId;
pthread_mutex_t gLock;
bool useOtCmd = 0;
int gOtCmd = 0;
bool gProcessCmds = 0;
const otOperationalDatasetTlvs *gDataset = NULL;
bool gTerminate = 0;

typedef struct PosixConfig
{
    otPlatformConfig mPlatformConfig;    ///< Platform configuration.
    otLogLevel       mLogLevel;          ///< Debug level of logging.
    bool             mPrintRadioVersion; ///< Whether to print radio firmware version.
    bool             mIsVerbose;         ///< Whether to print log to stderr.
} PosixConfig;

static otInstance *InitInstance(PosixConfig *aConfig)
{
    otInstance *instance = NULL;

    syslog(LOG_INFO, "Running %s", otGetVersionString());
    syslog(LOG_INFO, "Thread version: %hu", otThreadGetVersion());
    IgnoreError(otLoggingSetLevel(aConfig->mLogLevel));

    instance = otSysInit(&aConfig->mPlatformConfig);
    VerifyOrDie(instance != NULL, OT_EXIT_FAILURE);
    syslog(LOG_INFO, "Thread interface: %s", otSysGetThreadNetifName());

    if (aConfig->mPrintRadioVersion)
    {
        printf("%s\n", otPlatRadioGetVersionString(instance));
    }
    else
    {
        syslog(LOG_INFO, "RCP version: %s", otPlatRadioGetVersionString(instance));
    }

    if (aConfig->mPlatformConfig.mDryRun)
    {
        exit(OT_EXIT_SUCCESS);
    }

    return instance;
}

void otPlatReset(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    otSysDeinit();

    assert(false);
}

void processCmds()
{
    if(!gProcessCmds) {
        //syslog(LOG_INFO, "no ot cmds to process!!!");
        return;
    }

    syslog(LOG_INFO, "processCmds [%d]", gOtCmd);
    useOtCmd = 0;
    switch(gOtCmd) {
    case OT_CMD_IFCONFIG_UP:
        syslog(LOG_INFO, "OT_CMD_IFCONFIG_UP!!!");
        otIp6SetEnabled(gInstance, true);
        break;
    case OT_CMD_IFCONFIG_DOWN:
        syslog(LOG_INFO, "OT_CMD_IFCONFIG_DOWN!!!");
        otIp6SetEnabled(gInstance, false);
        break;  
    case OT_CMD_THREAD_START:
        syslog(LOG_INFO, "OT_CMD_THREAD_START!!!");
        otThreadSetEnabled(gInstance, true);
        break;  
    case OT_CMD_THREAD_STOP:
        syslog(LOG_INFO, "OT_CMD_THREAD_STOP!!!");
        otThreadSetEnabled(gInstance, false);
        break;
    case OT_CMD_SET_ACTIVE_DATSET:
        syslog(LOG_INFO, "OT_CMD_SET_ACTIVE_DATSET!!!");
        otDatasetSetActiveTlvs(gInstance, gDataset);
        break;
    default:
        syslog(LOG_INFO, "invalid ot command!!!");
    }

    syslog(LOG_INFO, "ot cmd  = [%d] processed", gOtCmd);
    useOtCmd = 1;
    gOtCmd = 0;
    gProcessCmds = 0;
}

static int getRadioURL(int intefaceIdx) {
    struct stat st;
    int i = 0;
    char radioDevice[20] = { 0, };

    /* For multiple devices and intefaces, 
     * we will align the index of them.
     */
    sprintf(radioDevice, "/dev/ttyACM%d", intefaceIdx);
    if(stat(radioDevice, &st) == 0) {
        syslog(LOG_INFO, "radio device found [%s]", radioDevice);
        return intefaceIdx;
    }

    /* original routine */
    for(i = 0; i < MULTIPLE_INSTANCE_MAX; i++) {
        sprintf(radioDevice, "/dev/ttyACM%d", i);
        if(stat(radioDevice, &st) == 0) {
            syslog(LOG_INFO, "radio device found [%s]", radioDevice);
            break;
        } else {
            syslog(LOG_INFO, "Not valid radio device [%s]....Try another", radioDevice);
        }
    }
    if(i == MULTIPLE_INSTANCE_MAX) {
        syslog(LOG_INFO, "Not valid radio device found!!!!");
        return -1;
    }

    return i;
}

static int getInterface() {
    struct stat st;
    int i = 0;
    char wpanInterface[50] = { 0, };

    for(i = 0; i < MULTIPLE_INSTANCE_MAX; i++) {
        sprintf(wpanInterface, "/sys/class/net/wpan%d", i);
        if(stat(wpanInterface, &st) == 0) {
            syslog(LOG_INFO, "Interface is already used [%s]", wpanInterface);
            continue;
        } else {
            syslog(LOG_INFO, "find empty interface [%s]", wpanInterface);
            break;
        }
    }
    if(i == MULTIPLE_INSTANCE_MAX) {
        syslog(LOG_INFO, "Not valid radio device found!!!!");
        return -1;
    }

    return i;
}

void otCreateInstance()
{
    pthread_mutex_lock(&gLock);
    PosixConfig config;
    int radioUrlIdx = 0;
    int interfaceIdx = 0;
    char radioUrl[100] = { 0, };
    char iface[100] = { 0, };

    memset(&config, 0, sizeof(config));

    syslog(LOG_INFO, "otCreateInstance");
	
    interfaceIdx = getInterface();
    sprintf(iface, "wpan%d", interfaceIdx);
    syslog(LOG_INFO, "interface found [%s]", iface);

    radioUrlIdx = getRadioURL(interfaceIdx);
    if (radioUrlIdx == -1) {
        return;
    }
    sprintf(radioUrl, "spinel+hdlc+uart:///dev/ttyACM%d", radioUrlIdx);
    syslog(LOG_INFO, "radio Url found [%s]", radioUrl);

    config.mLogLevel = OT_LOG_LEVEL_DEBG;
    config.mIsVerbose = true;
    config.mPlatformConfig.mInterfaceName = iface;
    VerifyOrDie(config.mPlatformConfig.mRadioUrlNum < OT_ARRAY_LENGTH(config.mPlatformConfig.mRadioUrls),
        OT_EXIT_INVALID_ARGUMENTS);
    config.mPlatformConfig.mRadioUrls[config.mPlatformConfig.mRadioUrlNum++] = radioUrl;
    config.mPlatformConfig.mRealTimeSignal = 41;
    config.mPlatformConfig.mSpeedUpFactor = 1;
	
	
    gInstance = InitInstance(&config);
    syslog(LOG_INFO, "ot instance create success!!!");
    syslog(LOG_INFO, "if config up");
    otIp6SetEnabled(gInstance, true);
    syslog(LOG_INFO, "thread start");
    otThreadSetEnabled(gInstance, true);
        
    useOtCmd = 1;
    pthread_mutex_unlock(&gLock);
	
    while (gTerminate == false)
    {
        // syslog(LOG_INFO, "Enter thread mainloop");
	
        otSysMainloopContext mainloop;

        otTaskletsProcess(gInstance);

        FD_ZERO(&mainloop.mReadFdSet);
        FD_ZERO(&mainloop.mWriteFdSet);
        FD_ZERO(&mainloop.mErrorFdSet);

        mainloop.mMaxFd           = -1;
        mainloop.mTimeout.tv_sec  = 10;
        mainloop.mTimeout.tv_usec = 0;

        otSysMainloopUpdate(gInstance, &mainloop);

        if (otSysMainloopPoll(&mainloop) >= 0)
        {
            otSysMainloopProcess(gInstance, &mainloop);
        }
        else if (errno != EINTR)
        {
            perror("select");
            ExitNow();
        }
        processCmds();
        // syslog(LOG_INFO, "Exit thread mainloop");
    }
	
exit:
    otSysDeinit();
    gThreadId = 0;
    gInstance = NULL;
    gTerminate = false;
    syslog(LOG_INFO, "terminate thread mainloop : exit");
    return;
	
}

void *otThreadMainLoop()
{
	syslog(LOG_INFO, "Inside otThreadMainLoop");
	
	otCreateInstance();
	
	return NULL;
}

void otGetInstance(otInstance **instance, pthread_t *instanceId) {
	
	syslog(LOG_INFO, "otGetInstance");
	
	 if(gInstance) {
        syslog(LOG_INFO, "ot instance already initialised!!!");
		*instance = gInstance;
		instanceId = &gThreadId;
		return;
    }

    if (pthread_mutex_init(&gLock, NULL) != 0) {
        syslog(LOG_INFO,  "mutex init has failed");
		return;
    }
	
    syslog(LOG_INFO, "Before otThread");
    pthread_create(&gThreadId, NULL, otThreadMainLoop, NULL);
	
    syslog(LOG_INFO, "Wait for 1 Seconds to initialise openthread stack !!!");
    sleep(1);

    pthread_mutex_lock(&gLock);
	
    *instance = gInstance;
    *instanceId = gThreadId;
    syslog(LOG_INFO, "After otThread : gThreadId[%ld]", gThreadId);

    pthread_mutex_unlock(&gLock);

    return;
}

void otWait() {
    syslog(LOG_INFO, "otWait");
    pthread_join(gThreadId, NULL);
}

void otDestroyInstance()  {
    syslog(LOG_INFO, "otDestroyInstance");

    pthread_mutex_destroy(&gLock);
    gOtCmd = 0;
    gProcessCmds = 0;
    useOtCmd = 0;
    gDataset = NULL;
    gTerminate = true;
}