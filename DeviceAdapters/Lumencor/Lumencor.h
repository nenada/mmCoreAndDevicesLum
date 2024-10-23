///////////////////////////////////////////////////////////////////////////////
// FILE:          Lumencor.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Lumencor Light Engine driver for all light engines
//						including GEN3
//
// AUTHOR:        Nenad Amodaj
//
// COPYRIGHT:     Lumencor 2019 - 2021         
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

#ifndef _LUMENCOR_H_
#define _LUMENCOR_H_

#include "MMDevice.h"
#include "DeviceBase.h"

//////////////////////////////////////////////////////////////////////////////
// Error codes
//
#define ERR_PORT_CHANGE_FORBIDDEN    13001
#define ERR_PARAMETER_ERROR          13002
#define ERR_INIT							 13003
#define ERR_INTERNAL						 13004

#define ERR_DOVER_CMD_FAILED         13005
#define ERR_DOVER_HOME_FAILED        13006
#define ERR_DOVER_LIMITS_EXCEEDED    13007

#define ERR_TTL_CHANNEL_NAME         13101
#define ERR_TTL_COMMAND_FAILED       13102
#define ERR_TTL_INVALID_SEQUENCE     13103



static const char* g_LightEngine = "LightEngine";
static const char* g_TTLSwitch = "TTLSwitch";
static const char* g_DoverStage = "DoverStage";
static const char* g_DoverXYStage = "DoverXYStage";
static const char* g_Prop_Connection = "Connection";
static const char* g_Prop_ComPort = "TTLGENComPort";
static const char* g_Prop_Model = "Model";
static const char* g_Prop_ModelName = "LEModel";
static const char* g_Prop_SerialNumber = "SerialNumber";
static const char* g_Prop_FirmwareVersion = "FirmwareVersion";
static const char* g_Prop_ModuleVersion = "ModuleVersion";

#define LUMENCOR_DEV_VERSION "1.0.1"

class LightEngineAPI;

namespace dover
{
   class XYStage;
   class DOF5Stage;
}

class LightEngine : public CShutterBase<LightEngine>
{
public:
   LightEngine();
   ~LightEngine();

   // Device API
   // ----------
   int Initialize();
   int Shutdown();

   void GetName(char* pszName) const;
   bool Busy();

   // Shutter API
   // ---------
   int SetOpen(bool open = true);
   int GetOpen(bool& open);
   int Fire(double /*interval*/) { return DEVICE_UNSUPPORTED_COMMAND; }

   // action interface
   // ----------------
   int OnConnection(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnModel(MM::PropertyBase* pProp, MM::ActionType eAct);

   int OnState(MM::PropertyBase* pProp, MM::ActionType eAct);

   int OnChannelEnable(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnChannelIntensity(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
   bool initialized;
	void* engine;
	std::string model;
	std::string connection;
	std::vector<std::string> channels;
	std::map<std::string, int> channelLookup;
	bool shutterState;
	std::vector<bool> channelStates; // cache for channel states

	int RetrieveError();
	int ZeroAll();
	int ApplyStates();
	int TurnAllOff();
};

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

private:
   dover::DOF5Stage* zStage;
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
   int OnPosition(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
   dover::XYStage* xyStage;
   bool initialized;
};

//////////////////////////////////////////////////////////////////////////////
// CTTLSwitch
// TTL controlled light source with hardware timing and sequencing
// Requires TTLGEN device attached to the light engine
//////////////////////////////////////////////////////////////////////////////

struct ChannelInfo
{
   std::string name;
   int channelId;
   int ttlId;
   double exposureMs;

   ChannelInfo() : channelId(0), ttlId(0), exposureMs(5.0) {}
};

class CTTLSwitch : public CStateDeviceBase<CTTLSwitch>
{
public:
   CTTLSwitch();
   ~CTTLSwitch();

   // MMDevice API
   // ------------
   int Initialize();
   int Shutdown();

   void GetName(char* pszName) const;
   bool Busy() { return false; }

   unsigned long GetNumberOfPositions()const { return (unsigned long)channels.size(); }

   // action interface
   // ----------------
   int OnState(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnLabel(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnSequence(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnConnection(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnChannelIntensity(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnPort(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnChannelExposure(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
   int LoadChannelSequence(const std::vector<int>& channelSequence);
   int SetTTLController(const ChannelInfo& inf);

   void* engine;
   bool initialized;
   std::string model;
   std::string connection;
   std::string ttlPort;
   std::vector<std::string> channels;
   std::map<std::string, ChannelInfo> channelLookup;
   int currentChannel;

   int RetrieveError();
   int ZeroAll();
   int TurnAllOff();

};

#endif //_LUMENCOR_H_
