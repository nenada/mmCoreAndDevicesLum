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

std::vector<std::string> split(const std::string& str, char delimiter) {
	std::vector<std::string> tokens;
	std::stringstream ss(str);
	std::string token;

	while (std::getline(ss, token, delimiter)) {
		tokens.push_back(token);
	}

	return tokens;
}

CWDIStage::CWDIStage() : initialized(false)
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

}

CWDIStage::~CWDIStage()
{
   Shutdown();
}

bool CWDIStage::Busy()
{
   // TODO:
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

	UpdateStatus();
	initialized = true;

	return DEVICE_OK;
}

int CWDIStage::Shutdown()
{
   return 0;
}

int CWDIStage::Home()
{
   return 0;
}

int CWDIStage::SetPositionUm(double pos)
{
   return 0;
}

int CWDIStage::GetPositionUm(double& pos)
{
   return 0;
}

double CWDIStage::GetStepSize()
{
   return 0.0;
}

int CWDIStage::SetPositionSteps(long steps)
{
   return 0;
}

int CWDIStage::GetPositionSteps(long& steps)
{
   return 0;
}

int CWDIStage::GetLimits(double& lower, double& upper)
{
   return 0;
}

int CWDIStage::OnPosition(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return 0;
}

int CWDIStage::OnConnection(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return 0;
}
