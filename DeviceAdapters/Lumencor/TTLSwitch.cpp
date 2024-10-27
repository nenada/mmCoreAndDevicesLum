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
const double ttlAnswerDelayMs = 50.0;
const map<string, int> ttlMap =
{
	{"RED", 0},
	{"GREEN", 1},
	{"CYAN", 2},
	{"SHUTTER", 3},
	{"TEAL", 4},
	{"BLUE", 5},
	{"VIOLET", 6},
	{"NIR", 7},
	{"YELLOW", 8},
	{"SPR1", 9}
};

CTTLSwitch::CTTLSwitch() :
   initialized(false),
	engine(0),
	currentChannel(0)
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
	CreateProperty(g_Prop_ModuleVersion, LUMENCOR_DEV_VERSION, MM::String, true);

	// light engine connection                                                                 
	auto pAct = new CPropertyAction(this, &CTTLSwitch::OnConnection);
	CreateProperty(g_Prop_Connection, "", MM::String, false, pAct, true);

	// arduino port
	pAct = new CPropertyAction(this, &CTTLSwitch::OnPort);
	CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);

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

	// State
	// -----
	auto pAct = new CPropertyAction(this, &CTTLSwitch::OnState);
	ret = CreateProperty(MM::g_Keyword_State, "0", MM::Integer, false, pAct);
	if (ret != DEVICE_OK)
		return ret;

	pAct = new CPropertyAction(this, &CTTLSwitch::OnLabel);
	ret = CreateStringProperty(MM::g_Keyword_Label, channels[0].c_str(), false, pAct);
	if (ret != DEVICE_OK)
		return ret;
	currentChannel = 0;

	channelLookup.clear();
	for (size_t i=0; i<channels.size(); i++)
	{
		ostringstream os;
		os << channels[i] << "_" << "Intensity";
	   pAct = new CPropertyAction(this, &CTTLSwitch::OnChannelIntensity);
		CreateProperty(os.str().c_str(), "0", MM::Integer, false, pAct);
		SetPropertyLimits(os.str().c_str(), 0, maxIntensity);

		channelLookup[channels[i]].channelId = (int)i;
		channelLookup[channels[i]].name = channels[i];
		auto it = ttlMap.find(channels[i]);
		if (it != ttlMap.end())
			channelLookup[channels[i]].ttlId = it->second;
		else
			return ERR_TTL_CHANNEL_NAME; // unable to identify the TTL index

		ostringstream osexp;
		osexp << channels[i] << "_" << "ExposureMs";
		pAct = new CPropertyAction(this, &CTTLSwitch::OnChannelExposure);
		CreateProperty(osexp.str().c_str(), "5.0", MM::Float, false, pAct);
		SetPropertyLimits(osexp.str().c_str(), 0.0, 100.0);  // limit to 100 ms
		
		AddAllowedValue(MM::g_Keyword_Label, channels[i].c_str());
	}

   // reset light engine
   // ------------------
	ret = ZeroAll();
	if (ret != DEVICE_OK)
		return ret;

	ret = TurnAllOff();
	if (ret != DEVICE_OK)
		return ret;
            
	// get TTL control info
	ret = SendSerialCommand(ttlPort.c_str(), "VER", "\r");
	if (ret != DEVICE_OK)
	{
		LogMessage("Unable to connect to the TTL controller.");
		return ret;
	}
	::Sleep(500);
	string answer;
	ret = GetSerialAnswer(ttlPort.c_str(), "\r", answer);
	if (ret != DEVICE_OK)
	{
		LogMessage("No response from the TTL controller.");
		return ret;
	}
	::Sleep(500);

	ret = CreateProperty("TTLVersion", answer.c_str(), MM::String, true);
	if (ret != DEVICE_OK)
		return ret;

	// initially set the first channel
	ret = SetTTLController(channelLookup[channels[0]], 100.0);
	if (ret != DEVICE_OK)
		return ret;

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


int CTTLSwitch::SetTTLController(const ChannelInfo& inf, double delayMs)
{
	int ttlId = inf.ttlId;
	int exposureUs = (int)nearbyint(inf.exposureMs * 1000);

	ostringstream os;
	os << "SQ " << ttlId << " " << exposureUs;
	int ret = SendSerialCommand(ttlPort.c_str(), os.str().c_str(), "\r");
	LogMessage("Sent SQ command: " + os.str());
	if (ret != DEVICE_OK)
	{
		LogMessage("Failed to send SQ command to " + ttlPort);
		return ret;
	}

	::Sleep((DWORD)(delayMs + 0.5));
	string answer;
	ret = GetSerialAnswer(ttlPort.c_str(), "\r", answer);
	LogMessage("Received SQ answer: " + answer);
	if (ret != DEVICE_OK)
	{
		LogMessage("Failed to get answer from SQ command from " + ttlPort);
		return ret;
	}
	answer.erase(std::remove(answer.begin(), answer.end(), '\n'), answer.end());
	if (answer.size() == 0 || answer.at(0) != 'A')
	{
		LogMessage("SQ command failed: " + answer);
		return ERR_TTL_COMMAND_FAILED;
	}

	return DEVICE_OK;
}

/**
 * Sends sequence information to Arduino
 * @param sequence - channel index sequence
 */
