///////////////////////////////////////////////////////////////////////////////
// FILE:          G2STiffFile.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Go2Scope devices. Includes the experimental StorageDevice
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
#define _LARGEFILE64_SOURCE
#include <sstream>
#include <filesystem>
#include <cstring>
#include "G2SBigTiffDataset.h"
#ifdef _WIN32
#include <Windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#endif

/**
 * Class constructor
 * Constructor doesn't open the file, just creates an object set sets the configuration
 * By convention G2S format files end with a .g2s.tif extension
 * First data chunk doesn't have a index (e.g. SampleDataset.g2s.tif)
 * Other data chunks contain an index (1-based, e.g. SampleDataset_1.g2s.tif, SampleDataset_2.g2s.tif..)
 * Dataset files are contained within a directory. The name of the directory matches the dataset name with the .g2s sufix (e.g. SampleDataset.g2s)
 */
G2SBigTiffDataset::G2SBigTiffDataset() noexcept
{
	dspath = "";
	datasetuid = "";
	bitdepth = 8;
	samples = 1;
	imgcounter = 0;
	flushcnt = 0;
	chunksize = 0;
	directIo = false;
	bigTiff = true;
	writemode = false;
}

/**
 * Create a dataset
 * If the file doesn't exist it will be created
 * If the file exists it will be discared and new one will be created
 * If the dataset is chunked files will be created only when the active chunk is filled
 * @param path Dataset (folder) path / File path of the first (only) data chunk
 * @param dio Use direct I/O
 * @param fbig Use BigTIFF format
 * @param chunksz Chunk size
 * @throws std::runtime_error
 */
void G2SBigTiffDataset::create(const std::string& path, bool dio, bool fbig, std::uint32_t chunksz)
{
	if(isOpen())
		throw std::runtime_error("Invalid operation. Dataset is already created");
	if(dspath.empty())
		throw std::runtime_error("Unable to create a file stream. Dataset path is undefined");
	directIo = dio;
	writemode = true;

	// Check dataset 
	int counter = 1;
	std::string ext(chunksize > 0 ? ".g2s.tif" : ".tif");
	std::filesystem::path basepath = std::filesystem::u8path(dspath);
	std::filesystem::path xpath = basepath;
	while(std::filesystem::exists(xpath))
	{
		// If the file path (path + name) exists, it should not be an error
		// nor the file should be overwritten, first available suffix (index) will be appended to the file name
		auto savePrefixTmp = savePrefix + "_" + std::to_string(counter++) + ext;
		dsName = saveRoot / savePrefixTmp;
	}	


	// Determine dataset name
	// Discard file extensions
	dsname = std::filesystem::u8path(dspath).filename().u8string();
	if(dsname.find(".tiff") == dsname.size() - 5)
		dsname = dsname.substr(0, dsname.size() - 5);
	else if(dsname.find(".tif") == dsname.size() - 4 || dsname.find(".tf8") == dsname.size() - 4)
		dsname = dsname.substr(0, dsname.size() - 4);
	if(dsname.find(".g2s") == dsname.size() - 4)
		dsname = dsname.substr(0, dsname.size() - 4);

	// Discard file index
	auto ix = dsname.find_last_of("_");
	if(ix != std::string::npos)
	{
		auto token = dsname.substr(ix + 1);
		bool isnum = !token.empty() && std::find_if(token.begin(), token.end(), [](unsigned char c) { return !std::isdigit(c); }) == token.end();
		if(isnum)
			dsname = dsname.substr(0, ix);
	}
	activechunk = std::make_shared<G2SBigTiffStream>(fp.u8string(), directIo);
	activechunk->open(true);
	datachunks.push_back(activechunk);
}

/**
 * Load a dataset
 * If the dataset doesn't exist an exception will be thrown
 * If the dataset exists dataset parameters and metadata will be parsed
 * If the dataset is chunked all files will be enumerated, but only the first file will be loaded
 * @param dio Use direct I/O
 * @param fbig Use BigTIFF format
 * @param chunksz Chunk size
 * @throws std::runtime_error
 */
