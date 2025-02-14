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
	// connect
	

	int ret = dover.initialize(zStage);
	if (ret != DOVER_OK)
	{
		// if init did not work, reverse the previous steps
		dover.destroy_z_stage(g_apiInstance);

		g_doverInstanceCounter--;
		g_doverInstanceCounter = max(0, g_doverInstanceCounter);

		// last instance releases the API
		if (g_doverInstanceCounter == 0)
		{
			ret = dover.destroy_api_instance(g_apiInstance);
			if (ret != DOVER_OK)
				LogMessage("Error destroying DoverAPI instance.");

			g_apiInstance = nullptr;
		}

		return ret;
	}

	auto pAct = new CPropertyAction(this, &CDoverStage::OnPosition);
	CreateProperty(MM::g_Keyword_Position, "0", MM::Float, false, pAct);
	double low, high;
	GetLimits(low, high);
	SetPropertyLimits(MM::g_Keyword_Position, low, high);

	pAct = new CPropertyAction(this, &CDoverStage::OnMoveDistancePerPulse);
	CreateProperty(g_Prop_MoveDistancePerPulse, "0.0", MM::Float, false, pAct);
	SetPropertyLimits(g_Prop_MoveDistancePerPulse, 0.0, 2.0); // safety limit to 2 um

	pAct = new CPropertyAction(this, &CDoverStage::OnActive);
	CreateProperty(g_Prop_Active, "1", MM::Integer, false, pAct);
	SetPropertyLimits(g_Prop_Active, 0, 1);
	g_active = true;

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