int CTTLSwitch::LoadChannelSequence(const std::vector<int>& sequence)
{
	ostringstream os;
	os << "SQ ";
	for (size_t i = 0; i < sequence.size(); i++)
	{
		auto cInfo = channelLookup[channels[i]];
		os << cInfo.channelId << " " << (int)nearbyint(cInfo.exposureMs * 1000);
		if (i < sequence.size() - 1)
			os << " ";
	}
	int ret = SendSerialCommand(ttlPort.c_str(), os.str().c_str(), "\r");
	LogMessage("Sent channel sequence SQ command: " + os.str());

	string answer;
	ret = GetSerialAnswer(ttlPort.c_str(), "\r", answer);
	LogMessage("Received SQ answer: " + answer);
	if (ret != DEVICE_OK)
	{
		LogMessage("Failed to get answer from SQ command from " + ttlPort);
		return ret;
	}
	answer.erase(std::remove(answer.begin(), answer.end(), '\n'), answer.end());
	if (answer.size() == 0 || answer.at(0) != 'A')
	{
		LogMessage("SQ command failed: " + answer);
		return ERR_TTL_COMMAND_FAILED;
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

// set all intensities to 0
int CTTLSwitch::ZeroAll()
{
	vector<int> ints;
	for (size_t i = 0; i < channels.size(); i++) ints.push_back(0);
	int ret = lum_setMultipleIntensities(engine, &ints[0], (int)channels.size());
	if (ret != LUM_OK)
		return RetrieveError();
	return DEVICE_OK;
}

// turns channels off but does not record change in channel state cache: channelStates
// used by the shutter emulator to implement closed shutter state
int CTTLSwitch::TurnAllOff()
{
	vector<lum_bool> states;
	for (size_t i = 0; i < channels.size(); i++) states.push_back(false);
	int ret = lum_setMultipleChannels(engine, &states[0], (int)channels.size());
	if (ret != LUM_OK)
		return RetrieveError();

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
		pProp->Set((long)currentChannel);
   }
   else if (eAct == MM::AfterSet)
   {
		// get channel
		long channelIndex;
		pProp->Get(channelIndex);
		if (channelIndex >= channels.size() || channelIndex < 0)
			return DEVICE_INVALID_PROPERTY_VALUE;

		auto it = channelLookup.find(channels[channelIndex]);
		if (it == channelLookup.end())
			return ERR_TTL_CHANNEL_NAME;

		int ret = SetTTLController(it->second, ttlAnswerDelayMs);
		if (ret != DEVICE_OK)
			return ret;

		currentChannel = channelIndex;
   }
	else if (eAct == MM::IsSequenceable)
	{
		LogMessage("MM::IsSequenceable event.");
		// pProp->SetSequenceable((long)channels.size());
		// disable sequencing for this property
		pProp->SetSequenceable(0);
	}
	else if (eAct == MM::AfterLoadSequence)
	{
		LogMessage("MM::AfterLoadSequence event.");
		std::vector<std::string> sequence = pProp->GetSequence();
		std::ostringstream os;
		if (sequence.size() > channels.size())
			return DEVICE_SEQUENCE_TOO_LARGE;

		std::vector<int> chSequence;
		try
		{
			for (auto& s : sequence)
			{
				chSequence.push_back(std::stoi(s));
				if (chSequence.back() >= channels.size())
					return ERR_TTL_INVALID_SEQUENCE;
			}
		}
		catch (...)
		{
			return ERR_TTL_INVALID_SEQUENCE;
		}
		int ret = LoadChannelSequence(chSequence);
		if (ret != DEVICE_OK)
			return ret;
	}
	else if (eAct == MM::StartSequence)
	{
		LogMessage("MM::StartSequence event. No action taken.");
	}
	else if (eAct == MM::StopSequence)
	{
		LogMessage("MM::StopSequence event. No action taken.");
	}

   return DEVICE_OK;
}

int CTTLSwitch::OnLabel(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(channels[currentChannel].c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		// get channel
		string channelLabel;
		pProp->Get(channelLabel);

		auto it = channelLookup.find(channelLabel);
		if (it == channelLookup.end())
			return ERR_TTL_CHANNEL_NAME;

		int ret = SetTTLController(it->second, ttlAnswerDelayMs);
		if (ret != DEVICE_OK)
			return ret;

		currentChannel = it->second.channelId;
	}

	return DEVICE_OK;
}

int CTTLSwitch::OnSequence(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	// TODO: this might not be needed
	return DEVICE_NOT_YET_IMPLEMENTED;
}

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
	auto it = channelLookup.find(channel);
	if (it == channelLookup.end())
	{
		ostringstream os;
		os << "Invalid channel name: " << channel;
		LogMessage(os.str());
		return ERR_INTERNAL;
	}

	int channelIdx = it->second.channelId;

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

int CTTLSwitch::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(ttlPort.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		pProp->Get(ttlPort);
	}
	return DEVICE_OK;
}

int CTTLSwitch::OnChannelExposure(MM::PropertyBase* pProp, MM::ActionType eAct)
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
	auto it = channelLookup.find(channel);
	if (it == channelLookup.end())
	{
		ostringstream os;
		os << "Invalid channel name: " << channel;
		LogMessage(os.str());
		return ERR_INTERNAL;
	}

	if (eAct == MM::AfterSet)
	{
		double val;
		pProp->Get(val);
		it->second.exposureMs = val;
	}
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(it->second.exposureMs);
	}

	return DEVICE_OK;
}