void G2SBigTiffDataset::load(const std::string& path, bool dio)
{
	if(isOpen())
		throw std::runtime_error("Invalid operation. Dataset is already loaded");
	if(dspath.empty())
		throw std::runtime_error("Unable to load a dataset. Dataset path is undefined");
	directIo = dio;
	writemode = false;

	// Check dataset / file path
	auto fp = std::filesystem::u8path(dspath);
	if(!std::filesystem::exists(fp))
	{
		// Check if the file path has a valid extension
		std::string fpath(path);
		if(fpath.find(".tiff") != fpath.size() - 5 && fpath.find(".tif") != fpath.size() - 4 && fpath.find(".tf8") != fpath.size() - 4)
			fpath += ".tif";
		throw std::runtime_error("Unable to load a dataset. File doesn't exist");
	}
	else if(std::filesystem::is_directory(fp))
	{
		// Dataset folder is specified, enumerate files
	}
	else
	{
		// File path is specified, load first data chunk
		activechunk = std::make_shared<G2SBigTiffStream>(fp.u8string(), directIo);
		activechunk->open(false);

		samples = 1;
		imgcounter = 0;
		metadata.clear();
		activechunk->parse(datasetuid, shape, chunksize, metadata, bitdepth);
		datachunks.push_back(activechunk);

		// Correct dataset (folder) path
		std
		if(dspath.find(".tiff") == dspath.size() - 5)
			dspath = dspath.substr(0, dspath.size() - 5);
		else if(dspath.find(".tif") == dspath.size() - 4 || dspath.find(".tf8") == dspath.size() - 4)
			dspath = dspath.substr(0, dspath.size() - 4);
		if(dspath.find(".g2s") == dspath.size() - 4)
			dspath = dspath.substr(0, dspath.size() - 4);

		// Check for other files in the same directory
	}
}

/**
 * Close the dataset
 * If a dataset hasn't been created / loaded this method will have no effect
 * File handles will be released / closed
 * In the create mode during closing final section (dataset metadata) is commited to the first data chunk (file)
 */
void G2SBigTiffDataset::close() noexcept
{
	if(!datachunks.empty() && datachunks[0]->isOpen() && writemode)
		datachunks[0]->appendMetadata(metadata);
	for(const auto& fx : datachunks)
		fx->close();
	imgcounter = 0;
	bitdepth = 8;
	samples = 1;
	metadata.clear();
	shape.clear();
	datachunks.clear();
}

/**
 * Set dataset shape / dimension & axis sizes
 * First two axis are always width and height
 * If the shape info is invalid, or the dataset configuration has already been set
 * this method will take no effect
 * @param dims Axis sizes list
 * @throws std::runtime_error
 */
void G2SBigTiffDataset::setShape(const std::vector<std::uint32_t>& dims)
{
	if(dims.size() < 2)
		throw std::runtime_error("Unable to set dataset shape. Invalid shape info");
	if(!writemode)
		throw std::runtime_error("Unable to set dataset shape in read mode");
	if(imgcounter > 0 && shape.size() >= 2)
	{
		if(dims.size() != shape.size())
			throw std::runtime_error("Unable to set dataset shape. Invalid axis count");
		if(dims[dims.size() - 2] != shape[shape.size() - 2] || dims[dims.size() - 1] != shape[shape.size() - 1])
			throw std::runtime_error("Unable to set dataset shape. Image dimensions don't match the existing image dimensions");
	}
	shape = dims;
	
	// Write Shape info to the header cache
	for(const auto& fs : datachunks)
	{
		bool isopen = fs->isOpen();
		if(!isopen)
			fs->open(false);
		fs->writeShapeInfo(shape, chunksize);
		// TODO: Commit header only
		if(!isopen)
			fs->close();
	}
}

/**
 * Set dataset shape / dimension & axis sizes
 * First two axis are always width and height
 * If the shape info is invalid, or the dataset configuration has already been set
 * this method will take no effect
 * @param dims Axis sizes list
 * @throws std::runtime_error
 */
void G2SBigTiffDataset::setShape(std::initializer_list<std::uint32_t> dims)
{
	if(dims.size() < 2)
		throw std::runtime_error("Unable to set dataset shape. Invalid shape info");
	if(!writemode)
		throw std::runtime_error("Unable to set dataset shape in read mode");
	if(imgcounter > 0 && shape.size() >= 2)
	{
		if(dims.size() != shape.size())
			throw std::runtime_error("Unable to set dataset shape. Invalid axis count");
		if(*(dims.end() - 2) != shape[shape.size() - 2] || *(dims.end() - 1) != shape[shape.size() - 1])
			throw std::runtime_error("Unable to set dataset shape. Image dimensions don't match the existing image dimensions");
	}
	shape = dims;

	// Write Shape info to the header cache
	writeShapeInfo();
}

