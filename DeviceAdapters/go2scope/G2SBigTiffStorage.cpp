///////////////////////////////////////////////////////////////////////////////
// FILE:          G2SBigTiffStorage.cpp
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
#include "G2STiffFile.h"
#include <filesystem>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include "G2SBigTiffStorage.h"

/**
 * Default class constructor
 */
G2SBigTiffStorage::G2SBigTiffStorage() : initialized(false)
{
   supportedFormats = { "tif", "tiff", "tf8" };
	directIo = true;

   InitializeDefaultErrorMessages();

   // set device specific error messages
   SetErrorText(ERR_INTERNAL, "Internal driver error, see log file for details");
   SetErrorText(ERR_TIFF, "Generic TIFF error. See log for more info.");

   // create pre-initialization properties                                   
   // ------------------------------------
   //
   
   // Name                                                                   
   CreateProperty(MM::g_Keyword_Name, g_BigTiffStorage, MM::String, true);
   //
   // Description
   std::ostringstream os;
   os << "BigTIFF Storage v1.0";
   CreateProperty(MM::g_Keyword_Description, os.str().c_str(), MM::String, true);
}

/**
 * Get device name
 * @param Name String buffer [out]
 */
void G2SBigTiffStorage::GetName(char* Name) const
{
   CDeviceUtils::CopyLimitedString(Name, g_MMV1Storage);
}

/**
 * Device driver initialization routine
 */
int G2SBigTiffStorage::Initialize()
{
   if(initialized)
      return DEVICE_OK;

   UpdateStatus();

   initialized = true;
   return DEVICE_OK;
}

/**
 * Device driver shutdown routine
 * During device shutdown cache will be emptied, 
 * and all open file handles will be closed
 */
int G2SBigTiffStorage::Shutdown()
{
   initialized = false;
   for(auto it = cache.begin(); it != cache.end(); it++)
   {
      if(it->second.isOpen())
		{
			auto fs = reinterpret_cast<G2STiffFile*>(it->second.FileHandle);
			fs->close();
			it->second.close();
			delete fs;
		}
   }
   cache.clear();
   return DEVICE_OK;
}

/**
 * Create storage entry
 * Dataset storage descriptor will open a file handle, to close a file handle call Close()
 * Dataset storage descriptor will reside in device driver cache
 * If the file already exists this method will fail with 'DEVICE_DUPLICATE_PROPERTY' status code
 * @param path Absolute file path (TIFF file)
 * @param name Dataset name
 * @param numberOfDimensions Number of dimensions
 * @param shape Axis sizes
 * @param pixType Pixel format
 * @param meta Metadata
 * @param handle Entry GUID [out]
 * @return Status code
 */
int G2SBigTiffStorage::Create(const char* path, const char* name, int numberOfDimensions, const int shape[], MM::StorageDataType pixType, const char* meta, char* handle)
{
   if(path == nullptr || numberOfDimensions <= 0)
      return DEVICE_INVALID_INPUT_PARAM;

   // Check cache size limits
   if(cache.size() >= MAX_CACHE_SIZE)
   {
      cacheReduce();
      if(CACHE_HARD_LIMIT && cache.size() >= MAX_CACHE_SIZE)
         return DEVICE_OUT_OF_MEMORY;
   }

   // Check if the file already exists
   if(std::filesystem::exists(std::filesystem::u8path(path)))
      return DEVICE_DUPLICATE_PROPERTY;
	
	// Create dataset storage descriptor
	std::string guid = boost::lexical_cast<std::string>(boost::uuids::random_generator()());           // Entry UUID
	if(guid.size() > MM::MaxStrLength)
		return DEVICE_INVALID_PROPERTY_LIMTS;
   
   // Create a file on disk and store the file handle
   auto fhandle = new G2STiffFile(path);
   if(fhandle == nullptr)
      return DEVICE_OUT_OF_MEMORY;
	
	try
	{
		fhandle->open(true, directIo);
		if(!fhandle->isOpen())
			return DEVICE_OUT_OF_MEMORY;
	}
	catch(std::exception&)
	{
		delete fhandle;
		return DEVICE_OUT_OF_MEMORY;
	}
   
   G2SStorageEntry sdesc(path, name, numberOfDimensions, shape, meta);
   sdesc.FileHandle = fhandle;

	// Set dataset UUID / shape / metadata
	std::vector<std::uint32_t> vshape;
	vshape.assign(shape, shape + numberOfDimensions);
	fhandle->setUID(guid);
	fhandle->setShape(vshape);
	fhandle->setMetadata(meta);

   // Append dataset storage descriptor to cache
   auto it = cache.insert(std::make_pair(guid, sdesc));
   if(!it.second)
	{
		delete fhandle;
      return DEVICE_OUT_OF_MEMORY;
	}

   // Copy UUID string to the GUID buffer
   guid.copy(handle, guid.size());
   return DEVICE_OK;
}

