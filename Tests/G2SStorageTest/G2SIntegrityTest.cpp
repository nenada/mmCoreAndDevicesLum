///////////////////////////////////////////////////////////////////////////////
// FILE:          G2SReaderTest.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     Device Driver Tests
//-----------------------------------------------------------------------------
// DESCRIPTION:   Go2Scope storage driver integrity test
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
extern bool compareText(const std::string& stra, const std::string& strb) noexcept;

/**
 * Storage integrity test:
 *   1. Create (acquire) a complete dataset without the axis info
 *   2. Load the dataset and confirm that all parameters have the expected values (axis info file shouldn't exist)
 *   3. Create (acquire) a complete dataset with the axis info
 *   4. Load the dataset and confirm that all parameters have the expected values (axis info file should exist)
 * 
 * @param core MM Core instance
 * @param path Data folder path
 * @param name Dataset name
 * @param c Channel count
 * @param t Time points
 * @param p Positions count
 * @throws std::runtime_error
 */
void testIntegrity(CMMCore& core, const std::string& path, const std::string& name, int c, int t, int p)
{
	std::cout << std::endl << "Starting G2SStorage driver integrity test" << std::endl;
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

	// STEP 1: Create dataset A (without the axis info)
	auto handleAcqA = core.createDataset(path.c_str(), name.c_str(), shape, MM::StorageDataType_GRAY16, meta.c_str());
	auto pathA = core.getDatasetPath(handleAcqA.c_str());

	std::cout << "STEP 1 - ACQUIRE DATASET / NO AXIS INFO" << std::endl;
	std::cout << "Dataset UID: " << handleAcqA << std::endl;
	std::cout << "Dataset shape (W-H-C-T-P): " << w << " x " << h << " x " << c << " x " << t << " x " << p << " x 16-bit" << std::endl;
	std::cout << "Dataset path: " << pathA << std::endl << std::endl;
	
	std::cout << "START OF ACQUISITION (1)" << std::endl;
	std::vector<std::string> imgmetaA;
	auto startAcqA = std::chrono::high_resolution_clock::now();
	runAcquisition(core, handleAcqA, imgSize, c, t, p, startAcqA, imgmetaA);
	std::cout << "END OF ACQUISITION (1)" << std::endl << std::endl;

	// STEP 2: Load acquired dataset & verify dataset parameters
	std::cout << "STEP 2 - LOAD DATASET / NO AXIS INFO" << std::endl;
	std::string handleLoadA = "";
	try
	{
		std::cout << "Dataset path: " << pathA << std::endl;
		handleLoadA = core.loadDataset(pathA.c_str());
		std::cout << "DATASET LOADED (1)" << std::endl;

		// Validate axis info file
		auto xpa = std::filesystem::u8path(pathA) / "axisinfo.txt";
		if(std::filesystem::exists(xpa))
			throw std::runtime_error("Dataset integrity check failed. Axis info file generated for a dataset without the axis info");

		// Validate dataset parameters
		validateDataset(core, handleLoadA, handleAcqA, shape, meta, imgmetaA);
		core.closeDataset(handleLoadA.c_str());
		std::cout << "DATASET VALIDATION COMPLETED SUCCESSFULLY (1)" << std::endl << std::endl;
	}
	catch(std::exception& e)
	{
		if(!handleLoadA.empty())
			core.closeDataset(handleLoadA.c_str());
		throw;
	}

	// STEP 3: Create dataset B (with the axis info)
	auto handleAcqB = core.createDataset(path.c_str(), name.c_str(), shape, MM::StorageDataType_GRAY16, meta.c_str());
	auto pathB = core.getDatasetPath(handleAcqB.c_str());

	// Form axis info
	std::vector<std::string> axisnames;
	std::vector<std::string> axisdesc;
	std::vector<std::vector<std::string>> axiscoords;
	fillAxisInfo(shape, axisnames, axisdesc, axiscoords);
	configureAxisInfo(core, handleAcqB, axisnames, axisdesc, axiscoords);

	std::cout << "STEP 3 - ACQUIRE DATASET / AXIS INFO DEFINED" << std::endl;
	std::cout << "Dataset UID: " << handleAcqB << std::endl;
	std::cout << "Dataset shape (W-H-C-T-P): " << w << " x " << h << " x " << c << " x " << t << " x " << p << " x 16-bit" << std::endl;
	std::cout << "Dataset path: " << pathB << std::endl << std::endl;
	std::cout << "START OF ACQUISITION (2)" << std::endl;

	std::vector<std::string> imgmetaB;
	auto startAcqB = std::chrono::high_resolution_clock::now();
	runAcquisition(core, handleAcqB, imgSize, c, t, p, startAcqB, imgmetaB);
	std::cout << "END OF ACQUISITION (1)" << std::endl << std::endl;

	// STEP 4: Load acquired dataset
	std::cout << "STEP 4 - LOAD DATASET / AXIS INFO DEFINED" << std::endl;
	std::string handleLoadB = "";
	try
	{
		std::cout << "Dataset path: " << pathB << std::endl;
		handleLoadB = core.loadDataset(pathB.c_str());
		std::cout << "DATASET LOADED (2)" << std::endl;
	
		// Validate axis info file
		std::filesystem::path xpb = std::filesystem::u8path(pathB) / "axisinfo.txt";
		if(!std::filesystem::exists(xpb))
			throw std::runtime_error("Dataset integrity check failed. Axis info file missing");

		// Validate dataset parameters
		validateDataset(core, handleLoadB, handleAcqB, shape, meta, imgmetaB);
		validateAxisInfo(core, handleLoadB, shape, axisnames, axisdesc, axiscoords);
		core.closeDataset(handleLoadB.c_str());
		std::cout << "DATASET VALIDATION COMPLETED SUCCESSFULLY (1)" << std::endl << std::endl;
	}
	catch(std::exception& e)
	{
		if(!handleLoadB.empty())
			core.closeDataset(handleLoadB.c_str());
		throw;
	}
}