/**
 * Set pixel format
 * If the shape info is invalid, or the dataset configuration has already been set
 * this method will take no effect
 * @param depth Bit depth (bits per sample)
 * @parma vsamples Samples per pixel
 * @throws std::runtime_error
 */
void G2SBigTiffDataset::setPixelFormat(std::uint8_t depth, std::uint8_t vsamples)
{
	if(!writemode)
		throw std::runtime_error("Unable to set pixel format in read mode");
	if(imgcounter > 0)
	{
		if(bitdepth != depth || samples != vsamples)
			throw std::runtime_error("Unable to set pixel format. Specified pixel format doesn't match current pixel format");
	}
	bitdepth = depth;
	samples = vsamples;
}

/**
 * Set dataset metadata
 * Metadata will be stored in byte buffer whose size is 1 byte larger than the metadata string length
 * @param meta Metadata string
 */
void G2SBigTiffDataset::setMetadata(const std::string& meta) noexcept
{
	metadata.clear();
	if(meta.empty())
		return;
	metadata.resize(meta.size() + 1);
	std::copy(meta.begin(), meta.end(), metadata.begin());
}

/**
 * Set dataset UID
 * UID must be in a standard UUID format, 16-bytes long hex string with or without the dash delimiters: 
 * XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
 * @param val Dataset UID
 * @throws std::runtime_error
 */
void G2SBigTiffDataset::setUID(const std::string& val)
{
	if(val.empty())
	{
		datasetuid = val;
		return;
	}
	if(val.size() != 32 && val.size() != 36)
		throw std::runtime_error("Unable to set the dataset UID. Invalid UID format");
	auto hasdashes = val.size() == 36;
	if(hasdashes && (val[8] != '-' || val[13] != '-' || val[18] != '-' || val[23] != '-'))
		throw std::runtime_error("Unable to set the dataset UID. Invalid UID format");
	for(std::size_t i = 0; i < val.size(); i++)
	{
		if(hasdashes && (i == 8 || i == 13 || i == 18 || i == 23))
			continue;
		if(val[i] < 48 || val[i] > 102 || (val[i] > 57 && val[i] < 65) || (val[i] > 70 && val[i] < 97))
			throw std::runtime_error("Unable to set the dataset UID. Invalid UID format");
	}
	datasetuid = hasdashes ? val : val.substr(0, 8) + "-" + val.substr(8, 4) + "-" + val.substr(12, 4) + "-" + val.substr(16, 4) + "-" + val.substr(20);
	if(!header.empty())
	{
		// Write UID to the header cache
		auto startind = bigTiff ? 24 : 16;
		auto cind = 0;
		for(int i = 0; i < 16; i++)
		{
			if(i == 4 || i == 6 || i == 8 || i == 10)
				cind++;
			char cv1 = datasetuid[cind++];
			char cv2 = datasetuid[cind++];
			std::uint8_t vx1 = cv1 >= 48 && cv1 <= 57 ? cv1 - 48 : (cv1 >= 65 && cv1 <= 70 ? cv1 - 55 : cv1 - 87);
			std::uint8_t vx2 = cv2 >= 48 && cv2 <= 57 ? cv2 - 48 : (cv2 >= 65 && cv2 <= 70 ? cv2 - 55 : cv2 - 87);
			auto xval = (std::uint8_t)(((vx1 & 0x0f) << 4) | (vx2 & 0x0f));
			header[startind + i] = xval;
		}
	}
}

/**
 * Get dataset metadata
 * If metadata is specified value will be returned from cache, otherwise it will be read from a file stream
 * @return Metadata string
 * @throws std::runtime_error
 */
std::string G2SBigTiffDataset::getMetadata()
{
	// Check metadata cache
	if(metadata.empty())
		return "";
	std::string str(metadata.begin(), metadata.end() - 1);
	return str;
}

/**
 * Get image metadata
 * If the coordinates are not specified images are read sequentially, metadata for the current image 
 * will be returned, in which case the current image won't be changed
 * If no metadata is defined this method will return an empty string
 * If no images are defined this method will return an empty string
 * In the sequential mode the image IFD will be loaded if this method is called before getImage() (only for the first image)
 * For other images getImage() should always be called prior to calling getImageMetadata()
 * @param coord Image coordinates
 * @return Image metadata
 */
