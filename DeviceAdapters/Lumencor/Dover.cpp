///////////////////////////////////////////////////////////////////////////////
// FILE:          Dover.cpp
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

#include "Lumencor.h"
#include "DoverAPI.h"

static int g_doverInstanceCounter(0);
dover::DoverApi* g_apiInstance = nullptr;
const double g_umPerStep(0.005); // TODO: this should be picked up from the Dover configuration file

CDoverStage::CDoverStage() : initialized(false), zStage(nullptr)
{
	if (g_apiInstance == nullptr)
	{
		g_apiInstance = dover::DoverApi::createInstance();
		g_doverInstanceCounter = 0;
	}
	if (g_apiInstance)
	{
		zStage = dover::DOF5Stage::create(g_apiInstance);
		g_doverInstanceCounter++;
	}
}

CDoverStage::~CDoverStage()
{
	Shutdown();
	dover::DOF5Stage::destroy(zStage);
	g_doverInstanceCounter--;
	g_doverInstanceCounter = max(0, g_doverInstanceCounter);

	// last instance releases the API
	if (g_doverInstanceCounter == 0)
	{
		dover::DoverApi::destroyInstance(g_apiInstance);
		g_apiInstance = nullptr;
	}
}

bool CDoverStage::Busy()
{
	try
	{
		return zStage->IsBusy();
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
	try
	{ 
		zStage->Initialize();
	}
	catch (std::exception& e)
	{
		LogMessage(e.what());
		return DEVICE_NATIVE_MODULE_FAILED;
	}
	auto pAct = new CPropertyAction(this, &CDoverStage::OnPosition);
	CreateProperty(MM::g_Keyword_Position, "0", MM::Float, false, pAct);
	double low, high;
	GetLimits(low, high);
	SetPropertyLimits(MM::g_Keyword_Position, low, high);

	UpdateStatus();
	initialized = true;

	return DEVICE_OK;
}

int CDoverStage::Shutdown()
{
	initialized = false;
	return DEVICE_OK;
}

int CDoverStage::Home()
{
	try
	{
		zStage->Home();
	}
	catch (std::exception& e)
	{
		LogMessage(e.what());
		return ERR_DOVER_HOME_FAILED;
	}
	return DEVICE_OK;
}

int CDoverStage::SetPositionUm(double pos)
{
	double low, high;
	GetLimits(low, high);
	if (pos >= high || pos <= low)
		return ERR_DOVER_LIMITS_EXCEEDED;

	try
	{
		zStage->SetPosition(pos / 1000.0);
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
	try
	{
		pos = zStage->GetPosition() * 1000.0;
	}
	catch (std::exception& e)
	{
		LogMessage(e.what());
		return ERR_DOVER_CMD_FAILED;
	}
	return DEVICE_OK;
}

double CDoverStage::GetStepSize()
{
	return g_umPerStep;
}

int CDoverStage::SetPositionSteps(long steps)
{
	double posUm = steps * g_umPerStep;
	try
	{
		zStage->SetPosition(posUm / 1000.0);
	}
	catch (std::exception& e)
	{
		LogMessage(e.what());
		return ERR_DOVER_CMD_FAILED;
	}
	return DEVICE_OK;
}

int CDoverStage::GetPositionSteps(long& steps)
{
	try
	{
		auto posUm = zStage->GetPosition() * 1000.0;
		steps = (long)(posUm / g_umPerStep + 0.5);
	}
	catch (std::exception& e)
	{
		LogMessage(e.what());
		return ERR_DOVER_CMD_FAILED;
	}
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
		double pos;
		pProp->Get(pos);
		return SetPositionUm(pos);
	}

	return DEVICE_OK;
}


CDoverXYStage::CDoverXYStage() : initialized(false), xyStage(nullptr)
{
	if (g_apiInstance == nullptr)
	{
		g_apiInstance = dover::DoverApi::createInstance();
		g_doverInstanceCounter = 0;
	}
	if (g_apiInstance)
	{
		xyStage = dover::XYStage::create(g_apiInstance);
		g_doverInstanceCounter++;
	}
}

CDoverXYStage::~CDoverXYStage()
{
	Shutdown();
	dover::XYStage::destroy(xyStage);
	g_doverInstanceCounter--;
	g_doverInstanceCounter = max(0, g_doverInstanceCounter);

	// last instance releases the API
	if (g_doverInstanceCounter == 0)
	{
		dover::DoverApi::destroyInstance(g_apiInstance);
		g_apiInstance = nullptr;
	}
}

bool CDoverXYStage::Busy()
{
	try
	{
		return xyStage->IsBusy();
	}
	catch (std::exception& e)
	{
		LogMessage(e.what());
		return false;
	}
}

void CDoverXYStage::GetName(char* pszName) const
{
	CDeviceUtils::CopyLimitedString(pszName, g_DoverXYStage);
}

int CDoverXYStage::Initialize()
{
	try
	{
		xyStage->Initialize();
	}
	catch (std::exception& e)
	{
		LogMessage(e.what());
		return DEVICE_NATIVE_MODULE_FAILED;
	}

	UpdateStatus();
	initialized = true;
	g_doverInstanceCounter++;

	return DEVICE_OK;
}

int CDoverXYStage::Shutdown()
{
	initialized = false;
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

	try
	{
		xyStage->SetPosition(xposUm / 1000.0, yposUm / 1000.0);
	}
	catch (std::exception& e)
	{
		LogMessage(e.what());
		return ERR_DOVER_CMD_FAILED;
	}
	return DEVICE_OK;
}

int CDoverXYStage::GetPositionSteps(long& x, long& y)
{
	try
	{
		auto xposUm = xyStage->GetPositionX() * 1000.0;
		x = (long)(xposUm / g_umPerStep + 0.5);
		auto yposUm = xyStage->GetPositionY() * 1000.0;
		y = (long)(yposUm / g_umPerStep + 0.5);
	}
	catch (std::exception& e)
	{
		LogMessage(e.what());
		return ERR_DOVER_CMD_FAILED;
	}
	return DEVICE_OK;
}


int CDoverXYStage::Home()
{
	try
	{
		xyStage->Home();
	}
	catch (std::exception& e)
	{
		LogMessage(e.what());
		return ERR_DOVER_HOME_FAILED;
	}
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

int CDoverXYStage::OnPosition(MM::PropertyBase*, MM::ActionType)
{
	return DEVICE_OK;
}
