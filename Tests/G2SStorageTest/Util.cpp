///////////////////////////////////////////////////////////////////////////////
// FILE:          Util.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     Device Driver Tests
//-----------------------------------------------------------------------------
// DESCRIPTION:   Helper methods
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
#include <sstream>
#include <thread>
#include "MMCore.h"

/**
 * Generate image metadata
 * @param core MM Core instance
 * @param imgind Image index
 * @return Image metadata (JSON)
 */
std::string generateImageMeta(CMMCore& core, int imgind)
{
	// Calculate pixel type
	std::string pixtype = "";
	auto depth = core.getBytesPerPixel();
	auto numComponents = core.getNumberOfComponents();
	switch (depth) {
		case 1:
			pixtype = "GRAY8";
			break;
		case 2:
			pixtype = "GRAY16";
			break;
		case 4:
			if(numComponents == 1)
				pixtype = "GRAY32";
			else
				pixtype = "RGB32";
			break;
		case 8:
			pixtype = "RGB64";
			break;
		default:
			break;
	}

	// Calculate ROI
	int x = 0, y = 0, w = 0, h = 0;
	core.getROI(x, y, w, h);
	std::string roi = std::to_string(x) + "-" + std::to_string(y) + "-" + std::to_string(w) + "-" + std::to_string(h);

	// Calculate ROI affine transform
	auto aff = core.getPixelSizeAffine(true);
	std::string psizeaffine = "";
	if(aff.size() == 6) 
	{
		for(int i = 0; i < 5; ++i)
			psizeaffine += std::to_string(aff[i]) + ";";
		psizeaffine += std::to_string(aff[5]);
	}
	
	// Write JSON
	std::stringstream ss;
	ss << "{";
	Configuration config = core.getSystemStateCache();
	for(int i = 0; (long)i < config.size(); ++i) 
	{
		PropertySetting setting = config.getSetting((long)i);
		auto key = setting.getDeviceLabel() + "-" + setting.getPropertyName();
		auto value = setting.getPropertyValue();
		ss << "\"" << key << "\":\"" << value << "\",";
	}
	ss << "\"BitDepth\":" << core.getImageBitDepth() << ",";
	ss << "\"PixelSizeUm\":" << core.getPixelSizeUm(true) << ",";
	ss << "\"PixelSizeAffine\":\"" << psizeaffine << "\",";
	ss << "\"ROI\":\"" << roi << "\",";
	ss << "\"Width\":" << core.getImageWidth() << ",";
	ss << "\"Height\":" << core.getImageHeight() << ",";
	ss << "\"PixelType\":\"" << pixtype << "\",";
	ss << "\"Frame\":0,";
	ss << "\"FrameIndex\":0,";
	ss << "\"Position\":\"Default\",";
	ss << "\"PositionIndex\":0,";
	ss << "\"Slice\":0,";
	ss << "\"SliceIndex\":0,";
	auto channel = core.getCurrentConfigFromCache(core.getPropertyFromCache("Core", "ChannelGroup").c_str());
	if(channel.empty())
		channel = "Default";
	ss << "\"Channel\":\""<< channel << "\",";
	ss << "\"ChannelIndex\":0,";

	try { ss << "\"BitDepth\":\"" << core.getProperty(core.getCameraDevice().c_str(), "Binning") << "\","; } catch(...) { }

	ss << "\"Image-index\":" << imgind;
	ss << "}";
	return ss.str();
}

/**
 * Calculate image coordinates for optimized access
 * @param ind Image index
 * @param shape Dataset shape
 * @param coords Image coordinates [out]
 */
std::vector<long> calcCoordsOptimized(long ind, const std::vector<long>& shape) 
{
	std::vector<long> ret(shape.size());
	int fx = 0;
	for(int j = 0; j < (int)shape.size() - 2; j++) {
		int sum = 1;
		for(int k = j + 1; k < (int)shape.size() - 2; k++)
			sum *= shape[k];
		int ix = (ind - fx) / sum;
		ret[j] = ix;
		fx += ix * sum;
	}
	return ret;
}

/**
 * Calculate image coordinates for random access
 * @param ind Image index
 * @param shape Dataset shape
 * @param coords Image coordinates [out]
 */
std::vector<long> calcCoordsRandom(long ind, const std::vector<long>& shape) 
{
	std::vector<long> ret(shape.size());
	int fx = 0;
	for(int j = (int)shape.size() - 3; j >= 0; j--) {
		int sum = 1;
		for(int k = 0; k < j; k++)
			sum *= shape[k];
		int ix = (ind - fx) / sum;
		ret[j] = ix;
		fx += ix * sum;
	}
	return ret;
}