/**
 * Load dataset from disk
 * Dataset storage descriptor will be read from file
 * Dataset storage descriptor will open a file handle, to close a file handle call Close()
 * Dataset storage descriptor will reside in device driver cache
 * @param path Absolute file path (TIFF file)
 * @param name Dataset name
 * @param handle Entry GUID [out]
 * @return Status code
 */
int G2SBigTiffStorage::Load(const char* path, const char* name, char* handle)
{
   if(path == nullptr)
      return DEVICE_INVALID_INPUT_PARAM;

   // Check if the file exists
   if(!std::filesystem::exists(std::filesystem::u8path(path)))
      return DEVICE_INVALID_INPUT_PARAM;

   // Open a file on disk and store the file handle
	auto fhandle = new G2STiffFile(path);
	if(fhandle == nullptr)
		return DEVICE_OUT_OF_MEMORY;

	try
	{
		fhandle->open(false, directIo);
		if(!fhandle->isOpen())
			return DEVICE_OUT_OF_MEMORY;
	}
	catch(std::exception&)
	{
		delete fhandle;
		return DEVICE_OUT_OF_MEMORY;
	}
	
	// Obtain / generate dataset UID
   std::string guid = fhandle->getUID().empty() ? boost::lexical_cast<std::string>(boost::uuids::random_generator()()) : fhandle->getUID();
	if(guid.size() > MM::MaxStrLength)
	{
		delete fhandle;
		return DEVICE_INVALID_PROPERTY_LIMTS;
	}

	// Create dataset storage descriptor
   G2SStorageEntry sdesc(path, name, (int)fhandle->getDimension(), reinterpret_cast<int*>(&fhandle->getShape()[0]), fhandle->getMetadata().empty() ? nullptr : fhandle->getMetadata().c_str());
   sdesc.FileHandle = fhandle;

   // Append dataset storage descriptor to cache
   auto it = cache.insert(std::make_pair(guid, sdesc));
   if(!it.second)
	{
		delete fhandle;
      return DEVICE_OUT_OF_MEMORY;
	}

   // Copy UUID string to the GUID buffer
   guid.copy(handle, guid.size());
   return DEVICE_OK;
}

int G2SBigTiffStorage::GetShape(const char* handle, int shape[])
{
   // TODO:
   // shape must have the size that includes x and y
   return DEVICE_NOT_YET_IMPLEMENTED;
}

int G2SBigTiffStorage::GetDataType(const char* handle, MM::StorageDataType& pixelDataType)
{
   // TODO:
   // in the case where the data type in file is not supported, return MM::StorageDataType_UNKNOWN

   return DEVICE_NOT_YET_IMPLEMENTED;
}

/**
 * Close the dataset
 * File handle will be closed
 * Metadata will be discarded
 * Storage entry descriptor will remain in cache
 * @param handle Entry GUID
 * @return Status code
 */
int G2SBigTiffStorage::Close(const char* handle)
{
   auto it = cache.find(handle);
   if(it == cache.end())
      return DEVICE_INVALID_INPUT_PARAM;
   if(it->second.isOpen())
   {
		auto fs = reinterpret_cast<G2STiffFile*>(it->second.FileHandle);
		fs->close();
		it->second.close();
		delete fs;
   }
   return DEVICE_OK;
}

/**
 * Delete existing dataset (file on disk)
 * If the file doesn't exist this method will return 'DEVICE_NO_PROPERTY_DATA' status code
 * Dataset storage descriptor will be removed from cache
 * @param handle Entry GUID
 * @return Status code
 */
int G2SBigTiffStorage::Delete(char* handle)
{
   if(handle == nullptr)
      return DEVICE_INVALID_INPUT_PARAM;
   auto it = cache.find(handle);
   if(it == cache.end())
      return DEVICE_INVALID_INPUT_PARAM;

   // Check if the file exists
   auto fp = std::filesystem::u8path(it->second.Path);
   if(!std::filesystem::exists(fp))
      return DEVICE_NO_PROPERTY_DATA;

   // Close the file handle
   if(it->second.isOpen())
   {
		auto fs = reinterpret_cast<G2STiffFile*>(it->second.FileHandle);
		fs->close();
		it->second.close();
		delete fs;
   }
   
   // Delete the file
   if(!std::filesystem::remove(fp))
      return DEVICE_ERR;

   // Discard the cache entry
   cache.erase(it);
   return DEVICE_OK;
}

