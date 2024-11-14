///////////////////////////////////////////////////////////////////////////////
// FILE:          Dover.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Dover stages
//
// AUTHOR:        Nenad Amodaj
//
// COPYRIGHT:     Lumencor 2024         
//
// LICENSE:       This file is distributed under the BSD license.
//                License text is included with the source distribution.
//
//                This file is distributed in the hope that it will be useful,
//                but WITHOUT ANY WARRANTY; without even the implied warranty
//                of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//                IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//                CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//                INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.
//

#include "Dover.h"
#include "ModuleInterface.h"

using namespace std;


///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////
MODULE_API void InitializeModuleData()
{
	RegisterDevice(g_DoverStage, MM::StageDevice, "Dover DOF5 Z Stage");
	RegisterDevice(g_DoverXYStage, MM::XYStageDevice, "Dover XY Stage");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
   if (deviceName == 0)
      return 0;

	if (strcmp(deviceName, g_DoverStage) == 0)
	{
		return new CDoverStage();
	}
	else if (strcmp(deviceName, g_DoverXYStage) == 0)
	{
		return new CDoverXYStage();
	}


   return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
   delete pDevice;
}
