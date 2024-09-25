///////////////////////////////////////////////////////////////////////////////
// FILE:          TTLSwitch.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   TTL Controlled light engine with hardware timing
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

#include "Lumencor.h"
#include "LightEngineAPI.h"
#include <algorithm>
#include <boost/algorithm/string.hpp>

using namespace std;

CTTLSwitch::CTTLSwitch() :
   initialized(false),
	engine(0)
{
   InitializeDefaultErrorMessages();

	   // set device specific error messages
   SetErrorText(ERR_INIT, "Light engine initialization error, see log file for details");
   SetErrorText(ERR_INTERNAL, "Internal driver error, see log file for details");
                                                                      
   // create pre-initialization properties                                   
   // ------------------------------------
   //
                                                                          
   // Name                                                                   
   CreateProperty(MM::g_Keyword_Name, g_TTLSwitch, MM::String, true);
   //
   // Description                                                            
   CreateProperty(MM::g_Keyword_Description, "Lumencor Light Engine, TTL control", MM::String, true);

	// Portd                                                                   
	CPropertyAction* pAct = new CPropertyAction(this, &CTTLSwitch::OnConnection);
	CreateProperty(g_Prop_Connection, "", MM::String, false, pAct, true);
}                                                                            
                                                                             
CTTLSwitch::~CTTLSwitch()
{                                                                            
   Shutdown();                                                               
} 

void CTTLSwitch::GetName(char* Name) const
{
   CDeviceUtils::CopyLimitedString(Name, g_TTLSwitch);
}  

int CTTLSwitch::Initialize()
{
   if (initialized)
      return DEVICE_OK;

	int ret(DEVICE_OK);

	// create light engine
	ret = lum_createLightEngine(&engine); // gen3 (universal)
	if (ret != LUM_OK)
	{
		ostringstream os;
		os << "Light Engine create() failed for model: " << model;
		LogMessage(os.str());
		return ERR_INIT;
	}

	size_t numDots = count(connection.begin(), connection.end(), '.');
	if (numDots == 3)
	{
		// interpreting destination as IP address
		ret = lum_connectTCP(engine, connection.c_str(), LUM_DEFAULT_TCP_PORT);
	}
	else
	{
		ret = lum_connectCOM(engine, connection.c_str(), LUM_STANDARD_BAUD_RATE);
	}

	if (ret != LUM_OK)
		return RetrieveError();

	// get light engine info
	// obtain model
	char engModel[LUM_MAX_MESSAGE_LENGTH];
	ret = lum_getModel(engine, engModel, LUM_MAX_MESSAGE_LENGTH);
	if (ret != LUM_OK)
		return RetrieveError();
	CreateProperty(g_Prop_ModelName, engModel, MM::String, true);

	// obtain firmware version
	char version[LUM_MAX_MESSAGE_LENGTH];
	ret = lum_getVersion(engine, version, LUM_MAX_MESSAGE_LENGTH);
	if (ret != LUM_OK)
		return RetrieveError();
	CreateProperty(g_Prop_FirmwareVersion, version, MM::String, true);

	// obtain device serial number
	char serialNumber[LUM_MAX_MESSAGE_LENGTH];
	ret = lum_getSerialNumber(engine, serialNumber, LUM_MAX_MESSAGE_LENGTH);
	if (ret != LUM_OK)
		return RetrieveError();
	CreateProperty(g_Prop_SerialNumber, serialNumber, MM::String, true);

	int maxIntensity(0);
	ret = lum_getMaximumIntensity(engine, &maxIntensity);
	if (ret != LUM_OK)
		return RetrieveError();

	// discover light channels
	int numChannels(0);
	ret = lum_getNumberOfChannels(engine, &numChannels);
	if (ret != LUM_OK)
		return RetrieveError();

	channels.clear();
	for (int i = 0; i < numChannels; i++)
	{
		char chName[LUM_MAX_MESSAGE_LENGTH];
		ret = lum_getChannelName(engine, i, chName, LUM_MAX_MESSAGE_LENGTH);
		if (ret != LUM_OK)
			return RetrieveError();

		channels.push_back(chName);
	}

	channelLookup.clear();
	for (size_t i=0; i<channels.size(); i++)
	{
		ostringstream os;
		os << channels[i] << "_" << "Intensity";
	   auto pAct = new CPropertyAction(this, &CTTLSwitch::OnChannelIntensity);
		ret = CreateProperty(os.str().c_str(), "0", MM::Integer, false, pAct);
		SetPropertyLimits(os.str().c_str(), 0, maxIntensity);

		channelLookup[channels[i]] = (int)i; 
	}

   // reset light engine
   // ------------------
	channelStates.resize(numChannels, false);
	ret = ZeroAll();
	if (ret != DEVICE_OK)
		return ret;

	ret = TurnAllOff();
	if (ret != DEVICE_OK)
		return ret;

   // State
   // -----
   auto pAct = new CPropertyAction(this, &CTTLSwitch::OnState);
   ret = CreateProperty(MM::g_Keyword_State, channels[0].c_str(), MM::Integer, false, pAct);
   if (ret != DEVICE_OK)
      return ret;                                                            
                                                                             
   AddAllowedValue(MM::g_Keyword_State, "0");                                
   AddAllowedValue(MM::g_Keyword_State, "1");                                
                                              
   // switch all channels off on startup
   SetProperty(MM::g_Keyword_State, "0");

   UpdateStatus();

   initialized = true;
   return DEVICE_OK;
}