/**
 * List datasets in the specified folder / path
 * If the list of found datasets is longer than 'maxItems' only first [maxItems] will be 
 * returned and 'DEVICE_SEQUENCE_TOO_LARGE' status code will be returned
 * If the dataset path is longer than 'maxItemLength' dataset path will be truncated
 * If the specified path doesn't exist, or it's not a valid folder path 'DEVICE_INVALID_INPUT_PARAM' status code will be returned
 * @param path Folder path
 * @param listOfDatasets Dataset path list [out]
 * @param maxItems Max dataset count
 * @param maxItemLength Max dataset path length
 * @return Status code
 */
int G2SBigTiffStorage::List(const char* path, char** listOfDatasets, int maxItems, int maxItemLength)
{
   if(path == nullptr || listOfDatasets == nullptr || maxItems <= 0 || maxItemLength <= 0)
      return DEVICE_INVALID_INPUT_PARAM;
   auto dp = std::filesystem::u8path(path);
   if(!std::filesystem::exists(dp) || !std::filesystem::is_directory(dp))
      return DEVICE_INVALID_INPUT_PARAM;
   auto allfnd = scanDir(path, listOfDatasets, maxItems, maxItemLength, 0);
   return allfnd ? DEVICE_OK : DEVICE_SEQUENCE_TOO_LARGE;
}

/**
 * Add image / write image to file
 * Image metadata will be stored in cache
 * @param handle Entry GUID
 * @param pixels Pixel data buffer
 * @param sizeInBytes pixel array size
 * @param coordinates Image coordinates
 * @param numCoordinates Coordinate count
 * @param imageMeta Image metadata
 * @return Status code
 */
int G2SBigTiffStorage::AddImage(const char* handle, int sizeInBytes, unsigned char* pixels, int coordinates[], int numCoordinates, const char* imageMeta)
{
	if(handle == nullptr || pixels == nullptr || sizeInBytes <= 0 || numCoordinates <= 0 || sizeof(coordinates) != numCoordinates * sizeof(int))
		return DEVICE_INVALID_INPUT_PARAM;

	// Obtain dataset descriptor from cache
	auto it = cache.find(handle);
	if(it == cache.end())
		return DEVICE_INVALID_INPUT_PARAM;

	// Validate image dimensions
	auto fs = reinterpret_cast<G2STiffFile*>(it->second.FileHandle);
	if((std::size_t)numCoordinates != fs->getDimension())
		return DEVICE_INVALID_INPUT_PARAM;
	if(fs->getImageCount() == 0)
		fs->setPixelFormat((std::uint8_t) fs->getBpp());

   for(int i = 0; i < numCoordinates; i++)
	{
		if(coordinates[i] < 0 || coordinates[i] >= (int)fs->getShape()[i + 2])
			return DEVICE_INVALID_INPUT_PARAM;
	}

	// Add image
	fs->addImage(pixels, sizeInBytes, imageMeta);
   return DEVICE_OK;
}

/**
 * Get dataset summary metadata
 * If the netadata size is longer than the provided buffer, only the first [bufSize] bytes will be
 * copied, and the status code 'DEVICE_SEQUENCE_TOO_LARGE' will be returned
 * @param handle Entry GUID
 * @param meta Metadata buffer [out]
 * @param bufSize Buffer size
 * @return Status code
 */
int G2SBigTiffStorage::GetSummaryMeta(const char* handle, char* meta, int bufSize)
{
   if(handle == nullptr || meta == nullptr || bufSize <= 0)
      return DEVICE_INVALID_INPUT_PARAM;

	// Obtain dataset descriptor from cache
   auto it = cache.find(handle);
   if(it == cache.end())
      return DEVICE_INVALID_INPUT_PARAM;

	// Copy metadata string
   it->second.Metadata.copy(meta, bufSize);
   return it->second.Metadata.size() > (std::size_t)bufSize ? DEVICE_SEQUENCE_TOO_LARGE : DEVICE_OK;
}

/**
 * Get dataset image metadata
 * If the netadata size is longer than the provided buffer, only the first [bufSize] bytes will be
 * copied, and the status code 'DEVICE_SEQUENCE_TOO_LARGE' will be returned
 * @param handle Entry GUID
 * @param coordinates Image coordinates
 * @param numCoordinates Coordinate count
 * @param meta Metadata buffer [out]
 * @param bufSize Buffer size
 * @return Status code
 */
