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
#include <sstream>

std::vector<std::string> split(const std::string& str, char delimiter) {
	std::vector<std::string> tokens;
	std::stringstream ss(str);
	std::string token;

	while (std::getline(ss, token, delimiter)) {
		tokens.push_back(token);
	}

	return tokens;
}

CWDIStage::CWDIStage() : initialized(false), currentStepPosition(0), tracking(false), laserEnable(false)
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

	// create child stage property
	pAct = new CPropertyAction(this, &CWDIStage::OnServiceStageLabel);
	CreateProperty(g_Prop_ServiceStageLabel, "", MM::String, false, pAct,  true);

	// create af controller property
	pAct = new CPropertyAction(this, &CWDIStage::OnServiceControllerLabel);
	CreateProperty(g_Prop_ServiceControllerLabel, "", MM::String, false, pAct, true);

	stepSizeUm = 0.1;
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

	auto pAct = new CPropertyAction(this, &CWDIStage::OnPosition);
	CreateProperty(MM::g_Keyword_Position, "0", MM::Float, false, pAct);
	double low, high;
	GetLimits(low, high);
	SetPropertyLimits(MM::g_Keyword_Position, low, high);

	// NOTE: we are assuming the stage is homed at this point
	currentStepPosition = 0;

	pAct = new CPropertyAction(this, &CWDIStage::OnTrack);
	CreateProperty(g_Prop_Tracking, "0", MM::Integer, false, pAct);
	SetPropertyLimits(g_Prop_Tracking, 0, 1);

	pAct = new CPropertyAction(this, &CWDIStage::OnLaser);
	CreateProperty(g_Prop_Laser, "0", MM::Integer, false, pAct);
	SetPropertyLimits(g_Prop_Laser, 0, 1);

	pAct = new CPropertyAction(this, &CWDIStage::OnMakeZero);
	CreateProperty(g_Prop_MakeZero, "0", MM::Integer, false, pAct);
	SetPropertyLimits(g_Prop_MakeZero, 0, 1);

	pAct = new CPropertyAction(this, &CWDIStage::OnStepSizeUm);
	CreateProperty(g_Prop_StepSizeUm, "0.1", MM::Float, false, pAct);
	SetPropertyLimits(g_Prop_StepSizeUm, 0.01, 0.5);

	// set initial values
	ret = ATF_DisableLaser();
	if (ret != AfStatusOK)
		return ret;
	laserEnable = false;

	ret = ATF_AfStop();
	if (ret != AfStatusOK)
		return ret;
	tracking = false;

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
	int steps = (int)std::round(pos / stepSizeUm);
	return SetPositionSteps(steps);
}

int CWDIStage::SetRelativePositionUm(double deltaPos)
{
	int deltaSteps = (int)std::round(deltaPos / stepSizeUm);

	int ret = ATF_MoveZ(deltaSteps); // relative mode
	if (ret != AfStatusOK)
		return ret;
	//long delayMs = (long)std::round(delayPerUmMs * abs(deltaSteps) * stepSizeUm);
	long delayMs = 100;
	Sleep(delayMs);
	currentStepPosition += deltaSteps; // absolute position

	std::ostringstream os;
	os << ">>> Relative move deltaUm=" << deltaPos << ", deltaSteps=" << deltaSteps << ", currentStep=" << currentStepPosition;
	LogMessage(os.str());

	return DEVICE_OK;
}

int CWDIStage::GetPositionUm(double& pos)
{
	long steps;
	GetPositionSteps(steps);
	pos = steps * stepSizeUm;
	return DEVICE_OK;
}

double CWDIStage::GetStepSize()
{
   return stepSizeUm;
}

int CWDIStage::SetPositionSteps(long steps)
{
	int delta = steps - currentStepPosition;
	int ret = ATF_MoveZ(delta); // relative mode
	if (ret != AfStatusOK)
		return ret;
	//long delayMs = (long)std::round(delayPerUmMs * abs(delta) * stepSizeUm);
	long delayMs = 100;
	Sleep(delayMs);
	currentStepPosition += delta; // absolute position

	std::ostringstream os;
	os << ">>> Absolute move steps=" << steps << ", deltaSteps=" << delta << ", currentStep=" << currentStepPosition;
	LogMessage(os.str());

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

int CWDIStage::OnServiceControllerLabel(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(afControllerName.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		pProp->Get(afControllerName);
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

int CWDIStage::OnStepSizeUm(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(stepSizeUm);
	}
	else if (eAct == MM::AfterSet)
	{
		pProp->Get(stepSizeUm);
	}

	return DEVICE_OK;
}

int CWDIStage::OnLaser(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(laserEnable ? 1L : 0);
	}
	else if (eAct == MM::AfterSet)
	{
		long val;
		pProp->Get(val);
		if (val == 1)
		{
			int ret = ATF_EnableLaser();
			if (ret != AfStatusOK)
				return ret;
			laserEnable = true;
		}
		else
		{
			int ret = ATF_DisableLaser();
			if (ret != AfStatusOK)
				return ret;
			laserEnable = false;
		}
	}

	return DEVICE_OK;
}

int CWDIStage::OnAutoFocus(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		bool state;
		int ret = GetEnableAF(state);
		if (ret != DEVICE_OK)
			return ret;
		pProp->Set(state ? 1L : 0);
	}
	else if (eAct == MM::AfterSet)
	{
		long val;
		pProp->Get(val);
		if (val == 1)
		{
			return EnableAF(true);
		}
		else
		{
			return EnableAF(false);
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

MM::Device* CWDIStage::GetServiceController()
{
	MM::Device* dev = GetCoreCallback()->GetDevice(this, afControllerName.c_str());
	return dev;
}

int CWDIStage::EnableAF(bool state)
{
	auto controller = GetServiceController();
	if (!controller)
		return ERR_WDI_AF_CONTROLLER;

	if (!controller->HasProperty(g_Prop_EnableAF))
		return ERR_WDI_AF_ENABLE;

	return controller->SetProperty(g_Prop_EnableAF, state ? "1" : "0");
}

int CWDIStage::GetEnableAF(bool& state)
{
	auto controller = GetServiceController();
	if (!controller)
		return ERR_WDI_AF_CONTROLLER;

	if (!controller->HasProperty(g_Prop_EnableAF))
		return ERR_WDI_AF_ENABLE;

	char propVal[MM::MaxStrLength];
	propVal[0] = 0;
	int ret = controller->GetProperty(g_Prop_EnableAF, propVal);
	if (ret != DEVICE_OK)
		return ret;

	if (propVal[0] == '1')
		state = true;
	else
		state = false;

	return DEVICE_OK;
}
