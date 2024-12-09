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

#define ERR_TTL_CHANNEL_NAME         13101
#define ERR_TTL_COMMAND_FAILED       13102
#define ERR_TTL_INVALID_SEQUENCE     13103


static const char* g_LightEngine = "LightEngine";
static const char* g_TTLSwitch = "TTLSwitch";
static const char* g_Prop_Connection = "Connection";
static const char* g_Prop_ComPort = "TTLGENComPort";
static const char* g_Prop_Model = "Model";
static const char* g_Prop_ModelName = "LEModel";
static const char* g_Prop_SerialNumber = "SerialNumber";
static const char* g_Prop_FirmwareVersion = "FirmwareVersion";
static const char* g_Prop_ModuleVersion = "ModuleVersion";
static const char* g_prop_ChannelSequence = "ChannelSequence";
static const char* g_prop_RunSequence = "RunSequence";

#define LUMENCOR_DEV_VERSION "1.0.3"

class LightEngineAPI;

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
   int OnChannelSequence(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnConnection(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnChannelIntensity(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnPort(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnChannelExposure(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnRunSequence(MM::PropertyBase* pProp, MM::ActionType eAct);


private:
   int LoadChannelSequence(const std::vector<int>& channelSequence);
   int LoadChannelSequence(const std::vector<std::string>& sequence);
   int SetTTLController(const ChannelInfo& inf, double delayMs=0.0);
   int RunSequence();

   void* engine;
   bool initialized;
   bool demo;
   std::string model;
   std::string connection;
   std::string ttlPort;
   std::vector<std::string> channels;
   std::map<std::string, ChannelInfo> channelLookup;
   int currentChannel;
   std::string channelSequenceCmd;

   int RetrieveError();
   int ZeroAll();
   int TurnAllOff();

};

#endif //_LUMENCOR_H_