int G2SBigTiffStorage::GetImageMeta(const char* handle, int coordinates[], int numCoordinates, char* meta, int bufSize)
{
   if(handle == nullptr || coordinates == nullptr || numCoordinates == 0 || meta == nullptr || bufSize <= 0 || sizeof(coordinates) != numCoordinates * sizeof(int))
      return DEVICE_INVALID_INPUT_PARAM;

	// Obtain dataset descriptor from cache
   auto it = cache.find(handle);
   if(it == cache.end())
      return DEVICE_INVALID_INPUT_PARAM;
   auto ikey = getImageKey(coordinates, numCoordinates);
   auto iit = it->second.ImageIndex.find(ikey);
   if(iit == it->second.ImageIndex.end())
      return DEVICE_INVALID_INPUT_PARAM;
   auto iind = iit->second;
   if(iind >= it->second.ImageMetadata.size())
      return DEVICE_INVALID_INPUT_PARAM;
   if(it->second.ImageMetadata[iind].size() > 0)
      it->second.ImageMetadata[iind].copy(meta, bufSize);
   return DEVICE_OK;
}

/**
 * Get image / pixel data
 * Image buffer will be created inside this method, so
 * object (buffer) destruction becomes callers responsibility
 * @param handle Entry GUID
 * @param coordinates Image coordinates
 * @param numCoordinates Coordinate count
 * @return Pixel buffer pointer
 */
const unsigned char* G2SBigTiffStorage::GetImage(const char* handle, int coordinates[], int numCoordinates)
{
	if(handle == nullptr || numCoordinates <= 0 || sizeof(coordinates) != numCoordinates * sizeof(int))
		return nullptr;

	// Obtain dataset descriptor from cache
	auto it = cache.find(handle);
	if(it == cache.end())
		return nullptr;

	// TODO:
   return nullptr;
}

/**
 * Configure metadata for a given dimension
 * @param handle Entry GUID
 * @param dimension Dimension index
 * @param name Name of the dimension
 * @param meaning Z,T,C, etc. (physical meaning)
 * @return Status code
 */
int G2SBigTiffStorage::ConfigureDimension(const char* handle, int dimension, const char* name, const char* meaning)
{
   if(handle == nullptr || dimension < 0)
      return DEVICE_INVALID_INPUT_PARAM;
   auto it = cache.find(handle);
   if(it == cache.end())
      return DEVICE_INVALID_INPUT_PARAM;
   if((std::size_t)dimension >= it->second.getDimSize())
      return DEVICE_INVALID_INPUT_PARAM;
   it->second.Dimensions[dimension].Name = std::string(name);
   it->second.Dimensions[dimension].Metadata = std::string(meaning);
   return DEVICE_OK;
}

/**
 * Configure a particular coordinate name. e.g. channel name / position name ...
 * @param handle Entry GUID
 * @param dimension Dimension index
 * @param coordinate Coordinate index
 * @param name Coordinate name
 * @return Status code
 */
int G2SBigTiffStorage::ConfigureCoordinate(const char* handle, int dimension, int coordinate, const char* name)
{
   if(handle == nullptr || dimension < 0 || coordinate < 0)
      return DEVICE_INVALID_INPUT_PARAM;
   auto it = cache.find(handle);
   if(it == cache.end())
      return DEVICE_INVALID_INPUT_PARAM;
   if((std::size_t)dimension >= it->second.getDimSize())
      return DEVICE_INVALID_INPUT_PARAM;
   if((std::size_t)coordinate >= it->second.Dimensions[dimension].getSize())
      return DEVICE_INVALID_INPUT_PARAM;
   it->second.Dimensions[dimension].Coordinates[coordinate] = std::string(name);
   return DEVICE_OK;
}

/**
 * Get number of dimensions
 * @param handle Entry GUID
 * @param numDimensions Number of dimensions [out]
 * @return Status code
 */
int G2SBigTiffStorage::GetNumberOfDimensions(const char* handle, int& numDimensions)
{
   if(handle == nullptr)
      return DEVICE_INVALID_INPUT_PARAM;
   auto it = cache.find(handle);
   if(it == cache.end())
      return DEVICE_INVALID_INPUT_PARAM;
   numDimensions = (int)it->second.getDimSize();
   return DEVICE_OK;
}

/**
 * Get number of dimensions
 * @param handle Entry GUID
 * @return Status code
 */
