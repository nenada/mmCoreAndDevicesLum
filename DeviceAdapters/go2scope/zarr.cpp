///////////////////////////////////////////////////////////////////////////////
// FILE:          Go2Scope.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Zarr writer based on the CZI acquire-zarr library
//
// AUTHOR:        Nenad Amodaj
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
#include "go2scope.h"
#include "zarr.h"
#include "nlohmann/json.hpp"

using namespace std;

///////////////////////////////////////////////////////////////////////////////
// Zarr storage

AcqZarrStorage::AcqZarrStorage() :
   initialized(false)
{
   InitializeDefaultErrorMessages();

	// set device specific error messages
   SetErrorText(ERR_INTERNAL, "Internal driver error, see log file for details");

   auto ver = Zarr_get_api_version();
                                                                             
   // create pre-initialization properties                                   
   // ------------------------------------
   //
                                                                          
   // Name                                                                   
   CreateProperty(MM::g_Keyword_Name, g_AcqZarrStorage, MM::String, true);
   //
   // Description
   ostringstream os;
   os << "Acquire Zarr Storage v" << ver;
   CreateProperty(MM::g_Keyword_Description, os.str().c_str(), MM::String, true);
}                                                                            
                                                                             
AcqZarrStorage::~AcqZarrStorage()                                                            
{                                                                            
   Shutdown();
} 

void AcqZarrStorage::GetName(char* Name) const
{
   CDeviceUtils::CopyLimitedString(Name, g_AcqZarrStorage);
}  

int AcqZarrStorage::Initialize()
{
   if (initialized)
      return DEVICE_OK;

	int ret(DEVICE_OK);

   UpdateStatus();

   initialized = true;
   return DEVICE_OK;
}

int AcqZarrStorage::Shutdown()
{
   if (initialized)
   {
      initialized = false;
   }
   return DEVICE_OK;
}

// Never busy because all commands block
bool AcqZarrStorage::Busy()
{
   return false;
}

int AcqZarrStorage::Create(const char* path, const char* name, int numberOfDimensions, const int shape[], MM::StorageDataType pixType, const char* meta, char* handle)
{
   if (numberOfDimensions < 3)
   {
      return ERR_ZARR_NUMDIMS;
   }

   auto settings = ZarrStreamSettings_create();
   if (!settings)
   {
      LogMessage("Failed creating Zarr stream settings.");
      return ERR_ZARR_SETTINGS;
   }

   // set store
   ostringstream os;
   os << "path" << "/" << name;
   ZarrStatus status = ZarrStreamSettings_set_store(settings,
                                                    os.str().c_str(),
                                                    os.str().size(),
                                                    nullptr);
   if (status != ZarrStatus_Success)
   {
      LogMessage(getErrorMessage(status));
      return ERR_ZARR_SETTINGS;
   }

   // set data type
   status = ZarrStreamSettings_set_data_type(settings, (ZarrDataType)pixType);
   if (status != ZarrStatus_Success)
   {
      LogMessage(getErrorMessage(status));
      return ERR_ZARR_SETTINGS;
   }

   status = ZarrStreamSettings_reserve_dimensions(settings, numberOfDimensions);
   if (status != ZarrStatus_Success)
   {
      LogMessage(getErrorMessage(status));
      return ERR_ZARR_SETTINGS;
   }

   ZarrDimensionProperties dimPropsX;
   string nameX("x");
   dimPropsX.name = nameX.c_str();
   dimPropsX.bytes_of_name = nameX.size();
   dimPropsX.array_size_px = shape[0];

   ZarrDimensionProperties dimPropsY;
   string nameY("y");
   dimPropsY.name = nameY.c_str();
   dimPropsY.bytes_of_name = nameY.size();
   dimPropsY.array_size_px = shape[1];

   for (size_t i = 2; i < numberOfDimensions; i++)
   {
      ZarrDimensionProperties dimProps;
      ostringstream osd;
      osd << "dim-" << 1;
      dimProps.name = osd.str().c_str();
      dimProps.bytes_of_name = osd.str().size();
      dimProps.array_size_px = shape[i];
      dimProps.chunk_size_px = 1;
      dimProps.shard_size_chunks = 1;
      ZarrStatus status = ZarrStreamSettings_set_dimension(settings, i, &dimProps);
   }



   return DEVICE_OK;
}

int AcqZarrStorage::ConfigureDimension(const char* handle, int dimension, const char* name, const char* meaning)
{
   return 0;
}

int AcqZarrStorage::ConfigureCoordinate(const char* handle, int dimension, int coordinate, const char* name)
{
   return 0;
}

int AcqZarrStorage::Close(const char* handle)
{
   return 0;
}

int AcqZarrStorage::Load(const char* path, const char* name, char* handle)
{
   return 0;
}

int AcqZarrStorage::Delete(char* handle)
{
   return 0;
}

int AcqZarrStorage::List(const char* path, char** listOfDatasets, int maxItems, int maxItemLength)
{
   return 0;
}

int AcqZarrStorage::AddImage(const char* handle, unsigned char* pixels, int width, int height, int depth, int coordinates[], int numCoordinates, const char* imageMeta)
{
   return 0;
}

int AcqZarrStorage::GetSummaryMeta(const char* handle, char* meta, int bufSize)
{
   return 0;
}

int AcqZarrStorage::GetImageMeta(const char* handle, int coordinates[], int numCoordinates, char* meta, int bufSize)
{
   return 0;
}

const unsigned char* AcqZarrStorage::GetImage(const char* handle, int coordinates[], int numCoordinates)
{
   return nullptr;
}

int AcqZarrStorage::GetNumberOfDimensions(const char* handle, int& numDimensions)
{
   return 0;
}

int AcqZarrStorage::GetDimension(const char* handle, int dimension, char* name, int nameLength, char* meaning, int meaningLength)
{
   return 0;
}

int AcqZarrStorage::GetCoordinate(const char* handle, int dimension, int coordinate, char* name, int nameLength)
{
   return 0;
}

std::string AcqZarrStorage::getErrorMessage(int code)
{
   return std::string(Zarr_get_error_message((ZarrStatus)code));
}


///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

