/* Copyright 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "comm-host.h"
#include "ec_commands.h"
#include "misc_util.h"

#include <IOKit/IOKitLib.h>
#include <mach/mach_error.h>
#include <iostream>
#include "../extern/CrosECOSX/Include/CrosECShared.h"

static io_connect_t servicePort;

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(t) (sizeof(t) / sizeof(t[0]))
#endif

int comm_init_lpc(void) {return -1; }
int comm_init_i2c(int i2c_bus) { return -1; }
int comm_init_servo_spi(const char *device_name) { return -1; }

static const char * const meanings[] = {
	"SUCCESS",
	"INVALID_COMMAND",
	"ERROR",
	"INVALID_PARAM",
	"ACCESS_DENIED",
	"INVALID_RESPONSE",
	"INVALID_VERSION",
	"INVALID_CHECKSUM",
	"IN_PROGRESS",
	"UNAVAILABLE",
	"TIMEOUT",
	"OVERFLOW",
	"INVALID_HEADER",
	"REQUEST_TRUNCATED",
	"RESPONSE_TOO_BIG",
	"BUS_ERROR",
	"BUSY",
	"INVALID_HEADER_VERSION",
	"INVALID_HEADER_CRC",
	"INVALID_DATA_CRC",
	"DUP_UNAVAILABLE",
};

static const char *strresult(int i)
{
	if (i < 0 || i >= ARRAY_SIZE(meanings))
		return "<unknown>";
	return meanings[i];
}

static int ec_command_osx(int command, int version,
			     const void *outdata, int outsize,
			     void *indata, int insize)
{
    CrosECUserCommandRequest request {0};
    CrosECUserCommandResponse response {0};
    int r = 0;
    kern_return_t kernResult;
    size_t requestSize, responseSize;

	assert(outsize == 0 || outdata != NULL);
	assert(insize == 0 || indata != NULL);

    request.command = command;
    request.version = version;
    request.maxReceiveSize = insize;
    request.sendSize = outsize;
    memcpy(request.sendBuffer, outdata, outsize);
    
    requestSize = sizeof(request);
    responseSize = sizeof(response);
    
    kernResult = IOConnectCallStructMethod(servicePort, kCrosUser_Command, &request, requestSize, &response, &responseSize);

    if (kernResult != KERN_SUCCESS) {
        std::cerr << "Failed command! " << mach_error_string(kernResult) << std::endl;
        r = -EC_RES_ERROR;
    } else {
        memcpy(indata, response.receiveBuffer, response.receivedSize);
        r = response.receivedSize;
        if (response.ecResponse != EC_RES_SUCCESS) {
            std::cerr << "EC result " << response.ecResponse << " (" << strresult(response.ecResponse) << ")" << std::endl;
			r = -EECRESULT - response.ecResponse;
        }
    }

	return r;
}

static int ec_readmem_osx(int offset, int bytes, void *dest)
{
    CrosECUserReadMemoryRequest request {0};
    CrosECUserReadMemoryResponse response {0};
    size_t requestSize, responseSize;
    kern_return_t kernResult;
    int ret;

    request.offset = offset;

    if (bytes == 0) {
        request.readSize = kCrosECUserReadStringSize;
    } else {
        request.readSize = bytes;
    }
    
    requestSize = sizeof(request);
    responseSize = sizeof(response);
    
    kernResult = IOConnectCallStructMethod(servicePort, kCrosUser_ReadMem, &request, requestSize, &response, &responseSize);

    if (kernResult != KERN_SUCCESS) {
        std::cerr << "Failed Read Mem! " << mach_error_string(kernResult) << std::endl;
        return -1;
    } else {
        ret = bytes == 0 ? response.stringSize : bytes;
        memcpy(dest, response.data, ret);
        return ret;
    }
}

static int ec_pollevent_dev(unsigned long mask, void *buffer, size_t buf_size,
			    int timeout)
{
	return 0;
#if 0 // TODO: Not Implemented
	int rv;
	struct pollfd pf = { .fd = fd, .events = POLLIN };

	ioctl(fd, CROS_EC_DEV_IOCEVENTMASK_V2, mask);

	rv = poll(&pf, 1, timeout);
	if (rv != 1)
		return rv;

	if (pf.revents != POLLIN)
		return -pf.revents;

	return read(fd, buffer, buf_size);
#endif
}

int comm_init_dev(const char *device_name)
{
	int (*ec_cmd_readmem)(int offset, int bytes, void *dest);
    io_service_t service;
    kern_return_t kernResult;
    char version[2];
    
    service = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("CrosECBus"));
    if (service == 0) {
        std::cerr << "No Chrome EC!" << std::endl;
        return -1;
    }
    
    kernResult = IOServiceOpen(service, mach_task_self(), 0, &servicePort);
    if (kernResult != KERN_SUCCESS) {
        std::cerr << "Failed to open EC!" << std::endl;
        return -2;
    }

	ec_command_proto = ec_command_osx;
	ec_cmd_readmem = ec_readmem_osx;

	if (ec_cmd_readmem(EC_MEMMAP_ID, 2, version) == 2 &&
	    version[0] == 'E' && version[1] == 'C')
		ec_readmem = ec_cmd_readmem;
	ec_pollevent = ec_pollevent_dev;

	/*
	 * Set temporary size, will be updated later.
	 */
	ec_max_outsize = EC_LPC_HOST_PACKET_SIZE - sizeof(struct ec_host_request);
	ec_max_insize = EC_LPC_HOST_PACKET_SIZE - sizeof(struct ec_host_response);

	return 0;
}