int CTTLSwitch::Shutdown()
{
   if (initialized)
   {
		lum_disconnect(engine);
		lum_deleteLightEngine(engine);
		engine = 0;
      initialized = false;
   }
   return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////
/*
 * Sets the connection path.
 * Should be called before initialization
 */
int CTTLSwitch::OnConnection(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(connection.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      if (initialized)
      {
         // revert
         pProp->Set(connection.c_str());
         return ERR_PORT_CHANGE_FORBIDDEN;
      }
                                                                             
      pProp->Get(connection);                                                     
   }                                                                         
   return DEVICE_OK;     
}

int CTTLSwitch::OnState(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
     // TODO
      return DEVICE_OK;
   }
   else if (eAct == MM::AfterSet)
   {
      long pos;
      pProp->Get(pos);
		// TODO
      return DEVICE_OK;
   }

   return DEVICE_OK;
}

int CTTLSwitch::OnSequence(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return 0;
}

// *****************************************************************************
// Property handlers
// *****************************************************************************

int CTTLSwitch::OnChannelIntensity(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	vector<string> tokens;
	string name = pProp->GetName();
	boost::split(tokens, name, boost::is_any_of("_"));
	if (tokens.size() == 0)
	{
		ostringstream os;
		os << "Invalid channel name: " << pProp->GetName();
		LogMessage(os.str());
		return ERR_INTERNAL;
	}

	string channel = tokens[0];
	map<string, int>::iterator it = channelLookup.find(channel);
	if (it == channelLookup.end())
	{
		ostringstream os;
		os << "Invalid channel name: " << channel;
		LogMessage(os.str());
		return ERR_INTERNAL;
	}

	int channelIdx = it->second;

   if (eAct == MM::AfterSet)
   {
      long val;
      pProp->Get(val);
		int ret = lum_setIntensity(engine, channelIdx, val);
      if (ret != LUM_OK)
			return RetrieveError();
   }
   if (eAct == MM::BeforeGet)
   {
      int inten;
      int ret = lum_getIntensity(engine, channelIdx, &inten);
      if (ret != DEVICE_OK)
         RetrieveError();

      pProp->Set((long)inten);
   }
   return DEVICE_OK;
}


// Get error from light engine
int CTTLSwitch::RetrieveError()
{
	const int maxLength(1024);
	int errorCode;
	char errorText[maxLength];
	lum_getLastErrorCode(engine, &errorCode);
	lum_getLastErrorText(engine, errorText, maxLength);

	ostringstream os;
	os << "Error : " << errorCode << ", " << errorText << endl;
	SetErrorText(errorCode, os.str().c_str());

	return errorCode;
}

// turns channels off but does not record change in channel state cache: channelStates
// used by the shutter emulator to implement closed shutter state
int CTTLSwitch::TurnAllOff()
{
	vector<lum_bool> states;
	for (size_t i=0; i<channels.size(); i++) states.push_back(false);
	int ret = lum_setMultipleChannels(engine, &states[0], (int)channels.size());
	if (ret != LUM_OK)
		return RetrieveError();

	// we do not update channel state cache because this is a virtual shutter operation 
	shutterState = false; // signals that shutter is now closed
	return DEVICE_OK;
}

// set all intensities to 0
int CTTLSwitch::ZeroAll()
{
	vector<int> ints;
	for (size_t i=0; i<channels.size(); i++) ints.push_back(0);
	int ret = lum_setMultipleIntensities(engine, &ints[0], (int)channels.size());
	if (ret != LUM_OK)
		return RetrieveError();
	return DEVICE_OK;
}

// simulates "open shutter" command by appling states that were in effect when shutter was closed
int CTTLSwitch::ApplyStates()
{
	vector<lum_bool> states;
	for (size_t i=0; i<channels.size(); i++) states.push_back(channelStates[i]);
	int ret = lum_setMultipleChannels(engine, &states[0], (int)channels.size());
	if (ret != LUM_OK)
		return RetrieveError();
	shutterState = true; // signals that shutter is now open

	return DEVICE_OK;
}
