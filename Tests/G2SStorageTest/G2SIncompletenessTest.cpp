///////////////////////////////////////////////////////////////////////////////
// FILE:          G2SReaderTest.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     Device Driver Tests
//-----------------------------------------------------------------------------
// DESCRIPTION:   Go2Scope storage driver incompleteness test
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
extern std::vector<long> calcCoordsOptimized(long ind, const std::vector<long>& shape);

/**
 * Storage incomplete acquisition integrity test
 * @param core MM Core instance
 * @param path Data folder path
 * @param name Dataset name
 * @param c Channel count
 * @param t Time points
 * @param p Positions count
 * @throws std::runtime_error
 */
void testIncompleteness(CMMCore& core, const std::string& path, const std::string& name, int c, int t, int p)
{
	std::cout << std::endl << "Starting G2SStorage driver partial acquisition test" << std::endl;
	int w = (int)core.getImageWidth();
	int h = (int)core.getImageHeight();
	int imgSize = 2 * w * h;

	// Validate dataset dimensions
	if(p == 1 || (p == 0 && t <= 1))
	{
		std::cout << "Invalid dataset dimensions for incompleteness test (P x T x C): " << p << " - " << t << " - " << c << std::endl; 
		return;
	}

	// Shape convention: Z/P, T, C, Y, X
	int imgCount = (p == 0 ? 1 : p) * t * c;
	std::vector<long> shape = { p, t, c, h, w };
	if(p == 0)
		shape = { t, c, h, w };
	std::string meta = "{\"name\":\"" + name + "\",\"channels\":" + std::to_string(c) + ",\"timepoints\":" + std::to_string(t);
	if(p > 0)
		meta += ",\"positions\":" + std::to_string(p);
	meta += "}";

	// Form axis info
	std::vector<std::string> axisnames;
	std::vector<std::string> axisdesc;
	std::vector<std::vector<std::string>> axiscoords;
	fillAxisInfo(shape, axisnames, axisdesc, axiscoords);

	// STEP 1: Create incomplete dataset A / axis 0 split
	auto handleAcqA = core.createDataset(path.c_str(), name.c_str(), shape, MM::StorageDataType_GRAY16, meta.c_str());
	auto pathA = core.getDatasetPath(handleAcqA.c_str());
	configureAxisInfo(core, handleAcqA, axisnames, axisdesc, axiscoords);
	int acqAp = p == 0 ? 0 : p / 2;
	int acqAt = p == 0 ? t / 2 : t;
	int acqAcnt = (acqAp == 0 ? 1 : acqAp) * acqAt * c;

	std::cout << "STEP 1 - ACQUIRE DATASET / AXIS 0 SPLIT" << std::endl;
	std::cout << "Dataset UID: " << handleAcqA << std::endl;
	std::cout << "Dataset shape - Expected (W-H-C-T-P): " << w << " x " << h << " x " << c << " x " << t << " x " << p << " x 16-bit" << std::endl;
	std::cout << "Dataset shape - Acquired (W-H-C-T-P): " << w << " x " << h << " x " << c << " x " << acqAt << " x " << acqAp << " x 16-bit" << std::endl;
	std::cout << "Number of images (expected / acquired): " << imgCount << " / " << acqAcnt << std::endl;
	std::cout << "Dataset path: " << pathA << std::endl << std::endl;
	
	std::cout << "START OF ACQUISITION (1)" << std::endl;
	std::vector<std::string> imgmetaA;
	auto startAcqA = std::chrono::high_resolution_clock::now();
	runAcquisition(core, handleAcqA, imgSize, c, acqAt, acqAp, startAcqA, imgmetaA);
	std::cout << "END OF ACQUISITION (1)" << std::endl << std::endl;

	// STEP 2: Load acquired dataset & verify dataset parameters
	std::cout << "STEP 2 - LOAD & VALIDATE DATASET / AXIS 0 SPLIT" << std::endl;
	std::string handleLoadA = "";
	try
	{
		std::cout << "Dataset path: " << pathA << std::endl;
		handleLoadA = core.loadDataset(pathA.c_str());
		std::cout << "DATASET LOADED (1)" << std::endl;

		// Validate dataset parameters
		validateDataset(core, handleLoadA, handleAcqA, shape, meta, imgmetaA, acqAcnt);
		validateAxisInfo(core, handleLoadA, shape, axisnames, axisdesc, axiscoords);

		// Test missing image access
		try
		{
			std::vector<long> invcoords = calcCoordsOptimized(imgCount - 1, shape);
			auto img = core.getImage(handleLoadA.c_str(), invcoords);
			if(img != nullptr)
				throw std::runtime_error("Dataset integrity check failed. Missing image access returned non-null handle");
		}
		catch(CMMError&)
		{
			std::cout << "MISSING IMAGE ACCESS CHECK OK - CMMError raised" << std::endl;
		}

		core.closeDataset(handleLoadA.c_str());
		std::cout << "DATASET VALIDATION COMPLETED SUCCESSFULLY (1)" << std::endl << std::endl;
	}
	catch(std::exception& e)
	{
		if(!handleLoadA.empty())
			core.closeDataset(handleLoadA.c_str());
		throw;
	}

	// STEP 3: Create incomplete dataset B / random split
	// Choose image count
	int acqBcnt = acqAcnt - (p == 0 ? 0 : t * c) - c - 1;
	if(acqBcnt <= 0)
		acqBcnt = imgCount - (p == 0 ? 0 : t * c) - c - 1;

	// Create dataset
	auto handleAcqB = core.createDataset(path.c_str(), name.c_str(), shape, MM::StorageDataType_GRAY16, meta.c_str());
	auto pathB = core.getDatasetPath(handleAcqB.c_str());
	configureAxisInfo(core, handleAcqB, axisnames, axisdesc, axiscoords);

	std::cout << "STEP 3 - ACQUIRE DATASET / RANDOM SPLIT" << std::endl;
	std::cout << "Dataset UID: " << handleAcqB << std::endl;
	std::cout << "Dataset shape - Expected (W-H-C-T-P): " << w << " x " << h << " x " << c << " x " << t << " x " << p << " x 16-bit" << std::endl;
	std::cout << "Number of images (expected / acquired): " << imgCount << " / " << acqBcnt << std::endl;
	std::cout << "Dataset path: " << pathB << std::endl << std::endl;

	std::cout << "START OF ACQUISITION (2)" << std::endl;
	std::vector<std::string> imgmetaB;
	auto startAcqB = std::chrono::high_resolution_clock::now();
	runAcquisition(core, handleAcqB, imgSize, c, t, p, startAcqB, imgmetaB, acqBcnt);
	std::cout << "END OF ACQUISITION (2)" << std::endl << std::endl;

	// STEP 4: Load acquired dataset & verify dataset parameters
	std::cout << "STEP 4 - LOAD & VALIDATE DATASET / RANDOM SPLIT" << std::endl;
	std::string handleLoadB = "";
	try
	{
		std::cout << "Dataset path: " << pathB << std::endl;
		handleLoadB = core.loadDataset(pathB.c_str());
		std::cout << "DATASET LOADED (2)" << std::endl;

		// Validate dataset parameters
		validateDataset(core, handleLoadB, handleAcqB, shape, meta, imgmetaB, acqBcnt);
		validateAxisInfo(core, handleLoadB, shape, axisnames, axisdesc, axiscoords);
		core.closeDataset(handleLoadB.c_str());
		std::cout << "DATASET VALIDATION COMPLETED SUCCESSFULLY (2)" << std::endl << std::endl;
	}
	catch(std::exception& e)
	{
		if(!handleLoadB.empty())
			core.closeDataset(handleLoadB.c_str());
		throw;
	}

	// STEP 5: Create incomplete dataset C / single image
	// Choose image count
	int acqCcnt = 1;

	// Create dataset
	auto handleAcqC = core.createDataset(path.c_str(), name.c_str(), shape, MM::StorageDataType_GRAY16, meta.c_str());
	auto pathC = core.getDatasetPath(handleAcqC.c_str());
	configureAxisInfo(core, handleAcqC, axisnames, axisdesc, axiscoords);

	std::cout << "STEP 5 - ACQUIRE DATASET / SINGLE IMAGE" << std::endl;
	std::cout << "Dataset UID: " << handleAcqC << std::endl;
	std::cout << "Dataset shape - Expected (W-H-C-T-P): " << w << " x " << h << " x " << c << " x " << t << " x " << p << " x 16-bit" << std::endl;
	std::cout << "Number of images (expected / acquired): " << imgCount << " / " << acqCcnt << std::endl;
	std::cout << "Dataset path: " << pathC << std::endl << std::endl;

	std::cout << "START OF ACQUISITION (3)" << std::endl;
	std::vector<std::string> imgmetaC;
	auto startAcqC = std::chrono::high_resolution_clock::now();
	runAcquisition(core, handleAcqC, imgSize, c, t, p, startAcqC, imgmetaC, acqCcnt);
	std::cout << "END OF ACQUISITION (2)" << std::endl << std::endl;

	// STEP 6: Load acquired dataset & verify dataset parameters
	std::cout << "STEP 6 - LOAD & VALIDATE DATASET / SINGLE IMAGE" << std::endl;
	std::string handleLoadC = "";
	try
	{
		std::cout << "Dataset path: " << pathC << std::endl;
		handleLoadC = core.loadDataset(pathC.c_str());
		std::cout << "DATASET LOADED (3)" << std::endl;

		// Validate dataset parameters
		validateDataset(core, handleLoadC, handleAcqC, shape, meta, imgmetaC, acqCcnt);
		validateAxisInfo(core, handleLoadC, shape, axisnames, axisdesc, axiscoords);
		core.closeDataset(handleLoadC.c_str());
		std::cout << "DATASET VALIDATION COMPLETED SUCCESSFULLY (3)" << std::endl << std::endl;
	}
	catch(std::exception& e)
	{
		if(!handleLoadC.empty())
			core.closeDataset(handleLoadC.c_str());
		throw;
	}
}