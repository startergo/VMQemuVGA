/*
 *  VMQemuVGAClient.cpp
 *  VMsvga2
 *
 *  Created by Zenith432 on July 4th 2009.
 *  Copyright 2009-2011 Zenith432. All rights reserved.
 *
 */

/**********************************************************
 * Portions Copyright 2009 VMware, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************/

#include "VMQemuVGAClient.h"
#include "VMQemuVGA.h"

#define super IOUserClient

OSDefineMetaClassAndStructors(VMQemuVGAClient, IOUserClient);

bool VMQemuVGAClient::start(IOService* provider)
{
	IOLog("%s: Starting user client for provider %s\n", __FUNCTION__, provider->getName());
	
	if (!super::start(provider)) {
		IOLog("%s: super::start() failed\n", __FUNCTION__);
		return false;
	}
	
	IOLog("%s: User client started successfully\n", __FUNCTION__);
	return true;
}

static IOExternalMethod const iofbFuncsCache[1] =
{
	{nullptr, reinterpret_cast<IOMethod>(&VMQemuVGA::CustomMode), kIOUCStructIStructO, sizeof(CustomModeData), sizeof(CustomModeData)}
};

// VirtIO framebuffer doesn't need custom methods - return empty method table
// This signals to WindowServer that no external methods are available (standard IOFramebuffer only)
static IOExternalMethod const virtioEmptyMethod = {0};

IOExternalMethod* VMQemuVGAClient::getTargetAndMethodForIndex(IOService** targetP, UInt32 index)
{
	IOLog( "%s: index=%u.\n", __FUNCTION__, static_cast<unsigned>(index));
	if (!targetP)
		return 0;
	
	// Get provider and validate it exists
	IOService* provider = getProvider();
	if (!provider) {
		IOLog( "%s: No provider available\n", __FUNCTION__);
		return nullptr;
	}
	
	// For index validation - only 0 and 3 are valid
	if (index != 0 && index != 3) {
		IOLog( "%s: Invalid index %u.\n",
				  __FUNCTION__, static_cast<unsigned>(index));
		return 0;
	}
	
	// Determine framebuffer type dynamically
	// VirtIO framebuffers use ONLY standard IOFramebuffer methods (no custom methods)
	// QXL/legacy framebuffers use CustomMode method for resolution changes
	const char* providerName = provider->getName();
	bool isVirtIOFramebuffer = false;
	
	if (providerName) {
		// Check if this is a VirtIO-based framebuffer (multiple possible names)
		if (0 == strcmp(providerName, "VMVirtIOFramebuffer") ||
		    0 == strcmp(providerName, "VMVirtIOGPU")) {
			isVirtIOFramebuffer = true;
		}
		IOLog( "%s: Provider=%s, isVirtIO=%d\n", __FUNCTION__, providerName, isVirtIOFramebuffer);
	}
	
	*targetP = provider;
	
	if (isVirtIOFramebuffer) {
		// VirtIO: Return NULL = no custom methods, use standard IOFramebuffer API only
		IOLog( "%s: VirtIO framebuffer - NO custom methods (standard IOFramebuffer only)\n", __FUNCTION__);
		return nullptr;
	} else {
		// QXL/legacy: Return CustomMode method
		IOLog( "%s: QXL framebuffer - returning method table (CustomMode)\n", __FUNCTION__);
		return const_cast<IOExternalMethod*>(&iofbFuncsCache[0]);
	}
}

IOReturn VMQemuVGAClient::clientClose()
{
	IOLog( "%s()\n", __FUNCTION__);
	if (!terminate())
		IOLog( "%s: terminate failed.\n", __FUNCTION__);
	return kIOReturnSuccess;
}

bool VMQemuVGAClient::initWithTask(task_t owningTask, void* securityToken, UInt32 type)
{
	if (!super::initWithTask(owningTask, securityToken, type) ||
		clientHasPrivilege(securityToken, kIOClientPrivilegeAdministrator) != kIOReturnSuccess)
		return false;
	return true;
}
