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
 * @parma imglimit Image count limit
 * @throws std::runtime_error
 */
void runAcquisition(CMMCore& core, const std::string& handle, int imgSize, int c, int t, int p, std::chrono::steady_clock::time_point& startAcq, std::vector<std::string>& vmeta, int imglimit = 0)
{
	try
	{
		int imgind = 0;
		bool limitreach = false;
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

					// Check image count limit
					if(imglimit > 0 && imgind >= imglimit)
					{
						limitreach = true;
						break;
					}
				}
				if(limitreach)
					break;
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
						
						// Check image count limit
						if(imglimit > 0 && imgind >= imglimit)
						{
							limitreach = true;
							break;
						}
					}
					if(limitreach)
						break;
				}
				if(limitreach)
					break;
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
 * Configure dataset axis info
 * @param core MM Core instance
 * @param handle Dataset UID
 * @param axisnames Axis names list [out]
 * @param axisdesc Axis descriptions list [out]
 * @param axiscoords Axis coordinates list [out]
 * @throws std::runtime_error
 */
void configureAxisInfo(CMMCore& core, const std::string& handle, const std::vector<std::string>& axisnames, const std::vector<std::string>& axisdesc, const std::vector<std::vector<std::string>>& axiscoords)
{
	for(int i = 0; i < (int)axisnames.size(); i++)
	{
		core.configureDimension(handle.c_str(), i, axisnames[i].c_str(), axisdesc[i].c_str());
		if(i < (int)axiscoords.size())
		{
			for(int j = 0; j < axiscoords[i].size(); j++)
				core.configureCoordinate(handle.c_str(), i, j, axiscoords[i][j].c_str());
		}
	}
}

/**
 * Fill dataset axis info
 * @param shape Dataset shape
 * @param axisnames Axis names list [out]
 * @param axisdesc Axis descriptions list [out]
 * @param axiscoords Axis coordinates list [out]
 * @throws std::runtime_error
 */
void fillAxisInfo(const std::vector<long>& shape, std::vector<std::string>& axisnames, std::vector<std::string>& axisdesc, std::vector<std::vector<std::string>>& axiscoords)
{
	axiscoords.resize(shape.size() - 2);
	if(shape.size() == 5)
	{
		axisnames = { "P", "T", "C", "Y", "X" };	
		axisdesc = { "XY Position", "Time point", "Image channel", "Image height", "Image width" };
		for(int i = 0; i < 3; i++)
			axiscoords[i].resize(shape[i]);
		for(int i = 0; i < shape[0]; i++)
			axiscoords[0][i] = "Position" + std::to_string(i);
	}
	else
	{
		axisnames = { "T", "C", "Y", "X" };
		axisdesc = { "Time point", "Image channel", "Image height", "Image width" };
		for(int i = 0; i < 2; i++)
			axiscoords[i].resize(shape[i]);
	}

	int t = shape.size() == 5 ? shape[1] : shape[0];
	int c = shape.size() == 5 ? shape[2] : shape[1];
	for(int i = 0; i < t; i++)
		axiscoords[shape.size() < 5 ? 0 : 1][i] = "T" + std::to_string(i);
	for(int i = 0; i < c; i++)
		axiscoords[shape.size() < 5 ? 1 : 2][i] = "Channel" + std::to_string(i);
}

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
void validateDataset(CMMCore& core, const std::string& handle, const std::string& acqhandle, const std::vector<long>& acqshape, const std::string& acqmeta, const std::vector<std::string>& acqimgmeta, int expimgcount = 0)
{	
	// Validate UID
	if(handle != acqhandle)
		throw std::runtime_error("Dataset integrity check failed. Dataset UID missmatch");
	std::cout << "Dataset UID: " << handle << std::endl;

	// Validate shape
	std::vector<long> shape = core.getDatasetShape(handle.c_str());
	if(shape.size() != acqshape.size())
		throw std::runtime_error("Dataset integrity check failed. Dataset shape dimension missmatch");
	for(std::size_t i = 1; i < shape.size(); i++)
	{
		if(shape[i] != acqshape[i])
			throw std::runtime_error("Dataset integrity check failed. Dataset axis dimension missmatch, axis " + std::to_string(i));
	}
	int w = shape[shape.size() - 1];
	int h = shape[shape.size() - 2];
	int c = shape[shape.size() - 3];
	int t = shape[shape.size() - 4];
	int p = shape.size() > 4 ? shape[0] : 0;
	int shapeimgcount = expimgcount == 0 ? c * t * (p == 0 ? 1 : p) : expimgcount;
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
		if(i > 0 && (std::size_t)shape[i] != dcoord[i].size())
			throw std::runtime_error("Dataset integrity check failed. Axis coordinate vector size missmatch");
		for(long j = 0; j < dcoord[i].size(); j++)
		{
			std::string zval = core.getCoordinateName(handle.c_str(), (int)i, (int)j);
			if(!compareText(zval, dcoord[i][j]))
				throw std::runtime_error("Dataset integrity check failed. Axis coordinate name missmatch, axis " + std::to_string(i) + ", coordinate " + std::to_string(j));
		}
		std::cout << "Axis " << i << xval << " (" << yval << "), " << dcoord[i].size() << " coordinates" << std::endl;
	}
}