std::string G2SBigTiffDataset::getImageMetadata(const std::vector<std::uint32_t>& coord)
{
	if(!isOpen())
		throw std::runtime_error("Invalid operation. No open file stream available");
	if(imgcounter == 0)
		throw std::runtime_error("Invalid operation. No images available");
	
	// Select current image (IFD)
	if(!coord.empty())
	{
		auto ind = calcImageIndex(coord);
		if(ind >= ifdcache.size())
			throw std::runtime_error("Invalid operation. Invalid image coordinates");
		currentimage = ind;
		loadIFD(ifdcache[ind]);
	}
	else if(currentifd.empty())
		// Load IFD
		loadIFD(currentifdpos);

	// Check IFD tag count
	auto tagcount = readInt(&currentifd[0], bigTiff ? 8 : 2);
	if(tagcount == G2STIFF_TAG_COUNT_NOMETA)
		return "";

	// Obtain metadata OFFSET and length
	auto metatagind = (bigTiff ? 8 : 2) + G2STIFF_TAG_COUNT_NOMETA * (bigTiff ? 20 : 12);
	auto metalen = readInt(&currentifd[metatagind + 4], bigTiff ? 8 : 4);
	auto metaoffset = readInt(&currentifd[metatagind + (bigTiff ? 12 : 8)], bigTiff ? 8 : 4);
	if(metalen == 0 || metaoffset == 0)
		return "";
	if(metaoffset < currentifdpos)
		throw std::runtime_error("Unable to obtain image metadata. File is corrupted");

	// Copy metadata from the IFD
	auto roff = metaoffset - currentifdpos;
	auto strlen = roff + metalen > currentifd.size() ? currentifd.size() - roff - metalen : metalen - 1;
	std::string str(&currentifd[roff], &currentifd[roff + strlen]);
	return str;
}

/**
 * Add image / write image to the file
 * Images are added sequentially
 * Image data is stored uncompressed
 * Metadata is stored in plain text, after the pixel data
 * Image IFD is stored before pixel data
 * @param buff Image buffer
 * @param meta Image metadata (optional)
 * @throws std::runtime_error
 */
void G2SBigTiffDataset::addImage(const unsigned char* buff, std::size_t len, const std::string& meta)
{
	if(!isOpen())
		throw std::runtime_error("Invalid operation. No open file stream available");
	if(shape.size() < 2)
		throw std::runtime_error("Invalid operation. Dataset shape is not defined");
	if(!bigTiff && len > TIFF_MAX_BUFFER_SIZE)
		throw std::runtime_error("Invalid operation. Image data is too long");
	if(!bigTiff && meta.size() > TIFF_MAX_BUFFER_SIZE)
		throw std::runtime_error("Invalid operation. Metadata string is too large");

	// Check file size limits
	std::uint32_t tot = 0;
	calcDescSize(meta.empty() ? 0 : meta.size() + 1, getTagCount(meta), nullptr, nullptr, &tot);
	if(meta.size() + len + currpos + tot > getMaxFileSize())
		throw std::runtime_error("Invalid operation. File size limit exceeded");

	// Commit header if empty file
	if(writepos == 0)
	{
		commit(&header[0], header.size());
		lastifdpos = readInt(&header[bigTiff ? 8 : 4], bigTiff ? 8 : 4);
		configset = true;
	}
	// Update last IFD for images in read mode
	else if(lastifd.empty() && lastifdpos > 0)
	{
		// Move read cursor to the last IFD
		auto lreadpos = readpos;
		auto lwritepos = writepos;
		seek(lastifdpos);
		moveReadCursor(currpos);

		// Load last IFD and change the next IFD offset
		auto nextoff = parseIFD(lastifd, lastifdsize);
		if(nextoff == 0)
			writeInt(&lastifd[lastifdsize - (bigTiff ? 8 : 4)], bigTiff ? 8 : 4, writepos);

		// Update last IFD
		seek(lastifdpos);
		commit(&lastifd[0], lastifd.size());

		// Reset cursors
		moveReadCursor(lreadpos);
		moveWriteCursor(lwritepos);
	}

	// Reposition file cursor if last operation was a file read
	if(writepos != currpos)
		seek(writepos);

	// Compose next IFD and write image metadata
	appendIFD(len, meta);

	// Write pixel data
	commit(buff, len);

	// Add padding bytes
	auto alignsz = directIo ? ssize : 2;
	if(len % alignsz != 0)
	{
		auto padsize = len - (len / alignsz) * alignsz;
		std::vector<unsigned char> pbuff(padsize);
		commit(&pbuff[0], pbuff.size());
	}

	// Flush pending data
	ifdcache.push_back(lastifdpos);
	imgcounter++;
	if(flushcnt > 0 && imgcounter % flushcnt == 0)
		flush();
}

