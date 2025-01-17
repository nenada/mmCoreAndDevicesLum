///////////////////////////////////////////////////////////////////////////////
// FILE:          DoverStages.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Adapter for Dover stages
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

#include "Dover.h"
#include "DoverAPI.h"
#include "DeviceBase.h"

static int g_doverInstanceCounter(0);
void* g_apiInstance = nullptr;
DoverFunctions dover;
HMODULE hDLL = nullptr;
const double g_umPerStep(0.005); // TODO: this should be picked up from the Dover configuration file
bool g_active = false;

int loadDoverDLL()
{
	if (hDLL == nullptr)
	{
		hDLL = LoadLibrary(TEXT("DoverAPI.dll"));
		if (!hDLL) {
			return ERR_DOVER_DLL_LOAD;
		}
	}

	if (!dover.LoadFunctions(hDLL)) {
		return ERR_DOVER_DLL_FUNCTION_LOAD;
	}

	return DOVER_OK;
}

CDoverStage::CDoverStage() : initialized(false), zStage(nullptr)
{
	if (!hDLL)
	{
		int ret = loadDoverDLL();
		if (ret != DOVER_OK)
		{
			LogMessage("Dover DLL load error: " + ret);
			return;
		}
	}

	CreateProperty(MM::g_Keyword_Description, "Dover DOF5 Z stage", MM::String, true);
	char versionStr[MM::MaxStrLength];
	int ret = dover.get_version(versionStr, MM::MaxStrLength);
	if (ret == DOVER_OK)
		CreateProperty(g_Prop_ModuleVersion, versionStr, MM::String, true);
}

CDoverStage::~CDoverStage()
{
	Shutdown();
}

bool CDoverStage::Busy()
{
	try
	{
		if (g_active)
			return dover.is_busy(zStage) != 0;
		else
			return false;
	}
	catch (std::exception& e)
	{
		LogMessage(e.what());
		return false;
	}
}

void CDoverStage::GetName(char* pszName) const
{
	CDeviceUtils::CopyLimitedString(pszName, g_DoverStage);
}

int CDoverStage::Initialize()
{
	// create api instance if it does not already exist
	if (g_apiInstance == nullptr)
	{
		int ret = dover.create_api_instance(&g_apiInstance);
		if (ret != DOVER_OK)
			LogMessage("Error creating DoverAPI instance.");
		g_doverInstanceCounter = 0;
	}

	// create z stage
	if (g_apiInstance)
	{
		int ret = dover.create_z_stage(g_apiInstance, &zStage);
		if (ret != DOVER_OK)
		{
			LogMessage("Error creating Dover Z stage instance.");
			return ERR_DOVER_INITIALIZE;
		}
		g_doverInstanceCounter++;
	}
	else
		return ERR_DOVER_API_INSTANCE;

	int ret = dover.initialize(zStage);
	if (ret != DOVER_OK)
		return ret;

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

int CDoverStage::Shutdown()
{
	if (!initialized)
		return DEVICE_OK;

	int ret = dover.destroy_z_stage(zStage);
	if (ret != DOVER_OK)
		LogMessage("Error destroying Dover Z stage instance.");
	zStage = nullptr;

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

	initialized = false;
	g_active = false;
	return DEVICE_OK;
}

int CDoverStage::Home()
{
	if (!g_active)
		return ERR_DOVER_SUSPENDED;

	int ret = dover.home(zStage);
	if (ret != DOVER_OK)
		return ERR_DOVER_HOME_FAILED;
	return DEVICE_OK;
}

int CDoverStage::SetPositionUm(double pos)
{
	if (!g_active)
		return ERR_DOVER_SUSPENDED;

	double low, high;
	GetLimits(low, high);
	if (pos >= high || pos <= low)
		return ERR_DOVER_LIMITS_EXCEEDED;

	try
	{
		dover.set_position(zStage, 0, pos / 1000.0);
	}
	catch (std::exception& e)
	{
		LogMessage(e.what());
		return ERR_DOVER_CMD_FAILED;
	}
	return DEVICE_OK;
}

int CDoverStage::GetPositionUm(double& pos)
{
	if (g_active)
	{
		double doverPos;
		int ret = dover.get_position(zStage, 0, &doverPos);
		if (ret != DOVER_OK)
			return ret;
		pos = doverPos * 1000.0;
	}
	else
	{
		pos = 0.0;
	}

	return DEVICE_OK;
}

double CDoverStage::GetStepSize()
{
	return g_umPerStep;
}

int CDoverStage::SetPositionSteps(long steps)
{
	if (!g_active)
		return ERR_DOVER_SUSPENDED;

	double posUm = steps * g_umPerStep;
	int ret = dover.set_position(zStage, 0, posUm / 1000.0);
	if (ret != DOVER_OK)
		return ret;
	
	return DEVICE_OK;
}

int CDoverStage::GetPositionSteps(long& steps)
{
	if (g_active)
	{
		double doverPos;
		int ret = dover.get_position(zStage, 0, &doverPos);
		if (ret != DOVER_OK)
			return ret;

		auto posUm = doverPos * 1000.0;
		steps = (long)(posUm / g_umPerStep + 0.5);
	}
	else
		steps = 0L;

	return DEVICE_OK;
}

int CDoverStage::GetLimits(double& lower, double& upper)
{
	// TODO: read from configuration
	lower = -2500.0;
	upper = 2500.0;
	return DEVICE_OK;
}

int CDoverStage::OnPosition(MM::PropertyBase* pProp, MM::ActionType eAct)
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
		if (!g_active)
			return ERR_DOVER_SUSPENDED;

		double pos;
		pProp->Get(pos);
		return SetPositionUm(pos);
	}

	return DEVICE_OK;
}