int G2SBigTiffStorage::GetDimension(const char* handle, int dimension, char* name, int nameLength, char* meaning, int meaningLength)
{
   if(handle == nullptr || dimension < 0 || meaningLength <= 0)
      return DEVICE_INVALID_INPUT_PARAM;
   auto it = cache.find(handle);
   if(it == cache.end())
      return DEVICE_INVALID_INPUT_PARAM;
   if((std::size_t)dimension >= it->second.getDimSize())
      return DEVICE_INVALID_INPUT_PARAM;
   if(it->second.Dimensions[dimension].Name.size() > (std::size_t)nameLength)
      return DEVICE_INVALID_PROPERTY_LIMTS;
   if(it->second.Dimensions[dimension].Metadata.size() > (std::size_t)meaningLength)
      return DEVICE_INVALID_PROPERTY_LIMTS;
   it->second.Dimensions[dimension].Name.copy(name, it->second.Dimensions[dimension].Name.size());
   it->second.Dimensions[dimension].Metadata.copy(meaning, it->second.Dimensions[dimension].Metadata.size());
   return DEVICE_OK;
}

/**
 * Get number of dimensions
 * @param handle Entry GUID
 * @return Status code
 */
int G2SBigTiffStorage::GetCoordinate(const char* handle, int dimension, int coordinate, char* name, int nameLength)
{
   if(handle == nullptr || dimension < 0 || coordinate < 0 || nameLength <= 0)
      return DEVICE_INVALID_INPUT_PARAM;
   auto it = cache.find(handle);
   if(it == cache.end())
      return DEVICE_INVALID_INPUT_PARAM;
   if((std::size_t)dimension >= it->second.getDimSize())
      return DEVICE_INVALID_INPUT_PARAM;
   if((std::size_t)coordinate >= it->second.Dimensions[dimension].getSize())
      return DEVICE_INVALID_INPUT_PARAM;
   if(it->second.Dimensions[dimension].Coordinates[coordinate].size() > (std::size_t)nameLength)
      return DEVICE_INVALID_PROPERTY_LIMTS;
   it->second.Dimensions[dimension].Coordinates[coordinate].copy(name, it->second.Dimensions[dimension].Coordinates[coordinate].size());
   return DEVICE_OK;
}

/**
 * Discard closed dataset storage descriptors from cache
 * By default storage descriptors are preserved even after the dataset is closed
 * To reclaim memory all closed descritors are evicted from cache
 */
void G2SBigTiffStorage::cacheReduce() noexcept
{
   for(auto it = cache.begin(); it != cache.end(); )
   {
      if(!it->second.isOpen())
         it = cache.erase(it);
      else
         it++;
   }
}

/**
 * Scan folder subtree for supported files
 * @paramm path Folder path
 * @param listOfDatasets Dataset path list [out]
 * @param maxItems Max dataset count
 * @param maxItemLength Max dataset path length
 * @param cpos Current position in the list
 * @return Was provided buffer large enough to store all dataset paths
 */
bool G2SBigTiffStorage::scanDir(const std::string& path, char** listOfDatasets, int maxItems, int maxItemLength, int cpos) noexcept
{
   try
   {
      auto dp = std::filesystem::u8path(path);
      if(!std::filesystem::exists(dp))
         return true;
      auto dit = std::filesystem::directory_iterator(dp);
      if(!std::filesystem::is_directory(dp))
         return false;
      for(const auto& entry : dit)
      {
         // Skip auto folder paths
         auto fname = entry.path().filename().u8string();
         if(fname == "." || fname == "..")
            continue;
         auto abspath = std::filesystem::absolute(entry).u8string();

         // Scan subfolder
         if(std::filesystem::is_directory(entry))
            return scanDir(abspath, listOfDatasets, maxItems, maxItemLength, cpos);
         
         // Skip unsupported file formats
         auto fext = entry.path().extension().u8string();
         if(fext.size() == 0)
            continue;
         if(fext[0] == '.')
            fext = fext.substr(1);
         std::transform(fext.begin(), fext.end(), fext.begin(), [](char c) { return (char)tolower(c); });
         if(std::find(supportedFormats.begin(), supportedFormats.end(), fext) == supportedFormats.end())
            continue;

         // We found a supported file type
         // Check result buffer limit
         if(cpos >= maxItems)
            return false;

         // Add to results list
         abspath.copy(listOfDatasets[cpos], maxItemLength);
         cpos++;
      }
      return true;
   }
   catch(std::filesystem::filesystem_error&)
   {
      return false;
   }
}

/**
 * Calculate image key from the specified image coordinates
 * @param coordinates Image coordinates
 * @param numCoordinates Coordinate count
 * @return Image key (for image cache indices)
 */
std::string G2SBigTiffStorage::getImageKey(int coordinates[], int numCoordinates) noexcept
{
   std::stringstream ss;
   for(int i = 0; i < numCoordinates; i++)
      ss << (i == 0 ? "" : "_") << coordinates[i];
   return ss.str();
}