///////////////////////////////////////////////////////////////////////////////
// FILE:          WDIStage.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   WDI AF device hw focus defined as a Z-stage
//						Requires Dover DOF stage
//
// AUTHOR:        Nenad Amodaj
//
// COPYRIGHT:     Lumencor 2025         
//
//
#include "WDI.h"
#include "atf_lib_exp.h"

// distance per pulse. TODO: this should be a property
const double g_umPerStep(0.1);


std::vector<std::string> split(const std::string& str, char delimiter) {
	std::vector<std::string> tokens;
	std::stringstream ss(str);
	std::string token;

	while (std::getline(ss, token, delimiter)) {
		tokens.push_back(token);
	}

	return tokens;
}

CWDIStage::CWDIStage() : initialized(false), currentStepPosition(0), tracking(false)
{
	SetErrorText(ERR_WDI_CMD_FAILED, "Command failed. See log file for more info.");

	// Name                                                                   
	CreateProperty(MM::g_Keyword_Name, g_WDIStage, MM::String, true);
	//
	// Description                                                            
	CreateProperty(MM::g_Keyword_Description, "WDI-DOF5 Z stage", MM::String, true);

	// connection                                                                   
	CPropertyAction* pAct = new CPropertyAction(this, &CWDIStage::OnConnection);
	CreateProperty(g_Prop_Connection, "", MM::String, false, pAct, true);

	stepSizeUm = 0.01; // this must be set up in the service stage
}

CWDIStage::~CWDIStage()
{
   Shutdown();
}

bool CWDIStage::Busy()
{
   // TODO: verify this
   return false;
}

void CWDIStage::GetName(char* pszName) const
{
   CDeviceUtils::CopyLimitedString(pszName, g_WDIStage);
}

int CWDIStage::Initialize()
{
	// create child stage property
	auto pAct = new CPropertyAction(this, &CWDIStage::OnServiceStageLabel);
	CreateProperty(g_Prop_ServiceStageLabel, "", MM::String, false, pAct);

	// connect
	BOOL bret = ATF_openLogFile("atf_test.log", "w");
	if (!bret)
	{
		LogMessage("Failed opening WDI log file.");
		return ERR_WDI_CMD_FAILED;
	}

	ATF_setLogLevel(3);     // 0 - nothing, 3 maximium

	auto tokens = split(connection, ':'); // expecting ip:port string
	if (tokens.size() != 2)
	{
		LogMessage("Invalid connection string.");
		return ERR_WDI_INVALID_CONNECTION;
	}

	std::ostringstream os;
	os << "Connecting to " << tokens[0] << ":" << tokens[1] << "..." << std::endl;
	int ecode = ATF_OpenConnection((char*)tokens[0].c_str(), atoi(tokens[1].c_str()));
	if (ecode != 0)
	{
		return ERR_WDI_INVALID_CONNECTION;
	}

	unsigned int sensorSN(0);
	int ret = ATF_ReadSerialNumber(&sensorSN);
	if (ret != AfStatusOK)
	{
		return ERR_WDI_CMD_FAILED;
	}

	int ver(0);
	ret = ATF_ReadFirmwareVer(&ver);
	if (ret != AfStatusOK)
	{
		return ERR_WDI_CMD_FAILED;
	}

	CreateProperty(g_Prop_Firmware, std::to_string(ver).c_str(), MM::Integer, true);
	CreateProperty(g_Prop_SN, std::to_string(sensorSN).c_str(), MM::Integer, true);

	pAct = new CPropertyAction(this, &CWDIStage::OnPosition);
	CreateProperty(MM::g_Keyword_Position, "0", MM::Float, false, pAct);
	double low, high;
	GetLimits(low, high);
	SetPropertyLimits(MM::g_Keyword_Position, low, high);

	// NOTE: we are assuming the stage is homed at this point
	currentStepPosition = 0;

	pAct = new CPropertyAction(this, &CWDIStage::OnTrack);
	CreateProperty(g_Prop_Tracking, "0", MM::Integer, false, pAct);

	pAct = new CPropertyAction(this, &CWDIStage::OnMakeZero);
	CreateProperty(g_Prop_MakeZero, "0", MM::Integer, false, pAct);

	UpdateStatus();
	initialized = true;

	return DEVICE_OK;
}

int CWDIStage::Shutdown()
{
	ATF_CloseConnection();
	ATF_closeLogFile();
   return DEVICE_OK;
}

int CWDIStage::Home()
{
   return DEVICE_UNSUPPORTED_COMMAND;
}

int CWDIStage::SetPositionUm(double pos)
{
	int steps = (int)std::round(pos / g_umPerStep);
	return SetPositionSteps(steps);
}

int CWDIStage::GetPositionUm(double& pos)
{
	long steps;
	GetPositionSteps(steps);
	pos = steps * g_umPerStep;
	return DEVICE_OK;
}

double CWDIStage::GetStepSize()
{
   return g_umPerStep;
}

int CWDIStage::SetPositionSteps(long steps)
{
	int ret = ATF_MoveZ(steps);
	if (ret != AfStatusOK)
		return ret;
	currentStepPosition = steps;

   return DEVICE_OK;
}

int CWDIStage::GetPositionSteps(long& steps)
{
   steps = currentStepPosition;
	return DEVICE_OK;
}

int CWDIStage::GetLimits(double& lower, double& upper)
{
	auto stage = GetServiceStage();
	if (!stage)
		return ERR_WDI_SERVICE_STAGE;
	int ret = stage->GetLimits(lower, upper);
	if (ret != DEVICE_OK)
		return ret;

   return DEVICE_OK;
}

int CWDIStage::OnPosition(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		double posUm;
		int ret = GetPositionUm(posUm);
		if (ret)
			return ret;
		pProp->Set(posUm);
	}
	else if (eAct == MM::AfterSet)
	{
		double pos;
		pProp->Get(pos);
		return SetPositionUm(pos);
	}

	return DEVICE_OK;
}

int CWDIStage::OnConnection(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(connection.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		pProp->Get(connection);
	}

	return DEVICE_OK;
}

int CWDIStage::OnServiceStageLabel(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(dofStageName.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		pProp->Get(dofStageName);
	}

	return DEVICE_OK;
}

int CWDIStage::OnMakeZero(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(0L);
	}
	else if (eAct == MM::AfterSet)
	{
		long val;
		pProp->Get(val);
		if (val == 1)
		{
			int ret = ATF_Make0();
			if (ret != AfStatusOK)
				return ret;
		}
	}

	return DEVICE_OK;
}

int CWDIStage::OnTrack(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(tracking ? 1L : 0L);
	}
	else if (eAct == MM::AfterSet)
	{
		long val;
		pProp->Get(val);
		if (val == 1)
		{
			int ret = ATF_AFTrack();
			// start AF tracking first, once at focus continue on AOI tracking
			if (ret != AfStatusOK)
				return ret;
			tracking = true;
		}
		else
		{
			int ret = ATF_AfStop();
			if (ret != AfStatusOK)
				return ret;
			tracking = false;
		}
	}
	return DEVICE_OK;
}

MM::Stage* CWDIStage::GetServiceStage()
{
	MM::Device* dev = GetCoreCallback()->GetDevice(this, dofStageName.c_str());
	if (!dev)
		return nullptr;
	MM::Stage* stage = dynamic_cast<MM::Stage*>(dev);
	return stage;
}
