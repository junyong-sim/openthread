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

#include <common/code_utils.hpp>
#include <lib/platform/exit_code.h>
#include <openthread/openthread-system.h>
#include <openthread/platform/misc.h>

static otInstance *gInstance = NULL;
static pthread_t gThreadId;
pthread_mutex_t gLock;

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

void otCreateInstance()
{
    pthread_mutex_lock(&gLock);
    PosixConfig config;
    struct stat st;
    int i = 0;
    int rval = 0;
    char radioDevicesList[][20] = {
        "/dev/ttyACM0",
        "/dev/ttyACM1",
        "/dev/ttyACM2",
        "/dev/ttyACM3",
        "/dev/ttyACM4",
    };
    char iface[] = "wpan0";
    char radioUrl[][100] = {
        "spinel+hdlc+uart:///dev/ttyACM0",
        "spinel+hdlc+uart:///dev/ttyACM1",
        "spinel+hdlc+uart:///dev/ttyACM2",
        "spinel+hdlc+uart:///dev/ttyACM3",
        "spinel+hdlc+uart:///dev/ttyACM4",
    };
	
    memset(&config, 0, sizeof(config));
    
    syslog(LOG_INFO, "otCreateInstance");
	

    for(i =0; i< 5; i++) {
        if(stat(radioDevicesList[i], &st) == 0) {
            syslog(LOG_INFO, "radio device found [%s]", radioDevicesList[i]);
            break;
        } else {
             syslog(LOG_INFO, "Not valid radio device [%s]....Try another", radioDevicesList[i]);
        }
    }
    if(i == 5) {
    syslog(LOG_INFO, "Not valid radio device found!!!!");
        return;
    }
	
    config.mLogLevel = OT_LOG_LEVEL_DEBG;
    config.mIsVerbose = true;
    config.mPlatformConfig.mInterfaceName = iface;
    VerifyOrDie(config.mPlatformConfig.mRadioUrlNum < OT_ARRAY_LENGTH(config.mPlatformConfig.mRadioUrls),
        OT_EXIT_INVALID_ARGUMENTS);
    config.mPlatformConfig.mRadioUrls[config.mPlatformConfig.mRadioUrlNum++] = radioUrl[i];
    config.mPlatformConfig.mRealTimeSignal = 41;
    config.mPlatformConfig.mSpeedUpFactor = 1;
	
	
    gInstance = InitInstance(&config);
    syslog(LOG_INFO, "ot instance create success!!!");

	otIp6SetEnabled(gInstance, true);
	syslog(LOG_INFO, "if config up!!!");
	otThreadSetEnabled(gInstance, true);
	syslog(LOG_INFO, "thread start!!!");

	pthread_mutex_unlock(&gLock);
	
	while (true)
    {
		//syslog(LOG_INFO, "Enter thread mainloop");
	
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
            ExitNow(rval = OT_EXIT_FAILURE);
        }
		//syslog(LOG_INFO, "Exit thread mainloop");
    }
	
exit:
    otSysDeinit();
    return;
	
}

void *otThreadMainLoop(void *vargp)
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
		instanceId = gThreadId;
		return;
    }

    if (pthread_mutex_init(&gLock, NULL) != 0) {
        syslog(LOG_INFO,  "mutex init has failed");
		return;
    }
	
    syslog(LOG_INFO, "Before otThread");
    pthread_create(&gThreadId, NULL, otThreadMainLoop, NULL);
	
    syslog(LOG_INFO, "Wait for 5 Seconds to initialise openthread stack !!!");
    sleep(5);

    pthread_mutex_lock(&gLock);
	
    *instance = gInstance;
    *instanceId = gThreadId;
    syslog(LOG_INFO, "After otThread : gThreadId[%ld]", gThreadId);

    pthread_mutex_unlock(&gLock);

    return;
}

void otWait(int instanceId) {
    syslog(LOG_INFO, "otWait");
    pthread_join(gThreadId, NULL);
}

void otDestroyInstance(otInstance **instance, int instanceId)  {
    syslog(LOG_INFO, "otDestroyInstance");
    gThreadId = 0;
    gInstance = NULL;
    pthread_mutex_destroy(&gLock);
    otSysDeinit();
}