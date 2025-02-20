///////////////////////////////////////////////////////////////////////////////
// FILE:          WDI.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Adapter for WDI hardware auto-focus devices
//
// AUTHOR:        Nenad Amodaj
//
// COPYRIGHT:     Lumencor Inc, 2025
//
// 
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "MMDevice.h"
#include "DeviceBase.h"

//////////////////////////////////////////////////////////////////////////////
// Error codes
//

#define ERR_WDI_CMD_FAILED           71001
#define ERR_WDI_INVALID_CONNECTION   71002
#define ERR_WDI_SERVICE_STAGE        71003
#define ERR_WDI_AF_ENABLE            71004
#define ERR_WDI_AF_CONTROLLER        71005

static const char* g_WDIStage = "WDIStage";
static const char* g_Prop_SN = "SerialNumber";
static const char* g_Prop_Firmware = "Firmware";
static const char* g_Prop_Connection = "Connection";
static const char* g_Prop_ServiceStageLabel = "ServiceStage";
static const char* g_Prop_ServiceControllerLabel = "ServiceController";
static const char* g_Prop_StepSizeUm = "StepSizeUm";
static const char* g_Prop_MakeZero = "MakeZero";
static const char* g_Prop_Tracking = "Tracking";
static const char* g_Prop_Laser = "Laser";
static const char* g_Prop_EnableAF = "AutoFocus";



#define WDI_DEVICE_VERSION "1.0.1"

//////////////////////////////////////////////////////////////////////////////
// WDIStage
// The AF module acts as a Z stage with focusing capabilities
// 
//////////////////////////////////////////////////////////////////////////////

class CWDIStage : public CStageBase<CWDIStage>
{
public:
   CWDIStage();
   ~CWDIStage();

   bool Busy();
   void GetName(char* pszName) const;

   int Initialize();
   int Shutdown();

   // Stage API
   int Home();
   int SetPositionUm(double pos);
   int SetRelativePositionUm(double deltaPos);
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
   int OnConnection(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnServiceStageLabel(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnServiceControllerLabel(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnMakeZero(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnStepSizeUm(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnTrack(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnLaser(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnAutoFocus(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
   std::string connection;
   std::string dofStageName;
   std::string afControllerName;
   bool initialized;
   double stepSizeUm;
   int currentStepPosition;
   bool tracking;
   bool laserEnable;
   const double delayPerUmMs = 0.5; // wait time per micron of movement

   MM::Stage* GetServiceStage();
   MM::Device* GetServiceController();
   int EnableAF(bool state);
   int GetEnableAF(bool& state);
};