int CDoverStage::OnMoveDistancePerPulse(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		if (g_active)
		{
			double stepUm(0.0);
			try
			{
				const bool forceRefresh(true);
				dover.get_external_control(zStage, forceRefresh, &stepUm);
				stepUm *= 1000; // convert from mm to um
			}
			catch (std::exception& e)
			{
				LogMessage(e.what());
				return ERR_DOVER_CMD_FAILED;
			}

			pProp->Set(stepUm);
		}
		else
			pProp->Set(0.0);
	}
	else if (eAct == MM::AfterSet)
	{
		if (!g_active)
			return ERR_DOVER_SUSPENDED;

		double stepUm;
		pProp->Get(stepUm);
		try
		{
			dover.set_external_control(zStage, stepUm / 1000.0);
		}
		catch (std::exception& e)
		{
			LogMessage(e.what());
			return ERR_DOVER_CMD_FAILED;
		}
	}

	return DEVICE_OK;
}

int CDoverStage::OnActive(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(g_active ? 1L : 0L);
	}
	else if (eAct == MM::AfterSet)
	{
		long val;
		pProp->Get(val);
		if (val)
		{
			int ret = Initialize();
			if (ret != DEVICE_OK)
				return ret;
		}
		else
		{
			int ret = Shutdown();
			if (ret != DEVICE_OK)
				return ret;
		}
	}

	return DEVICE_OK;

}

////////////////////////////////////////////////////////////////////////////////////////
// Dover XY Stage
//
CDoverXYStage::CDoverXYStage() : initialized(false), xyStage(nullptr)
{
	if (!hDLL)
	{
		int ret = loadDoverDLL();
		if (ret != DOVER_OK)
		{
			LogMessage("Dover DLL load error: " + ret);
			return;
		}
	}

	CreateProperty(MM::g_Keyword_Description, "Dover XY stage", MM::String, true);
	char versionStr[MM::MaxStrLength];
	int ret = dover.get_version(versionStr, MM::MaxStrLength);
	if (ret == DOVER_OK)
		CreateProperty(g_Prop_ModuleVersion, versionStr, MM::String, true);
}

CDoverXYStage::~CDoverXYStage()
{
	Shutdown();
}

bool CDoverXYStage::Busy()
{
	return dover.is_busy(xyStage) != 0;
}

void CDoverXYStage::GetName(char* pszName) const
{
	CDeviceUtils::CopyLimitedString(pszName, g_DoverXYStage);
}

