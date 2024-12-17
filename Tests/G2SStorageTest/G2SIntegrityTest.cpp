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

extern void runAcquisition(CMMCore& core, const std::string& handle, int imgSize, int c, int t, int p, std::chrono::steady_clock::time_point& startAcq, std::vector<std::string>& vmeta);
extern std::vector<long> calcCoordsOptimized(long ind, const std::vector<long>& shape);
extern bool compareText(const std::string& stra, const std::string& strb) noexcept;

/**
 * Validate dataset parameters
 * @param core MM Core instance
 * @param handle Dataset handle (from the loaded dataset)
 * @param acqhandle Expected dataset handle
 * @param acqshape Expected dataset shape
 * @param acqmeta Expected dataset meta
 * @param acqimgmeta Expected image metadata list
 * @throws std::runtime_error
 */
void validateDataset(CMMCore& core, const std::string& handle, const std::string& acqhandle, const std::vector<long>& acqshape, const std::string& acqmeta, const std::vector<std::string>& acqimgmeta)
{	
	// Validate UID
	if(handle != acqhandle)
		throw std::runtime_error("Dataset integrity check failed. Dataset UID missmatch");
	std::cout << "Dataset UID: " << handle << std::endl;

	// Validate shape
	std::vector<long> shape = core.getDatasetShape(handle.c_str());
	if(shape.size() != acqshape.size())
		throw std::runtime_error("Dataset integrity check failed. Dataset shape dimension missmatch");
	for(std::size_t i = 0; i < shape.size(); i++)
	{
		if(shape[i] != acqshape[i])
			throw std::runtime_error("Dataset integrity check failed. Dataset axis dimension missmatch, axis " + std::to_string(i));
	}
	int w = shape[shape.size() - 1];
	int h = shape[shape.size() - 2];
	int c = shape[shape.size() - 3];
	int t = shape[shape.size() - 4];
	int p = shape.size() > 4 ? shape[0] : 0;
	int shapeimgcount = c * t * (p == 0 ? 1 : p);
	std::uint32_t imgsize = 2 * w * h;
	double imgSizeMb = (double)imgsize / (1024.0 * 1024.0);
	std::cout << "Dataset shape (W-H-C-T-P): " << w << " x " << h << " x " << c << " x " << t << " x " << p << " x 16-bit" << std::endl;

	// Validate pixel format
	auto pixformat = core.getDatasetPixelType(handle.c_str());
	if(pixformat != MM::StorageDataType_GRAY16)
		throw std::runtime_error("Dataset integrity check failed. Dataset pixel format missmatch");

	// Validate image count
	auto imgcnt = core.getImageCount(handle.c_str());
	if(imgcnt != shapeimgcount)
		throw std::runtime_error("Dataset integrity check failed. Dataset image count missmatch");
	std::cout << "Dataset image count: " << imgcnt << std::endl;

	// Validate metadata
	auto meta = core.getSummaryMeta(handle.c_str());
	if(!compareText(meta, acqmeta))
		throw std::runtime_error("Dataset integrity check failed. Dataset metadata missmatch");
	std::cout << "Dataset metadata: " << meta << std::endl;

	// Read images (with image metadata)
	for(long i = 0; i < imgcnt; i++)
	{
		// Calculate coordinates
		auto coords = calcCoordsOptimized(i, shape);

		// Read image from the file stream
		auto img = core.getImage(handle.c_str(), coords);
		if(img == nullptr)
			throw std::runtime_error("Dataset integrity check failed. Failed to fetch image " + i);

		std::cout << "Image " << std::setw(3) << i << " [";
		for(std::size_t i = 0; i < coords.size(); i++)
			std::cout << (i == 0 ? "" : ", ") << coords[i];
		std::cout << "], size: " << std::fixed << std::setprecision(1) << imgSizeMb << " MB" << std::endl;

		auto imgmeta = core.getImageMeta(handle.c_str(), coords);
		if(imgmeta.empty())
			throw std::runtime_error("Dataset integrity check failed. Failed to fetch image metadata, image " + i);
		if((std::size_t)i >= acqimgmeta.size() || !compareText(imgmeta, acqimgmeta[i]))
			throw std::runtime_error("Dataset integrity check failed. Image metadata missmatch, image " + i);
	}
}

