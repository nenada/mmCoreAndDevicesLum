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

CWDIStage::CWDIStage() : initialized(false)
{
}

CWDIStage::~CWDIStage()
{
}

bool CWDIStage::Busy()
{
   return false;
}

void CWDIStage::GetName(char* pszName) const
{
}

int CWDIStage::Initialize()
{
   return 0;
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