int CDoverXYStage::Initialize()
{
	// create api instance if it does not already exist
	if (g_apiInstance == nullptr)
	{
		int ret = dover.create_api_instance(&g_apiInstance);
		if (ret != DOVER_OK)
			LogMessage("Error creating DoverAPI instance.");
		g_doverInstanceCounter = 0;
	}

	if (g_apiInstance == nullptr)
	{
		int ret = dover.create_api_instance(&g_apiInstance);
		if (ret != DOVER_OK)
			LogMessage("Error creating DoverAPI instance.");
		g_doverInstanceCounter = 0;
	}

	// create xy stage
	if (g_apiInstance)
	{
		int ret = dover.create_xy_stage(g_apiInstance, &xyStage);
		if (ret != DOVER_OK)
			LogMessage("Error creating Dover XY stage instance.");
		g_doverInstanceCounter++;
	}
	else
		return ERR_DOVER_API_INSTANCE;

	if (!xyStage)
		return ERR_DOVER_INITIALIZE;

	int ret = dover.initialize(xyStage);
	if (ret != DOVER_OK)
		return ret;

	// TODO: define property for trigger value
	ret = dover.xy_set_digital_trigger(xyStage, 1); // corresponds to "InMotion"
	if (ret != DOVER_OK)
		return ret;

	double minX, minY, maxX, maxY;
	GetLimitsUm(minX, maxX, minY, maxY);
	auto pAct = new CPropertyAction(this, &CDoverXYStage::OnPositionX);
	CreateProperty(g_Prop_DoverX, "0.0", MM::Float, false, pAct);
	SetPropertyLimits(g_Prop_DoverX, minX, maxX);

	pAct = new CPropertyAction(this, &CDoverXYStage::OnPositionY);
	CreateProperty(g_Prop_DoverY, "0.0", MM::Float, false, pAct);
	SetPropertyLimits(g_Prop_DoverY, minY, maxY);

	pAct = new CPropertyAction(this, &CDoverXYStage::OnMoveDistancePerPulse);
	CreateProperty(g_Prop_MoveDistancePerPulse, "0.0", MM::Float, false, pAct);
	SetPropertyLimits(g_Prop_MoveDistancePerPulse, 0.0, 2.0); // safety limit to 2 um

	pAct = new CPropertyAction(this, &CDoverXYStage::OnActive);
	CreateProperty(g_Prop_Active, "1", MM::Integer, false, pAct);
	SetPropertyLimits(g_Prop_Active, 0, 1);
	g_active = true;

	UpdateStatus();
	initialized = true;

	return DEVICE_OK;
}

int CDoverXYStage::Shutdown()
{
	if (!initialized)
		return DEVICE_OK;

	int ret = dover.destroy_xy_stage(xyStage);
	if (ret != DOVER_OK)
		LogMessage("Error destroying Dover XY stage instance.");
	xyStage = nullptr;

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

	initialized = false;
	g_active = false;
	return DEVICE_OK;
}

double CDoverXYStage::GetStepSize()
{
	return g_umPerStep;
}

int CDoverXYStage::SetPositionSteps(long x, long y)
{
	double xposUm = x * g_umPerStep;
	double yposUm = y * g_umPerStep;

	double xlow, xhigh, ylow, yhigh;
	GetLimitsUm(xlow, xhigh, ylow, yhigh);


	if (xposUm >= xhigh || xposUm <= xlow || yposUm <= ylow || yposUm >= yhigh)
		return ERR_DOVER_LIMITS_EXCEEDED;

	int ret = dover.set_position(xyStage, 0, xposUm / 1000.0);
	if (ret != DOVER_OK)
		return ret;

	ret = dover.set_position(xyStage, 1, yposUm / 1000.0);
	if (ret != DOVER_OK)
		return ret;

	return DEVICE_OK;
}