/**
 * Validate dataset axis info
 * @param core MM Core instance
 * @param handle Dataset handle (from the loaded dataset)
 * @param shape Dataset shape
 * @param dname Expected dimension names
 * @param ddesc Expected dimension descriptions
 * @param dcoord Expected axis coordinate names
 * @throws std::runtime_error
 */
void validateAxisInfo(CMMCore& core, const std::string& handle, const std::vector<long>& shape, const std::vector<std::string>& dname, const std::vector<std::string>& ddesc, const std::vector<std::vector<std::string>>& dcoord)
{
	int c = shape[shape.size() - 3];
	int t = shape[shape.size() - 4];
	int p = shape.size() > 4 ? shape[0] : 0;

	if(shape.size() != dname.size() || shape.size() != ddesc.size() || shape.size() - 2 != dcoord.size())
		throw std::runtime_error("Dataset integrity check failed. Dataset dimension info vector size missmatch");

	for(std::size_t i = 0; i < shape.size(); i++)
	{
		std::string xval = core.getDimensionName(handle.c_str(), (int)i);
		std::string yval = core.getDimensionMeaning(handle.c_str(), (int)i);
		if(!compareText(xval, dname[i]))
			throw std::runtime_error("Dataset integrity check failed. Axis name missmatch, axis " + std::to_string(i));
		if(!compareText(yval, ddesc[i]))
			throw std::runtime_error("Dataset integrity check failed. Axis description missmatch, axis " + std::to_string(i));
		
		if(i >= shape.size() - 2)
			continue;
		if((std::size_t)shape[i] != dcoord[i].size())
			throw std::runtime_error("Dataset integrity check failed. Axis coordinate vector size missmatch");
		for(long j = 0; j < shape[i]; j++)
		{
			std::string zval = core.getCoordinateName(handle.c_str(), (int)i, (int)j);
			if(!compareText(zval, dcoord[i][j]))
				throw std::runtime_error("Dataset integrity check failed. Axis coordinate name missmatch, axis " + std::to_string(i) + ", coordinate " + std::to_string(j));
		}
		std::cout << "Axis " << i << xval << " (" << yval << "), " << dcoord[i].size() << " coordinates" << std::endl;
	}
}

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
	std::vector<std::string> axisnames = { "T", "C", "Y", "X" };
	std::vector<std::string> axisdesc = { "Time point", "Image channel", "Image height", "Image width" };
	std::vector<std::vector<std::string>> axiscoords(shape.size() - 2);
	if(p > 0)
	{
		axisnames = { "P", "T", "C", "Y", "X" };	
		axisdesc = { "XY Position", "Time point", "Image channel", "Image height", "Image width" };
		axiscoords[0].resize(p);
		axiscoords[1].resize(t);
		axiscoords[2].resize(c);
		for(int i = 0; i < p; i++)
			axiscoords[0][i] = "Position" + std::to_string(i);
	}
	else
	{
		axiscoords[0].resize(t);
		axiscoords[1].resize(c);
	}

	for(int i = 0; i < t; i++)
		axiscoords[p == 0 ? 0 : 1][i] = "T" + std::to_string(i);
	for(int i = 0; i < c; i++)
		axiscoords[p == 0 ? 1 : 2][i] = "Channel" + std::to_string(i);

	// Set axis info
	for(int i = 0; i < (int)shape.size(); i++)
	{
		core.configureDimension(handleAcqB.c_str(), i, axisnames[i].c_str(), axisdesc[i].c_str());
		if(i < (int)shape.size() - 2)
		{
			for(int j = 0; j < shape[i]; j++)
				core.configureCoordinate(handleAcqB.c_str(), i, j, axiscoords[i][j].c_str());
		}
	}

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