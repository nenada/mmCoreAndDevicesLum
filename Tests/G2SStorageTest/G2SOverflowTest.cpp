///////////////////////////////////////////////////////////////////////////////
// FILE:          G2SReaderTest.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     Device Driver Tests
//-----------------------------------------------------------------------------
// DESCRIPTION:   Go2Scope storage driver overflow test
//
// AUTHOR:        Milos Jovanovic <milos@tehnocad.rs>
//
// COPYRIGHT:     Nenad Amodaj, Chan Zuckerberg Initiative, 2024
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
// NOTE:          Storage Device development is supported in part by
//                Chan Zuckerberg Initiative (CZI)
// 
///////////////////////////////////////////////////////////////////////////////
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include "MMCore.h"

extern void runAcquisition(CMMCore& core, const std::string& handle, int imgSize, int c, int t, int p, std::chrono::steady_clock::time_point& startAcq, std::vector<std::string>& vmeta, int imglimit = 0);
extern void configureAxisInfo(CMMCore& core, const std::string& handle, const std::vector<std::string>& axisnames, const std::vector<std::string>& axisdesc, const std::vector<std::vector<std::string>>& axiscoords);
extern void fillAxisInfo(const std::vector<long>& shape, std::vector<std::string>& axisnames, std::vector<std::string>& axisdesc, std::vector<std::vector<std::string>>& axiscoords);
extern void validateDataset(CMMCore& core, const std::string& handle, const std::string& acqhandle, const std::vector<long>& acqshape, const std::string& acqmeta, const std::vector<std::string>& acqimgmeta, int expimgcount = 0);
extern void validateAxisInfo(CMMCore& core, const std::string& handle, const std::vector<long>& shape, const std::vector<std::string>& dname, const std::vector<std::string>& ddesc, const std::vector<std::vector<std::string>>& dcoord);

/**
 * Storage integrity test for acquisition with overflow (larger axis 0 than originally specified) * 
 * @param core MM Core instance
 * @param path Data folder path
 * @param name Dataset name
 * @param c Channel count
 * @param t Time points
 * @param p Positions count
 * @throws std::runtime_error
 */
void testOverflow(CMMCore& core, const std::string& path, const std::string& name, int c, int t, int p)
{
	std::cout << std::endl << "Starting G2SStorage driver overflow test" << std::endl;
	int w = (int)core.getImageWidth();
	int h = (int)core.getImageHeight();
	int imgSize = 2 * w * h;

	// Shape convention: Z/P, T, C, Y, X
	std::vector<long> shape = { p, t, c, h, w };
	if(p == 0)
		shape = { t, c, h, w };
	std::string meta = "{\"name\":\"" + name + "\",\"channels\":" + std::to_string(c) + ",\"timepoints\":" + std::to_string(t);
	if(p > 0)
		meta += ",\"positions\":" + std::to_string(p);
	meta += "}";

	// STEP 1: Create a dataset
	auto handleAcq = core.createDataset(path.c_str(), name.c_str(), shape, MM::StorageDataType_GRAY16, meta.c_str());
	auto actualpath = core.getDatasetPath(handleAcq.c_str());

	// Form axis info
	std::vector<std::string> axisnames;
	std::vector<std::string> axisdesc;
	std::vector<std::vector<std::string>> axiscoords;
	fillAxisInfo(shape, axisnames, axisdesc, axiscoords);
	for(int i = 0; i < 3; i++)
		axiscoords[0].push_back((p == 0 ? "T" : "P") + std::to_string(shape[0] + i));
	configureAxisInfo(core, handleAcq, axisnames, axisdesc, axiscoords);

	std::cout << "STEP 1 - ACQUIRE DATASET" << std::endl;
	std::cout << "Dataset UID: " << handleAcq << std::endl;
	std::cout << "Dataset shape (W-H-C-T-P): " << w << " x " << h << " x " << c << " x " << t << " x " << p << " x 16-bit" << std::endl;
	std::cout << "Dataset path: " << actualpath << std::endl << std::endl;
	std::cout << "START OF ACQUISITION" << std::endl;

	int acqp = p == 0 ? 0 : p + 3;
	int acqt = p == 0 ? t + 3 : t;
	int acqimgcount = (acqp == 0 ? 1 : acqp) * acqt * c;
	std::vector<std::string> imgmeta;
	auto startAcq = std::chrono::high_resolution_clock::now();
	runAcquisition(core, handleAcq, imgSize, c, acqt, acqp, startAcq, imgmeta);
	std::cout << "END OF ACQUISITION" << std::endl << std::endl;

	// STEP 4: Load acquired dataset
	std::cout << "STEP 2 - LOAD DATASET" << std::endl;
	std::string handleLoad = "";
	try
	{
		std::cout << "Dataset path: " << actualpath << std::endl;
		handleLoad = core.loadDataset(actualpath.c_str());
		std::cout << "DATASET LOADED" << std::endl;

		// Validate dataset parameters
		validateDataset(core, handleLoad, handleAcq, shape, meta, imgmeta, acqimgcount);
		validateAxisInfo(core, handleLoad, shape, axisnames, axisdesc, axiscoords);
		core.closeDataset(handleLoad.c_str());
		std::cout << "DATASET VALIDATION COMPLETED SUCCESSFULLY" << std::endl << std::endl;
	}
	catch(std::exception& e)
	{
		if(!handleLoad.empty())
			core.closeDataset(handleLoad.c_str());
		throw;
	}
}