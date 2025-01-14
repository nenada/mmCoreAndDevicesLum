///////////////////////////////////////////////////////////////////////////////
// FILE:          Dover.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Dover Stages
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
#pragma once

#include "MMDevice.h"
#include "DeviceBase.h"

//////////////////////////////////////////////////////////////////////////////
// Error codes
//

#define ERR_DOVER_CMD_FAILED         13005
#define ERR_DOVER_HOME_FAILED        13006
#define ERR_DOVER_LIMITS_EXCEEDED    13007
#define ERR_DOVER_INITIALIZE         13008
#define ERR_DOVER_DLL_LOAD           13009
#define ERR_DOVER_DLL_FUNCTION_LOAD  13010


static const char* g_DoverStage = "DoverStage";
static const char* g_DoverXYStage = "DoverXYStage";
static const char* g_Prop_ModuleVersion = "ModuleVersion";
static const char* g_Prop_DoverX = "X";
static const char* g_Prop_DoverY = "Y";
static const char* g_Prop_MoveDistancePerPulse = "MoveDistancePerPulse";

#define DOVER_DEVICE_VERSION "1.0.2"

//////////////////////////////////////////////////////////////////////////////
// CDoverStage
// Single-axis Dover stage
//////////////////////////////////////////////////////////////////////////////

class CDoverStage : public CStageBase<CDoverStage>
{
public:
   CDoverStage();
   ~CDoverStage();

   bool Busy();
   void GetName(char* pszName) const;

   int Initialize();
   int Shutdown();

   // Stage API
   int Home();
   int SetPositionUm(double pos);
   int GetPositionUm(double& pos);
   double GetStepSize();
   int SetPositionSteps(long steps);
   int GetPositionSteps(long& steps);
   int GetLimits(double& lower, double& upper);
   int SetOrigin() { return DEVICE_UNSUPPORTED_COMMAND; }

   bool IsContinuousFocusDrive() const { return false; }
   int IsStageSequenceable(bool& isSequenceable) const { isSequenceable = false; return DEVICE_OK; }


   // action interface
   // ----------------
   int OnPosition(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnMoveDistancePerPulse(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
   void* zStage;
   bool initialized;
};

//////////////////////////////////////////////////////////////////////////////
// CDoverXYStage
// Dover XY stage
//////////////////////////////////////////////////////////////////////////////

class CDoverXYStage : public CXYStageBase<CDoverXYStage>
{
public:
   CDoverXYStage();
   ~CDoverXYStage();

   bool Busy();
   void GetName(char* pszName) const;

   int Initialize();
   int Shutdown();

   double GetStepSize();
   int SetPositionSteps(long x, long y);
   int GetPositionSteps(long& x, long& y);
   int Home();
   int Stop();

   int GetLimitsUm(double& xMin, double& xMax, double& yMin, double& yMax);
   int GetStepLimits(long& /*xMin*/, long& /*xMax*/, long& /*yMin*/, long& /*yMax*/);
   double GetStepSizeXUm();
   double GetStepSizeYUm();

   int IsXYStageSequenceable(bool& isSequenceable) const { isSequenceable = false; return DEVICE_OK; }
   int SetOrigin() { return DEVICE_UNSUPPORTED_COMMAND; }

   // action interface
   // ----------------
   int OnPositionX(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnPositionY(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnMoveDistancePerPulse(MM::PropertyBase* pProp, MM::ActionType eAct);


private:
   void* xyStage;
   bool initialized;
};

