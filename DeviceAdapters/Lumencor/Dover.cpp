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

static int doverInstanceCounter(0);
const double umPerStep(0.005); // TODO: this should be picked up from the Dover configuration file

CDoverStage::CDoverStage()
{
	zStage = new dover::DOF5Stage;
}

CDoverStage::~CDoverStage()
{
	Shutdown();
	delete zStage;
}

bool CDoverStage::Busy()
{
	return false;
}

void CDoverStage::GetName(char* pszName) const
{
}

int CDoverStage::Initialize()
{
	try
	{ 
		zStage->Initialize();
		// this will automatically create a global instance of the API
		doverInstanceCounter++;
	}
	catch (std::exception& e)
	{
		LogMessage(e.what());
		return DEVICE_NATIVE_MODULE_FAILED;
	}
	auto pAct = new CPropertyAction(this, &CDoverStage::OnPosition);
	CreateProperty(MM::g_Keyword_Position, "0", MM::Float, false, pAct);
	// TODO
	//SetPropertyLimits(MM::g_Keyword_Position, min, max);

	return DEVICE_OK;
}

int CDoverStage::Shutdown()
{
	doverInstanceCounter--;
	doverInstanceCounter = max(0, doverInstanceCounter);

	// last instance releases the API
	if (doverInstanceCounter == 0)
		dover::DoverApi::releaseInstance();

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
	return umPerStep;
}

int CDoverStage::SetPositionSteps(long steps)
{
	double posUm = steps * umPerStep;
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
		steps = (long)(posUm / umPerStep + 0.5);
	}
	catch (std::exception& e)
	{
		LogMessage(e.what());
		return ERR_DOVER_CMD_FAILED;
	}
	return DEVICE_OK;
}

int CDoverStage::SetOrigin()
{
	// TODO: what to do here?
	return DEVICE_OK;
}

int CDoverStage::GetLimits(double& lower, double& upper)
{
	// TODO:
	return 0;
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


CDoverXYStage::CDoverXYStage()
{
}

CDoverXYStage::~CDoverXYStage()
{
}

bool CDoverXYStage::Busy()
{
	return false;
}

void CDoverXYStage::GetName(char* pszName) const
{
}

int CDoverXYStage::Initialize()
{
	return 0;
}

int CDoverXYStage::Shutdown()
{
	return 0;
}

double CDoverXYStage::GetStepSize()
{
	return 0.0;
}

int CDoverXYStage::SetPositionSteps(long x, long y)
{
	return 0;
}

int CDoverXYStage::GetPositionSteps(long& x, long& y)
{
	return 0;
}

int CDoverXYStage::SetRelativePositionSteps(long x, long y)
{
	return 0;
}

int CDoverXYStage::Home()
{
	return 0;
}

int CDoverXYStage::Stop()
{
	return 0;
}

int CDoverXYStage::SetOrigin()
{
	return 0;
}

int CDoverXYStage::GetLimitsUm(double& xMin, double& xMax, double& yMin, double& yMax)
{
	return 0;
}

int CDoverXYStage::GetStepLimits(long&, long&, long&, long&)
{
	return 0;
}

double CDoverXYStage::GetStepSizeXUm()
{
	return 0.0;
}

double CDoverXYStage::GetStepSizeYUm()
{
	return 0.0;
}

int CDoverXYStage::OnPosition(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return 0;
}