/**
 * Execute dataset acquisition
 * @param core MM Core instance
 * @param handle Dataset UID
 * @param imgSize Image size (bytes)
 * @param c Number of channels
 * @param t Number of time points
 * @param p Number of positions
 * @param startAcq Acquisition start time point [out]
 * @param vmeta Image metadata list [out]
 */
void runAcquisition(CMMCore& core, const std::string& handle, int imgSize, int c, int t, int p, std::chrono::steady_clock::time_point& startAcq, std::vector<std::string>& vmeta)
{
	try
	{
		int imgind = 0;
		double imgSizeMb = (double)imgSize / (1024.0 * 1024.0);
		if(p == 0)
		{
			// Start acquisition
			core.startSequenceAcquisition(c * t, 0.0, true);
			for(int j = 0; j < t; j++) 
			{
				for(int k = 0; k < c; k++)
				{
					if(core.isBufferOverflowed()) 
						throw std::runtime_error("Buffer overflow!!");

					// wait for images to become available
					while(core.getRemainingImageCount() == 0)
						std::this_thread::sleep_for(std::chrono::milliseconds(1));

					// Reset acquisition timer when the first image becomes available)
					if(imgind == 0)
						startAcq = std::chrono::high_resolution_clock::now();

					// fetch the image
					unsigned char* img = reinterpret_cast<unsigned char*>(core.popNextImage());

					// Generate image metadata
					std::string meta = generateImageMeta(core, imgind);

					// Add image to the stream
					auto startSave = std::chrono::high_resolution_clock::now();
					core.addImage(handle.c_str(), imgSize, img, { j, k }, meta.c_str());
					auto endSave = std::chrono::high_resolution_clock::now();

					// Calculate statistics
					double imgSaveTimeMs = (endSave - startSave).count() / 1000000.0;
					double bw = imgSizeMb / (imgSaveTimeMs / 1000.0);
					std::cout << "Saved image " << imgind++ << " in ";
					std::cout << std::fixed << std::setprecision(2) << imgSaveTimeMs << " ms, size ";
					std::cout << std::fixed << std::setprecision(1) << imgSizeMb << " MB, BW: " << bw << " MB/s" << std::endl;
					vmeta.push_back(meta);
				}
			}
		}
		else
		{
			// Start acquisition
			core.startSequenceAcquisition(c * t * p, 0.0, true);
			for(int i = 0; i < p; i++) 
			{
				for(int j = 0; j < t; j++) 
				{
					for(int k = 0; k < c; k++)
					{
						if(core.isBufferOverflowed()) 
							throw std::runtime_error("Buffer overflow!!");

						// wait for images to become available
						while(core.getRemainingImageCount() == 0)
							std::this_thread::sleep_for(std::chrono::milliseconds(1));

						// Reset acquisition timer when the first image becomes available)
						if(imgind == 0)
							startAcq = std::chrono::high_resolution_clock::now();

						// fetch the image
						unsigned char* img = reinterpret_cast<unsigned char*>(core.popNextImage());

						// Generate image metadata
						std::string meta = generateImageMeta(core, imgind);

						// Add image to the stream
						auto startSave = std::chrono::high_resolution_clock::now();
						core.addImage(handle.c_str(), imgSize, img, { i, j, k }, meta.c_str());
						auto endSave = std::chrono::high_resolution_clock::now();

						// Calculate statistics
						double imgSaveTimeMs = (endSave - startSave).count() / 1000000.0;
						double bw = imgSizeMb / (imgSaveTimeMs / 1000.0);
						std::cout << "Saved image " << imgind++ << " in ";
						std::cout << std::fixed << std::setprecision(2) << imgSaveTimeMs << " ms, size ";
						std::cout << std::fixed << std::setprecision(1) << imgSizeMb << " MB, BW: " << bw << " MB/s" << std::endl;
						vmeta.push_back(meta);
					}
				}
			}
		}

		// We are done so close the dataset
		core.stopSequenceAcquisition();
		core.closeDataset(handle.c_str());
	}
	catch(std::exception& e)
	{
		core.closeDataset(handle.c_str());
		throw;
	}
}

/**
 * Compare strings
 * @param stra String A
 * @parma strb String B
 * @return Are strings equal
 */
bool compareText(const std::string& stra, const std::string& strb) noexcept
{
	if(stra.size() != strb.size())
		return false;
	for(std::size_t i = 0; i < stra.size(); i++)
	{
		if(stra[i] != strb[i])
			return false;
	}
	return true;
}