/**
 * Get image data (pixel buffer)
 * If the coordinates are not specified images are read sequentially
 * This method will change (advance) the current image
 * If this method is called after the last available image (in sequential mode), or with invalid coordinates an exception will be thrown
 * @param coord Image coordinates
 * @return Image data
 */
std::vector<unsigned char> G2SBigTiffDataset::getImage(const std::vector<std::uint32_t>& coord)
{
	if(!isOpen())
		throw std::runtime_error("Invalid operation. No open file stream available");
	if(imgcounter == 0 || (currentimage + 1 > imgcounter) || nextifdpos == 0)
		throw std::runtime_error("Invalid operation. No images available");

	// Select current image (IFD)
	if(!coord.empty())
	{
		auto ind = calcImageIndex(coord);
		if(ind >= ifdcache.size())
			throw std::runtime_error("Invalid operation. Invalid image coordinates");
		currentimage = ind;
		loadIFD(ifdcache[ind]);
	}
	else
	{
		// Clear current IFD before advancing
		// In a case where getImageMetadata() is called before any getImage() call
		// we should skip clearing of current IFD, this works only for the first image
		if(currentimage > 0)
		{
			currentifd.clear();
			currentifdpos = nextifdpos;
		}

		// Advance current image
		currentimage++;

		// Load IFD (skip if already loaded by the getImageMetadata())
		if(currentifd.empty())
			loadNextIFD();
	}

	// Obtain pixel data strip locations
	auto offind = (bigTiff ? 8 : 2) + 5 * (bigTiff ? 20 : 12);
	auto lenind = (bigTiff ? 8 : 2) + 7 * (bigTiff ? 20 : 12);
	auto dataoffset = readInt(&currentifd[offind + (bigTiff ? 12 : 8)], bigTiff ? 8 : 4);
	auto datalen = readInt(&currentifd[lenind + (bigTiff ? 12 : 8)], bigTiff ? 8 : 4);
	if(dataoffset == 0 || datalen == 0)
		return {};

	std::vector<unsigned char> ret(datalen);
	moveReadCursor(seek(dataoffset));
	fetch(&ret[0], ret.size());
	return ret;
}

/**
 * Calculate image index from image coordinates
 * Image coordiantes should not contain indices for the last two dimensions (width & height)
 * By convention image acquisitions loops through the coordinates in the descending order (higher coordinates are looped first)
 * E.g. ZTC order means that all channels are acquired before changing the time point, and all specified time points 
 * are acquired before moving the Z-stage, in which case dataset with the shape 2-4-3 for coordinates 1-2-1 will return 19 (=1*12 + 2*3 + 1*1)
 * First image coordinate can go beyond the specified shape size
 * @param coord Image coordinates
 * @return Image index
 * @throws std::runtime_error
 */
std::uint32_t G2SBigTiffDataset::calcImageIndex(const std::vector<std::uint32_t>& coord) const
{
	// Validate coordinates count
	if(coord.size() > shape.size() - 2)
		throw std::runtime_error("Invalid number of coordinates");

	// Validate ranges for all axis (except the first)
	for(std::size_t i = 1; i < coord.size(); i++)
	{
		if(coord[i] >= shape[i])
			throw std::runtime_error("Invalid coordinate for dimension " + std::to_string(i + 2));
	}

	// Calculate image index by 
	std::uint32_t ind = 0;
	for(int i = 0; i < coord.size(); i++)
	{
		if(coord[i] == 0)
			continue;
		std::uint32_t sum = 1;
		for(int j = i + 1; j < shape.size() - 2; j++)
			sum *= shape[j];
		ind += sum * coord[i];
	}
	return ind;
}
