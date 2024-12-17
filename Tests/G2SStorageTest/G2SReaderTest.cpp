///////////////////////////////////////////////////////////////////////////////
// FILE:          G2SReaderTest.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     Device Driver Tests
//-----------------------------------------------------------------------------
// DESCRIPTION:   Go2Scope storage driver reader test
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
#include <filesystem>
#include <string>
#include <vector>
#include <chrono>
#include "MMCore.h"

extern std::vector<long> calcCoordsOptimized(long ind, const std::vector<long>& shape);
extern std::vector<long> calcCoordsRandom(long ind, const std::vector<long>& shape);

/**
 * Storage read test
 * @param core MM Core instance
 * @param path Data folder path
 * @param name Dataset name
 * @param optimized Optimized / random access test
 * @param printmeta Print image metadata
 * @throws std::runtime_error
 */
void testReader(CMMCore& core, const std::string& path, const std::string& name, bool optimized, bool printmeta)
{
	std::cout << std::endl << "Starting G2SStorage driver reader test" << std::endl;
	std::filesystem::path ds = std::filesystem::u8path(path) / (name + ".g2s");

	// Load the dataset
	auto start = std::chrono::high_resolution_clock::now();
	auto handle = core.loadDataset(ds.u8string().c_str());
	auto loadEnd = std::chrono::high_resolution_clock::now();
	double loadTimeS = (loadEnd - start).count() / 1000000000.0;

	// Obtain dataset shape
	auto shape = core.getDatasetShape(handle.c_str());
	auto ptype = core.getDatasetPixelType(handle.c_str());
	auto pos = shape.size() == 5 ? shape[0] : 0;
	auto timep = shape.size() == 5 ? shape[1] : shape[0];
	auto chn = shape.size() == 5 ? shape[2] : shape[1];
	auto imgw = shape.size() == 5 ? shape[3] : shape[2];
	auto imgh = shape.size() == 5 ? shape[4] : shape[3];
	auto imgcnt = (pos == 0 ? 1 : pos) * timep * chn;
	auto imgSize = imgw * imgh * (ptype == MM::StorageDataType_GRAY16 ? 2 : 1);
	double imgSizeMb = (double)imgSize / (1024.0 * 1024.0);
	double totalSizeMb = (double)imgSize * imgcnt / (1024.0 * 1024.0);
	std::cout << std::fixed << std::setprecision(3) << "Dataset loaded in " << loadTimeS << " sec, contains " << imgcnt << " images" << std::endl;
	std::cout << "Dataset UID: " << handle << std::endl;
	std::cout << "Dataset shape (W-H-C-T-P): " << imgw << " x " << imgh << " x " << chn << " x " << timep << " x " << pos << " x " << (ptype == MM::StorageDataType_GRAY16 ? 16 : 8) << "-bit" << std::endl << std::endl;

	// Read images
	for(long i = 0; i < imgcnt; i++)
	{
		// Calculate coordinates
		auto coords = optimized ? calcCoordsOptimized(i, shape) : calcCoordsRandom(i, shape);

		// Read image from the file stream
		auto startRead = std::chrono::high_resolution_clock::now();
		auto img = core.getImage(handle.c_str(), coords);
		auto emdRead = std::chrono::high_resolution_clock::now();
		if(img == nullptr)
			throw std::runtime_error("Failed to fetch image " + i);
		double readTimeMs = (emdRead - startRead).count() / 1000000.0;

		double bw = imgSizeMb / (readTimeMs / 1000.0);
		std::cout << "Image " << std::setw(3) << i << " [";
		for(std::size_t i = 0; i < coords.size(); i++)
			std::cout << (i == 0 ? "" : ", ") << coords[i];
		std::cout << "], size: " << std::fixed << std::setprecision(1) << imgSizeMb << " MB in " << readTimeMs << " ms (" << bw << " MB/s)" << std::endl;

		auto meta = core.getImageMeta(handle.c_str(), coords);
		if(printmeta)
			std::cout << "Image metadata: " << meta << std::endl;
	}
	
	// We are done so close the dataset
	core.closeDataset(handle.c_str());
	auto end = std::chrono::high_resolution_clock::now();
	std::cout << std::endl;
	
	// Calculate storage driver bandwidth
	double totalTimeS = (end - start).count() / 1000000000.0;
	double bw = totalSizeMb / totalTimeS;
	std::cout << std::fixed << std::setprecision(3) << "Read completed in " << totalTimeS << " sec" << std::endl;
	std::cout << std::fixed << std::setprecision(1) << "Dataset size " << totalSizeMb << " MB" << std::endl;
	std::cout << std::fixed << std::setprecision(1) << "Storage driver bandwidth " << bw << " MB/s" << std::endl;
}