int CDoverXYStage::GetPositionSteps(long& x, long& y)
{
	double doverXPos, doverYPos;
	int ret = dover.get_position(xyStage, 0, &doverXPos);
	if (ret != DOVER_OK)
		return ret;
	ret = dover.get_position(xyStage, 1, &doverYPos);
	if (ret != DOVER_OK)
		return ret;

	auto xposUm = doverXPos * 1000.0;
	x = (long)(xposUm / g_umPerStep + 0.5);
	auto yposUm = doverYPos * 1000.0;
	y = (long)(yposUm / g_umPerStep + 0.5);

	return DEVICE_OK;
}

int CDoverXYStage::Home()
{
	int ret = dover.home(xyStage);
	if (ret != DOVER_OK)
		return ret;

	return DEVICE_OK;
}

int CDoverXYStage::Stop()
{
	// TODO: implement
	return DEVICE_UNSUPPORTED_COMMAND;
}

int CDoverXYStage::GetLimitsUm(double& xMin, double& xMax, double& yMin, double& yMax)
{
	// TODO read from config
	xMin = -75000.0;
	xMax = 75000.0;
	yMin = -50000.0;
	yMax = 50000.0;

	return 0;
}

int CDoverXYStage::GetStepLimits(long& xMinS, long& xMaxS, long& yMinS, long& yMaxS)
{
	double xMin, xMax, yMin, yMax;
	GetLimitsUm(xMin, xMax, yMin, yMax);

	xMinS = (long)std::nearbyint(xMin / g_umPerStep);
	xMaxS = (long)std::nearbyint(xMax / g_umPerStep);
	yMinS = (long)std::nearbyint(yMin / g_umPerStep);
	yMaxS = (long)std::nearbyint(yMax / g_umPerStep);

	return DEVICE_OK;
}

double CDoverXYStage::GetStepSizeXUm()
{
	return g_umPerStep;
}

double CDoverXYStage::GetStepSizeYUm()
{
	return g_umPerStep;
}

int CDoverXYStage::OnPositionX(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		double pos;
		int ret = dover.get_position(xyStage, 0, &pos);
		if (ret != DOVER_OK)
			return ret;
		pProp->Set(pos * 1000.0);
	}
	else if (eAct == MM::AfterSet)
	{
		long pos;
		pProp->Get(pos);
		int ret = dover.set_position(xyStage, 0, pos / 1000.0);
		if (ret != DOVER_OK)
			return ret;
	}

	return DEVICE_OK;
}

int CDoverXYStage::OnPositionY(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		double pos;
		int ret = dover.get_position(xyStage, 1, &pos);
		if (ret != DOVER_OK)
			return ret;
		pProp->Set(pos * 1000.0);
	}
	else if (eAct == MM::AfterSet)
	{
		long pos;
		pProp->Get(pos);
		int ret = dover.set_position(xyStage, 1, pos / 1000.0);
		if (ret != DOVER_OK)
			return ret;
	}

	return DEVICE_OK;
}

int CDoverXYStage::OnMoveDistancePerPulse(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		double stepUm(0.0);
		try
		{
			const bool forceRefresh(true);
			dover.get_external_control(xyStage, forceRefresh, &stepUm);
			stepUm *= 1000; // convert from mm to um
		}
		catch (std::exception& e)
		{
			LogMessage(e.what());
			return ERR_DOVER_CMD_FAILED;
		}

		pProp->Set(stepUm);
	}
	else if (eAct == MM::AfterSet)
	{
		double stepUm;
		pProp->Get(stepUm);
		try
		{
			dover.set_external_control(xyStage, stepUm / 1000.0);
		}
		catch (std::exception& e)
		{
			LogMessage(e.what());
			return ERR_DOVER_CMD_FAILED;
		}
	}

	return DEVICE_OK;
}

int CDoverXYStage::OnActive(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(g_active ? 1L : 0L);
	}
	else if (eAct == MM::AfterSet)
	{
		long val;
		pProp->Get(val);
		if (val)
		{
			int ret = Initialize();
			if (ret != DEVICE_OK)
				return ret;
		}
		else
		{
			int ret = Shutdown();
			if (ret != DEVICE_OK)
				return ret;
		}
	}

	return DEVICE_OK;

}


