/*
* Copyright (c) 2017, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/
//!
//! \file      cm_surface_manager.cpp  
//! \brief     Contains Class CmSurfaceManager  definitions  
//!

#include "cm_surface_manager.h"

#include "cm_debug.h"
#include "cm_queue_rt.h"
#include "cm_buffer_rt.h"
#include "cm_mem.h"
#include "cm_state_buffer.h"
#include "cm_surface_2d_up_rt.h"
#include "cm_surface_2d_rt.h"
#include "cm_surface_3d_rt.h"
#include "cm_surface_vme.h"
#include "cm_surface_sampler.h"
#include "cm_surface_sampler8x8.h"
#include "cm_device_rt.h"

namespace CMRT_UMD
{
int32_t CmSurfaceManager::UpdateStateForDelayedDestroy(SURFACE_DESTROY_KIND destroyKind, uint32_t index)
{
    switch (destroyKind)
    {
        case DELAYED_DESTROY:
        case GC_DESTROY:
            if (!m_surfaceReleased[index] ||
                m_surfaceStates[index])
            {
                return CM_SURFACE_IN_USE;
            }
            break;

        case APP_DESTROY:
            m_surfaceReleased[index] = true;
            if (m_surfaceStates[index])
            {
                return CM_SURFACE_IN_USE;
            }
            break;

        default:
            CM_ASSERTMESSAGE("Error: Invalid surface destroy kind.");
            return CM_FAILURE;
    }
    
    return CM_SUCCESS;
}

int32_t CmSurfaceManager::UpdateStateForRealDestroy(uint32_t index, CM_ENUM_CLASS_TYPE surfaceType)
{
    m_surfaceReleased[index] = false;
    m_surfaceArray[index] = nullptr;

    m_surfaceSizes[index] = 0;
    
    switch (surfaceType)
    {
    case CM_ENUM_CLASS_TYPE_CMBUFFER_RT:
        m_bufferCount--;
        break;
    case CM_ENUM_CLASS_TYPE_CMSURFACE2D:
        m_2DSurfaceCount--;
        break;
    case CM_ENUM_CLASS_TYPE_CMSURFACE2DUP: 
        m_2DUPSurfaceCount--;
        break;
    case CM_ENUM_CLASS_TYPE_CMSURFACE3D:
        m_3DSurfaceCount--;
        break;
    case CM_ENUM_CLASS_TYPE_CMSURFACESAMPLER:
    case CM_ENUM_CLASS_TYPE_CMSURFACESAMPLER8X8:
    case CM_ENUM_CLASS_TYPE_CMSURFACEVME:
        break;
    default:
        CM_ASSERTMESSAGE("Error: Invalid surface type.");
        break;
    }
    
    return CM_SUCCESS;
}

int32_t CmSurfaceManager::UpdateProfileFor2DSurface(uint32_t index, uint32_t width, uint32_t height, CM_SURFACE_FORMAT format)
{
    uint32_t size = 0;
    uint32_t sizeperpixel = 1;

    int32_t hr = GetFormatSize(format, sizeperpixel);
    if (hr != CM_SUCCESS)
    {
        CM_ASSERTMESSAGE("Error: Failed to get correct surface info.");
        return  hr;
    }
    size = width * height * sizeperpixel;

    m_2DSurfaceAllCount++;
    m_2DSurfaceAllSize += size;

    m_2DSurfaceCount ++;
    m_surfaceSizes[index] = size;


    return CM_SUCCESS;
}

int32_t CmSurfaceManager::UpdateProfileFor1DSurface(uint32_t index, uint32_t size)
{
    m_bufferAllCount++;
    m_bufferAllSize += size;

    m_bufferCount++;
    m_surfaceSizes[index] = size;

    return CM_SUCCESS;
}

int32_t CmSurfaceManager::UpdateProfileFor3DSurface(uint32_t index, uint32_t width, uint32_t height, uint32_t depth, CM_SURFACE_FORMAT format)
{
    uint32_t size = 0;
    uint32_t sizeperpixel = 1;

    int32_t hr = GetFormatSize(format, sizeperpixel);
    if (hr != CM_SUCCESS)
    {
        CM_ASSERTMESSAGE("Error: Failed to get surface info.");
        return  hr;
    }
    size = width * height * depth * sizeperpixel;

    m_3DSurfaceAllCount++;
    m_3DSurfaceAllSize += size;

    m_3DSurfaceCount ++;
    m_surfaceSizes[index] = size;

    return CM_SUCCESS;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Create Surface Manager 
//| Arguments :
//|               device         [in]       Pointer to Cm Device 
//|               halMaxValues      [in]       Cm Max values
//|               surfaceManager          [out]      Reference to pointer to CmSurfaceManager
//|
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::Create(
                              CmDeviceRT* device,
                              CM_HAL_MAX_VALUES halMaxValues,
                              CM_HAL_MAX_VALUES_EX halMaxValuesEx,
                              CmSurfaceManager* &surfaceManager )
{
    int32_t result = CM_SUCCESS;

    surfaceManager = new (std::nothrow) CmSurfaceManager( device );
    if( surfaceManager )
    {
        result = surfaceManager->Initialize( halMaxValues, halMaxValuesEx );
        if( result != CM_SUCCESS )
        {
            CmSurfaceManager::Destroy( surfaceManager );
        }
    }
    else
    {
        CM_ASSERTMESSAGE("Error: Failed to create CmSurfaceManager due to out of system memory.");
        result = CM_OUT_OF_HOST_MEMORY;
    }

    return result;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Destroy CmSurfaceManager
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::Destroy( CmSurfaceManager* &surfaceManager )
{
    if( surfaceManager )
    {
        delete surfaceManager;
        surfaceManager = nullptr;
    }

    return CM_SUCCESS;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Constructor of CmSurfaceManager
//| Returns:    None
//*-----------------------------------------------------------------------------
CmSurfaceManager::CmSurfaceManager( CmDeviceRT* device):
    m_device( device ),
    m_surfaceArraySize( 0 ),
    m_surfaceArray( nullptr ),
    m_surfaceStates(nullptr),
    m_surfaceReleased(nullptr),
    m_surfaceSizes(nullptr),
    m_maxBufferCount(0),
    m_bufferCount(0),
    m_max2DSurfaceCount(0),
    m_2DSurfaceCount(0),
    m_max3DSurfaceCount(0),
    m_3DSurfaceCount(0),
    m_max2DUPSurfaceCount(0),
    m_2DUPSurfaceCount(0),
    m_bufferAllCount(0),
    m_2DSurfaceAllCount(0),
    m_3DSurfaceAllCount(0),
    m_bufferAllSize(0),
    m_2DSurfaceAllSize(0),
    m_3DSurfaceAllSize(0),
    m_garbageCollectionTriggerTimes(0),
    m_garbageCollection1DSize(0),
    m_garbageCollection2DSize(0),
    m_garbageCollection3DSize(0)
{
    GetSurfaceBTIInfo();
};

//*-----------------------------------------------------------------------------
//| Purpose:    Destructor of CmSurfaceManager
//| Returns:    None
//*-----------------------------------------------------------------------------
CmSurfaceManager::~CmSurfaceManager()
{
    for( uint32_t i = ValidSurfaceIndexStart(); i < m_surfaceArraySize; i ++) 
    {
        DestroySurfaceArrayElement(i);
    }

#ifdef SURFACE_MANAGE_PROFILE
    printf("\n\n");
    printf("Total %d 1D buffers, with size: %d\n", m_bufferAllCount, m_bufferAllSize);
    printf("Total %d 2D surfaces, with size: %d\n", m_2DSurfaceAllCount, m_2DSurfaceAllSize);
    printf("Total %d 3D surfaces, with size: %d\n", m_3DSurfaceAllCount, m_3DSurfaceAllSize);

    printf("\nGarbage Collection trigger times: %d\n", m_garbageCollectionTriggerTimes);
    printf("Garbage collection 1D surface size: %d\n", m_garbageCollection1DSize);
    printf("Garbage collection 2D surface size: %d\n", m_garbageCollection2DSize);
    printf("Garbage collection 3D surface size: %d\n", m_garbageCollection3DSize);

    printf("\n\n");
#endif

    MosSafeDeleteArray(m_surfaceStates);
    MosSafeDeleteArray(m_surfaceReleased);
    MosSafeDeleteArray(m_surfaceSizes);
    MosSafeDeleteArray(m_surfaceArray);
}

//*-----------------------------------------------------------------------------
//| Purpose:    Destructor one specific element from SuffaceArray
//| Returns:    None
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::DestroySurfaceArrayElement( uint32_t index )
{
    uint32_t i = index; 

    if (i >=  m_surfaceArraySize)
        return CM_FAILURE;

    CmSurface* pSurface = m_surfaceArray[ i ];

    if( pSurface )
    {
        CmSurface2DRT*   pSurf2D  = nullptr;
        CmBuffer_RT*   pSurf1D  = nullptr;
        CmSurface3DRT*   pSurf3D  = nullptr;
        CmSurfaceVme*  pSurfVme = nullptr;
        CmSurface2DUPRT* pSurf2DUP = nullptr;
        CmSurfaceSampler* pSurfaceSampler = nullptr;
        CmSurfaceSampler8x8* pSurfaceSampler8x8 = nullptr;
        CmStateBuffer* pStateBuff = nullptr;

        switch (pSurface->Type())
        {
        case CM_ENUM_CLASS_TYPE_CMSURFACE2D : 
            pSurf2D = static_cast< CmSurface2DRT* >( pSurface );
            if (pSurf2D)
            {
                DestroySurface( pSurf2D, FORCE_DESTROY);
            }
            break; 

        case CM_ENUM_CLASS_TYPE_CMBUFFER_RT : 
            pSurf1D = static_cast< CmBuffer_RT* >( pSurface );
            if (pSurf1D)
            {
                DestroySurface( pSurf1D, FORCE_DESTROY);
            }
            break; 

        case CM_ENUM_CLASS_TYPE_CMSURFACE3D :
            pSurf3D = static_cast< CmSurface3DRT* >( pSurface );
            if (pSurf3D)
            {
                 DestroySurface( pSurf3D, FORCE_DESTROY);
            }
            break; 

        case CM_ENUM_CLASS_TYPE_CMSURFACEVME: 
            pSurfVme = static_cast< CmSurfaceVme* >( pSurface );
            if( pSurfVme )
            {
                 DestroySurface( pSurfVme );
            }
            break;

        case CM_ENUM_CLASS_TYPE_CMSURFACE2DUP: 
             pSurf2DUP = static_cast< CmSurface2DUPRT* >( pSurface );
             if( pSurf2DUP )
             {
                  DestroySurface( pSurf2DUP, FORCE_DESTROY );
             }
             break;

        case CM_ENUM_CLASS_TYPE_CMSURFACESAMPLER: 
             pSurfaceSampler = static_cast< CmSurfaceSampler* >( pSurface );
             if ( pSurfaceSampler )
             {
                  DestroySurface( pSurfaceSampler );
             }
             break;

         case CM_ENUM_CLASS_TYPE_CMSURFACESAMPLER8X8:
              pSurfaceSampler8x8 = static_cast< CmSurfaceSampler8x8* >( pSurface );
              if ( pSurfaceSampler8x8 )
              {
                   DestroySurface( pSurfaceSampler8x8 );
              }
              break;
            
        case CM_ENUM_CLASS_TYPE_CM_STATE_BUFFER:
            pStateBuff = static_cast< CmStateBuffer* >( pSurface );
            if ( pStateBuff )
            {
                DestroyStateBuffer( pStateBuff, FORCE_DESTROY );
            }
            break;
              
         default: 
              break;
         }
    }

    return CM_SUCCESS;
}

int32_t CmSurfaceManager::GetSurfaceArraySize(uint32_t& surfaceArraySize)
{
    surfaceArraySize = m_surfaceArraySize;
    return CM_SUCCESS;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Create Surface 2D 
//| Arguments :
//|               halMaxValues     [in]      HAL max values
//|               halMaxValuesEx   [in]      Extended HAL max values
//|
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::Initialize( CM_HAL_MAX_VALUES halMaxValues, CM_HAL_MAX_VALUES_EX halMaxValuesEx )
{
    uint32_t totalSurfaceCount = halMaxValues.iMaxBufferTableSize + halMaxValues.iMax2DSurfaceTableSize + halMaxValues.iMax3DSurfaceTableSize + halMaxValuesEx.iMax2DUPSurfaceTableSize;
    uint32_t totalVirtualSurfaceCount = halMaxValues.iMaxSamplerTableSize + halMaxValuesEx.iMaxSampler8x8TableSize;
    m_surfaceArraySize = totalSurfaceCount + totalVirtualSurfaceCount;
    m_maxSurfaceIndexAllocated = 0;

    m_maxBufferCount = halMaxValues.iMaxBufferTableSize;
    m_max2DSurfaceCount = halMaxValues.iMax2DSurfaceTableSize;
    m_max3DSurfaceCount = halMaxValues.iMax3DSurfaceTableSize;
    m_max2DUPSurfaceCount = halMaxValuesEx.iMax2DUPSurfaceTableSize;

    typedef CmSurface* PCMSURFACE;

    m_surfaceArray      = MOS_NewArray(PCMSURFACE, m_surfaceArraySize);
    m_surfaceStates      = MOS_NewArray(int32_t, m_surfaceArraySize);
    m_surfaceReleased   = MOS_NewArray(bool, m_surfaceArraySize);
    m_surfaceSizes      = MOS_NewArray(int32_t, m_surfaceArraySize);

    if( m_surfaceArray == nullptr ||
        m_surfaceStates == nullptr ||
        m_surfaceReleased == nullptr ||
        m_surfaceSizes == nullptr)
    {
        MosSafeDeleteArray(m_surfaceStates);
        MosSafeDeleteArray(m_surfaceReleased);
        MosSafeDeleteArray(m_surfaceSizes);
        MosSafeDeleteArray(m_surfaceArray);

        CM_ASSERTMESSAGE("Error: Out of system memory.");
        return CM_OUT_OF_HOST_MEMORY;
    }

    CmSafeMemSet( m_surfaceArray, 0, m_surfaceArraySize * sizeof( CmSurface* ) );
    CmSafeMemSet( m_surfaceStates, 0, m_surfaceArraySize * sizeof( int32_t ) );
    CmSafeMemSet( m_surfaceReleased, 0, m_surfaceArraySize * sizeof( bool ) );
    CmSafeMemSet( m_surfaceSizes, 0, m_surfaceArraySize * sizeof( int32_t ) );
    return CM_SUCCESS;
}

// Sysmem based surface allocation will always use new surface entry.
int32_t CmSurfaceManager::DestroySurfaceInPool(uint32_t &freeSurfaceCount, SURFACE_DESTROY_KIND destroyKind)
{
    CmSurface*   pSurface = nullptr;
    CmBuffer_RT*   pSurf1D  = nullptr;
    CmSurface2DRT*   pSurf2D  = nullptr;
    CmSurface2DUPRT*   pSurf2DUP = nullptr;
    CmSurface3DRT*   pSurf3D  = nullptr;
    CmStateBuffer* pSurfStateBuffer = nullptr;
    int32_t status = CM_FAILURE;
    uint32_t index = ValidSurfaceIndexStart();
    
    freeSurfaceCount = 0;

    while(index <= m_maxSurfaceIndexAllocated )
    {
        pSurface  = m_surfaceArray[index];
        if (!pSurface)
        {
            index ++;
            continue;
        }
        
        status = CM_FAILURE;
        
        switch (pSurface->Type())
        {
        case CM_ENUM_CLASS_TYPE_CMSURFACE2D :
            pSurf2D = static_cast< CmSurface2DRT* >( pSurface );
            if (pSurf2D)
            {
                status = DestroySurface( pSurf2D, destroyKind);
            }
            break; 

        case CM_ENUM_CLASS_TYPE_CMBUFFER_RT : 
            pSurf1D = static_cast< CmBuffer_RT* >( pSurface );
            if (pSurf1D)
            {
                status = DestroySurface( pSurf1D, destroyKind);
            }
            break; 

        case CM_ENUM_CLASS_TYPE_CMSURFACE3D :
            pSurf3D = static_cast< CmSurface3DRT* >( pSurface );
            if (pSurf3D)
            {
                 status = DestroySurface( pSurf3D, destroyKind);
            }
            break; 

        case CM_ENUM_CLASS_TYPE_CMSURFACE2DUP: 
             pSurf2DUP = static_cast< CmSurface2DUPRT* >( pSurface );
             if( pSurf2DUP )
             {
                  status = DestroySurface( pSurf2DUP, destroyKind );
             }
             break;

        case CM_ENUM_CLASS_TYPE_CM_STATE_BUFFER:
            pSurfStateBuffer = static_cast< CmStateBuffer* >( pSurface );
            if ( pSurfStateBuffer )
            {
                status = DestroyStateBuffer( pSurfStateBuffer, destroyKind );
            }
            break;

        case CM_ENUM_CLASS_TYPE_CMSURFACESAMPLER:
        case CM_ENUM_CLASS_TYPE_CMSURFACESAMPLER8X8:
        case CM_ENUM_CLASS_TYPE_CMSURFACEVME:
            //Do nothing to these kind surfaces
            break;
        
         default: 
             CM_ASSERTMESSAGE("Error: Invalid surface type.");
             break;
        }
        
        if(status == CM_SUCCESS)
        {
            freeSurfaceCount++;
        }
        index ++;
    }

    return CM_SUCCESS;
}

int32_t CmSurfaceManager::TouchSurfaceInPoolForDestroy()
{
    uint32_t freeNum = 0;
    std::vector<CmQueueRT*> &pCmQueue = m_device->GetQueue();
    
    DestroySurfaceInPool(freeNum, GC_DESTROY);
    while (!freeNum)
    {
        CSync *lock = m_device->GetQueueLock();
        lock->Acquire();
        for (auto iter = pCmQueue.begin(); iter != pCmQueue.end(); iter++)
        {
            int32_t result = (*iter)->TouchFlushedTasks();
            if (FAILED(result))
            {
                CM_ASSERTMESSAGE("Error: Flush tasks to flushed queue failure.");
                lock->Release();
                return result;
            }
        }
        lock->Release();

        DestroySurfaceInPool(freeNum, GC_DESTROY);
    }

    m_garbageCollectionTriggerTimes++;

    return freeNum;
}

int32_t CmSurfaceManager::GetFreeSurfaceIndexFromPool(uint32_t &freeIndex)
{
    uint32_t index = ValidSurfaceIndexStart();

    while( ( index < m_surfaceArraySize ) && m_surfaceArray[ index ] )
    {
        index ++;
    }

    if( index >= m_surfaceArraySize )
    {
        CM_ASSERTMESSAGE("Error: Invalid surface index.");
        return CM_FAILURE;
    }

    freeIndex = index;

    return CM_SUCCESS;
}

int32_t CmSurfaceManager::GetFreeSurfaceIndex(uint32_t &freeIndex)
{
    uint32_t index = 0;
    
    if (GetFreeSurfaceIndexFromPool(index) != CM_SUCCESS)
    {
        if (!TouchSurfaceInPoolForDestroy())
        {
            CM_ASSERTMESSAGE("Error: Flush tasks to flushed queue failure.");
            return CM_FAILURE;
        }
        //Try again
        if (GetFreeSurfaceIndexFromPool(index) != CM_SUCCESS)
        {
            CM_ASSERTMESSAGE("Error: Invalid surface index.");
            return CM_FAILURE;
        }
   }

   freeIndex = index;
   m_maxSurfaceIndexAllocated = Max(freeIndex, m_maxSurfaceIndexAllocated);
   
   return CM_SUCCESS;
}


int32_t CmSurfaceManager::GetFormatSize(CM_SURFACE_FORMAT format,uint32_t &sizePerPixel)
{
     switch( format )
    {
        case CM_SURFACE_FORMAT_A16B16G16R16:
        case CM_SURFACE_FORMAT_A16B16G16R16F:
        case CM_SURFACE_FORMAT_Y416:
            sizePerPixel = 8;
            break;

        case CM_SURFACE_FORMAT_X8R8G8B8:
        case CM_SURFACE_FORMAT_A8R8G8B8:
        case CM_SURFACE_FORMAT_R32F:
        case CM_SURFACE_FORMAT_R10G10B10A2:
        case CM_SURFACE_FORMAT_R32_UINT:
        case CM_SURFACE_FORMAT_R32_SINT:
        case CM_SURFACE_FORMAT_AYUV:
        case CM_SURFACE_FORMAT_A8B8G8R8:
        case CM_SURFACE_FORMAT_R16G16_UNORM:
        case CM_SURFACE_FORMAT_Y410:
        case CM_SURFACE_FORMAT_Y216:
        case CM_SURFACE_FORMAT_Y210:
            sizePerPixel = 4;
            break;

        case CM_SURFACE_FORMAT_R8G8_UNORM:
        case CM_SURFACE_FORMAT_R8G8_SNORM:
        case CM_SURFACE_FORMAT_V8U8:
        case CM_SURFACE_FORMAT_Y16_UNORM:
        case CM_SURFACE_FORMAT_Y16_SNORM:
        case CM_SURFACE_FORMAT_R16_UINT:
        case CM_SURFACE_FORMAT_R16_SINT:
        case CM_SURFACE_FORMAT_R16_UNORM:
        case CM_SURFACE_FORMAT_D16:
        case CM_SURFACE_FORMAT_L16:
        case CM_SURFACE_FORMAT_UYVY:
        case CM_SURFACE_FORMAT_VYUY:
        case CM_SURFACE_FORMAT_YUY2:
        case CM_SURFACE_FORMAT_P016:
        case CM_SURFACE_FORMAT_P010:
        case CM_SURFACE_FORMAT_IRW0:
        case CM_SURFACE_FORMAT_IRW1:
        case CM_SURFACE_FORMAT_IRW2:
        case CM_SURFACE_FORMAT_IRW3:
        case CM_SURFACE_FORMAT_R16_FLOAT:
        case CM_SURFACE_FORMAT_A8P8:
            sizePerPixel = 2;
            break;

        case CM_SURFACE_FORMAT_A8:
        case CM_SURFACE_FORMAT_P8:
        case CM_SURFACE_FORMAT_R8_UINT:
        case CM_SURFACE_FORMAT_411P:
        case CM_SURFACE_FORMAT_411R:
        case CM_SURFACE_FORMAT_422H:
        case CM_SURFACE_FORMAT_444P:
        case CM_SURFACE_FORMAT_IMC3:
        case CM_SURFACE_FORMAT_I420:
        case CM_SURFACE_FORMAT_RGBP:
        case CM_SURFACE_FORMAT_BGRP:
        case CM_SURFACE_FORMAT_422V:
        case CM_SURFACE_FORMAT_L8:
        case CM_SURFACE_FORMAT_IA44:
        case CM_SURFACE_FORMAT_AI44:
        case CM_SURFACE_FORMAT_400P:
        case CM_SURFACE_FORMAT_BUFFER_2D:
            sizePerPixel = 1;
            break;

        case CM_SURFACE_FORMAT_NV12:
        case CM_SURFACE_FORMAT_YV12:
            sizePerPixel = 1;
            break;

        default:
            CM_ASSERTMESSAGE("Error: Unsupported surface format.");
            return CM_SURFACE_FORMAT_NOT_SUPPORTED;
    }

    return CM_SUCCESS;
}

inline int32_t CmSurfaceManager::GetMemorySizeOfSurfaces()
{
    uint32_t index = ValidSurfaceIndexStart();
    uint32_t MemSize = 0;

    while( ( index < m_surfaceArraySize ))
    {
        if (!m_surfaceArray[index])
        {
            index ++;
            continue;
        }

        MemSize += m_surfaceSizes[index];
        index++;
    }

    return MemSize;
}


// Allocate surface index from surface pool
int32_t CmSurfaceManager::AllocateSurfaceIndex(uint32_t width, uint32_t height, uint32_t depth, CM_SURFACE_FORMAT format, uint32_t &freeIndex, void *sysMem)
{
    uint32_t index = ValidSurfaceIndexStart();

    if ((m_bufferCount >= m_maxBufferCount && width && !height && !depth)      ||
        (m_2DSurfaceCount >= m_max2DSurfaceCount && width && height && !depth) ||
        (m_3DSurfaceCount >= m_max3DSurfaceCount && width && height && depth))
    {
        if (!TouchSurfaceInPoolForDestroy())
        {
            CM_ASSERTMESSAGE("Error: Failed to flush surface in pool for destroy.");
            return CM_FAILURE;
        }
    }

    if (GetFreeSurfaceIndex(index) != CM_SUCCESS)
    {
        return CM_FAILURE;
    }

    freeIndex = index;
    m_surfaceReleased[index] = false;
    m_maxSurfaceIndexAllocated = Max(index, m_maxSurfaceIndexAllocated);

    return CM_SUCCESS;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Create Buffer
//| Arguments :
//|               size         [in]       the size of buffer
//|               buffer   [in/out]   Reference to cm buffer
//|               mosResource [in]       pointer to mos resource
//|               sysMem      [in]       Pointer to system memory
//|
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::CreateBuffer(uint32_t size, CM_BUFFER_TYPE type, bool svmAllocatedByCm, CmBuffer_RT* & buffer, MOS_RESOURCE * mosResource, void* &sysMem, bool isConditionalBuffer, uint32_t comparisonValue )
{
    uint32_t index = ValidSurfaceIndexStart();
    buffer = nullptr;

    //Not created by CM
    if (mosResource)
    {
        if (GetFreeSurfaceIndex(index) != CM_SUCCESS)
        {
            return CM_EXCEED_SURFACE_AMOUNT;
        }
    }
    else
    {

        if (AllocateSurfaceIndex(size, 0, 0, CM_SURFACE_FORMAT_INVALID, index, sysMem) != CM_SUCCESS)
        {
            return CM_EXCEED_SURFACE_AMOUNT;
        }

    }

    if( m_bufferCount >= m_maxBufferCount )
    {
        CM_ASSERTMESSAGE("Error: Exceed maximum buffer count.");
        return CM_EXCEED_SURFACE_AMOUNT;
    }

    uint32_t handle = 0;
    int32_t result = AllocateBuffer( size, type, handle, mosResource, sysMem );  
    if( result != CM_SUCCESS )
    {
        CM_ASSERTMESSAGE("Error: Falied to allocate buffer.");
        return result;
    }

    result = CmBuffer_RT::Create( index, handle, size, mosResource == nullptr, this, type, svmAllocatedByCm, sysMem, buffer, isConditionalBuffer, comparisonValue);
    if( result != CM_SUCCESS )
    {
        FreeBuffer( handle );
        CM_ASSERTMESSAGE("Error: Falied to create CM buffer.");
        return result;
    }

    m_surfaceArray[ index ] = buffer;
    UpdateProfileFor1DSurface(index, size);

    return CM_SUCCESS;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Allocate Buffer
//| Arguments :
//|               size         [in]       the size of buffer
//|               handle       [in/out]   Reference to handle of cm buffer
//|               mosResource [in]       pointer to mos resource
//|               sysMem      [in]       Pointer to system memory
//|
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::AllocateBuffer(uint32_t size, CM_BUFFER_TYPE type, uint32_t & handle, MOS_RESOURCE * mosResource, void* sysMem )
{
    CM_RETURN_CODE  hr          = CM_SUCCESS;
    MOS_STATUS      mos_status  = MOS_STATUS_SUCCESS;

    PCM_CONTEXT_DATA pCmData = (PCM_CONTEXT_DATA)m_device->GetAccelData();

    handle = 0;
    CM_HAL_BUFFER_PARAM inParam;
    CmSafeMemSet( &inParam, 0, sizeof( CM_HAL_BUFFER_PARAM ) );
    inParam.iSize = size;

    inParam.type = type;

    if (mosResource)
    {
        inParam.pMosResource = mosResource;
        inParam.isAllocatedbyCmrtUmd = false;
    }
    else
    {
        inParam.pMosResource = nullptr;
        inParam.isAllocatedbyCmrtUmd = true;
    }
    if( sysMem )
    { // For BufferUp/BufferSVM
        inParam.pData = sysMem;
    }

    mos_status = pCmData->pCmHalState->pfnAllocateBuffer(pCmData->pCmHalState, &inParam);
    while (mos_status == MOS_STATUS_NO_SPACE )
    {
        if (!TouchSurfaceInPoolForDestroy())
        {
            CM_ASSERTMESSAGE("Error: Failed to flush surface in pool for destroy.");
            return CM_SURFACE_ALLOCATION_FAILURE;
        }
        mos_status = pCmData->pCmHalState->pfnAllocateBuffer(pCmData->pCmHalState, &inParam);
    }
    MOSSTATUS2CM_AND_CHECK(mos_status, hr);

    handle = inParam.dwHandle;

finish:
    return hr;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Free buffer
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::FreeBuffer( uint32_t handle )
{
    CM_RETURN_CODE  hr          = CM_SUCCESS;

    PCM_CONTEXT_DATA pCmData = (PCM_CONTEXT_DATA)m_device->GetAccelData();
    CHK_MOSSTATUS_RETURN_CMERROR(pCmData->pCmHalState->pfnFreeBuffer(pCmData->pCmHalState, (uint32_t)handle));

finish:
    return hr;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Create surface 2d up
//| Arguments :
//|               width        [in]       width of surface 2d up
//|               height       [in]       height of surface 2d up
//|               format       [in]       format of  surface 2d UP
//|               sysMem      [in]       Pointer to system memory
//|               surface   [IN/out]   Reference to pointer of CmSurface2DUP
//|
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::CreateSurface2DUP(uint32_t width, uint32_t height, CM_SURFACE_FORMAT format, void* sysMem, CmSurface2DUPRT* &surface)
{
    surface = nullptr;
    uint32_t index = ValidSurfaceIndexStart();

    if (GetFreeSurfaceIndex(index) != CM_SUCCESS)
    {
        return CM_EXCEED_SURFACE_AMOUNT;
    }

    if( m_2DUPSurfaceCount >= m_max2DUPSurfaceCount )
    {
        CM_ASSERTMESSAGE("Error: Exceed the maximum surface 2D UP count.");
        return CM_EXCEED_SURFACE_AMOUNT;
    }

    uint32_t handle = 0;
    int32_t result = AllocateSurface2DUP( width, height, format, sysMem, handle ); 
    if( result != CM_SUCCESS )
    {
        CM_ASSERTMESSAGE("Error: Falied to allocate surface 2D UP.");
        return result;
    }

    result = CmSurface2DUPRT::Create( index, handle, width, height, format, sysMem, this, surface );
    if( result != CM_SUCCESS )
    {
        FreeSurface2DUP( handle );
        CM_ASSERTMESSAGE("Error: Falied to create CmSurface2DUP.");
        return result;
    }

    m_surfaceArray[ index ] = surface;
    m_2DUPSurfaceCount ++;
    uint32_t sizeperpixel = 1;

    result = GetFormatSize(format, sizeperpixel);
    if (result != CM_SUCCESS)
    {
        CM_ASSERTMESSAGE("Error: Unsupported surface format.");
        return  result;
    }
    m_surfaceSizes[index] = width * height * sizeperpixel;

    return CM_SUCCESS;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Allocate surface 2d up
//| Arguments :
//|               width        [in]       width of surface 2d up
//|               height       [in]       height of surface 2d up
//|               format       [in]       format of  surface 2d UP
//|               sysMem      [in]       Pointer to system memory
//|               handle       [IN/out]   Reference to handle of CmSurface2DUP
//|
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::AllocateSurface2DUP(uint32_t width, uint32_t height, CM_SURFACE_FORMAT format, void* sysMem, uint32_t & handle)
{
    CM_RETURN_CODE  hr          = CM_SUCCESS;
    MOS_STATUS      mos_status  = MOS_STATUS_SUCCESS;

    handle = 0;
    PCM_CONTEXT_DATA pCmData = (PCM_CONTEXT_DATA)m_device->GetAccelData();

    CM_HAL_SURFACE2D_UP_PARAM inParam;
    CmSafeMemSet( &inParam, 0, sizeof( CM_HAL_SURFACE2D_UP_PARAM ) );
    inParam.iWidth  = width;
    inParam.iHeight = height;
    inParam.format  = format;
    inParam.pData   = sysMem;

    mos_status = pCmData->pCmHalState->pfnAllocateSurface2DUP(pCmData->pCmHalState,&inParam);
    while ( mos_status == MOS_STATUS_NO_SPACE )
    {
        if (!TouchSurfaceInPoolForDestroy())
        {
            CM_ASSERTMESSAGE("Error: Failed to flush surface in pool for destroy.");
            return CM_SURFACE_ALLOCATION_FAILURE;
        }
        mos_status = pCmData->pCmHalState->pfnAllocateSurface2DUP(pCmData->pCmHalState,&inParam);
    }
    MOSSTATUS2CM_AND_CHECK(mos_status, hr);

    handle = inParam.dwHandle;

finish:
    return hr;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Free surface 2d up
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::FreeSurface2DUP( uint32_t handle )
{
    CM_RETURN_CODE  hr          = CM_SUCCESS;

    PCM_CONTEXT_DATA pCmData = (PCM_CONTEXT_DATA)m_device->GetAccelData();

    CHK_MOSSTATUS_RETURN_CMERROR(pCmData->pCmHalState->pfnFreeSurface2DUP(pCmData->pCmHalState, (uint32_t)handle));

finish:
    return hr;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Create surface 2d 
//| Arguments :
//|               width        [in]       width of surface 2d 
//|               height       [in]       height of surface 2d 
//|               pitch        [in]       pitch of surface 2d
//|               format       [in]       format of  surface 2d
//|               surface   [IN/out]   Reference to CmSurface2D
//|
//| Returns:    Result of the operation.
//| NOTE: It's only called within CMRT@UMD, will not be called by CMRT Thin
//|           For 2D surface, since the real memory buffer is not allocated in CMRT@UMD, no reuse/manager can be done
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::CreateSurface2D(uint32_t width, uint32_t height, uint32_t pitch, bool createdByCm, CM_SURFACE_FORMAT format,  CmSurface2DRT* &surface)
{
    uint32_t handle = 0;
    uint32_t index = ValidSurfaceIndexStart();
    int32_t result =0;

    surface = nullptr;

    result = Surface2DSanityCheck(width, height, format);
    if( result != CM_SUCCESS )
    {
        CM_ASSERTMESSAGE("Error: Surface 2D sanity check failure.");
        return result;
    }

    if (createdByCm)
    {
        if (AllocateSurfaceIndex(width, height, 0, format, index, nullptr) != CM_SUCCESS)
        {
            return CM_EXCEED_SURFACE_AMOUNT;
        }
    }
    else
    {
        if (GetFreeSurfaceIndex(index) != CM_SUCCESS)
        {
            return CM_EXCEED_SURFACE_AMOUNT;
        }
    }

    if( m_2DSurfaceCount >= m_max2DSurfaceCount )
    {
        CM_ASSERTMESSAGE("Error: Exceed the maximum surface 2D count.");
        return CM_EXCEED_SURFACE_AMOUNT;
    }

    result = AllocateSurface2D( width, height, format, handle, pitch); 
    if( result != CM_SUCCESS )
    {
        CM_ASSERTMESSAGE("Error: Falied to allocate surface.");
        return result;
    }

    result = CmSurface2DRT::Create( index, handle, width, height, pitch, format, true, this, surface );
    if( result != CM_SUCCESS )
    {
        FreeSurface2D( handle );
        CM_ASSERTMESSAGE("Error: Falied to create CmSurface2D.");
        return result;
    }

    m_surfaceArray[ index ] = surface;

    result = UpdateProfileFor2DSurface(index, width, height, format);
    if (result != CM_SUCCESS)
    {
        FreeSurface2D(handle);
        CM_ASSERTMESSAGE("Error: Falied to update profile for surface 2D.");
        return  result;
    }


    return CM_SUCCESS;
}


//*-----------------------------------------------------------------------------
//| Purpose:    Allocate surface 2d 
//| Arguments :
//|               width        [in]       width of surface 2d 
//|               height       [in]       height of surface 2d 
//|               format       [in]       format of  surface 2d
//|               handle       [in/out]   Reference to handle of Surface 2D
//|               pitch        [in/out]   Reference to pitch of surface 2d
//|
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::AllocateSurface2D(uint32_t width, uint32_t height, CM_SURFACE_FORMAT format, uint32_t & handle,uint32_t & pitch)
{
    CM_RETURN_CODE  hr          = CM_SUCCESS;
    MOS_STATUS      mos_status  = MOS_STATUS_SUCCESS;
    PCM_CONTEXT_DATA pCmData = (PCM_CONTEXT_DATA)m_device->GetAccelData();

    CM_HAL_SURFACE2D_PARAM inParam;
    CmSafeMemSet( &inParam, 0, sizeof( CM_HAL_SURFACE2D_PARAM ) );
    inParam.iWidth = width;
    inParam.iHeight = height;
    inParam.format = format;
    inParam.pData  = nullptr;
    inParam.isAllocatedbyCmrtUmd = true;

    mos_status = pCmData->pCmHalState->pfnAllocateSurface2D(pCmData->pCmHalState,&inParam);
    while (mos_status == MOS_STATUS_NO_SPACE)
    {
        if (!TouchSurfaceInPoolForDestroy())
        {
            CM_ASSERTMESSAGE("Error: Failed to flush surface in pool for destroy.");
            return CM_SURFACE_ALLOCATION_FAILURE;
        }
        mos_status = pCmData->pCmHalState->pfnAllocateSurface2D(pCmData->pCmHalState,&inParam);
    }
    MOSSTATUS2CM_AND_CHECK(mos_status, hr);

    handle = inParam.dwHandle;

    //Get pitch size for 2D surface
    CHK_MOSSTATUS_RETURN_CMERROR(pCmData->pCmHalState->pfnGetSurface2DTileYPitch(pCmData->pCmHalState, &inParam));

    pitch = inParam.iPitch;

finish:
    return hr;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Create surface 2d 
//| Arguments :
//|               width        [in]       width of surface 2d 
//|               height       [in]       height of surface 2d 
//|               format       [in]       format of  surface 2d
//|               mosResource [in]       pointer to mos resource
//|               handle       [in/out]   Reference to handle of Surface 2D
//|               pitch        [in/out]   Reference to pitch of surface 2d
//|
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::AllocateSurface2D( uint32_t width, uint32_t height, CM_SURFACE_FORMAT format, MOS_RESOURCE * mosResource, uint32_t &handle)
{
    CM_RETURN_CODE  hr          = CM_SUCCESS;
    MOS_STATUS      mos_status  = MOS_STATUS_SUCCESS;

    PCM_CONTEXT_DATA pCmData = (PCM_CONTEXT_DATA)m_device->GetAccelData();

    CM_HAL_SURFACE2D_PARAM inParam;
    CmSafeMemSet( &inParam, 0, sizeof( CM_HAL_SURFACE2D_PARAM ) );
    inParam.iWidth                 = width;
    inParam.iHeight                = height;
    inParam.format                 = format;
    inParam.pMosResource           = mosResource;
    inParam.isAllocatedbyCmrtUmd   = false;

    mos_status = pCmData->pCmHalState->pfnAllocateSurface2D(pCmData->pCmHalState,&inParam);
    while (mos_status == MOS_STATUS_NO_SPACE)
    {
        if (!TouchSurfaceInPoolForDestroy())
        {
            CM_ASSERTMESSAGE("Error: Failed to flush surface in pool for destroy.");
            return CM_SURFACE_ALLOCATION_FAILURE;
        }
        mos_status = pCmData->pCmHalState->pfnAllocateSurface2D(pCmData->pCmHalState,&inParam);
    }
    MOSSTATUS2CM_AND_CHECK(mos_status, hr);
    
    handle = inParam.dwHandle;

finish:
    return hr;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Free surface 2D
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::FreeSurface2D( uint32_t handle )
{
    CM_RETURN_CODE  hr          = CM_SUCCESS;

    PCM_CONTEXT_DATA pCmData = (PCM_CONTEXT_DATA)m_device->GetAccelData();
    CHK_MOSSTATUS_RETURN_CMERROR(pCmData->pCmHalState->pfnFreeSurface2D(pCmData->pCmHalState, handle));

finish:
    return hr;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Create Sampler8x8 surface
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::CreateSampler8x8Surface(CmSurface2DRT* currentSurface, SurfaceIndex* & sampler8x8SurfaceIndex, CM_SAMPLER8x8_SURFACE sampler8x8Type, CM_SURFACE_ADDRESS_CONTROL_MODE addressControl, CM_FLAG* flag)
{
    uint32_t index = ValidSurfaceIndexStart();
    CmSurfaceSampler8x8* pCmSurfaceSampler8x8 = nullptr;
    SurfaceIndex * surfCurrent = nullptr;
    uint32_t cmIndex = 0xFFFFFFFF;

    if (AllocateSurfaceIndex(0, 0, 0, CM_SURFACE_FORMAT_INVALID, index, nullptr) != CM_SUCCESS)
    {
        return CM_EXCEED_SURFACE_AMOUNT;
    }

    uint32_t indexFor2D = 0xFFFFFFFF;
    currentSurface->GetIndexFor2D( indexFor2D );
    currentSurface->GetIndex(surfCurrent);
    cmIndex = surfCurrent->get_data();

    int32_t result = CmSurfaceSampler8x8::Create( index, indexFor2D, cmIndex, this, pCmSurfaceSampler8x8, sampler8x8Type, addressControl, flag);
    if ( result != CM_SUCCESS )
    {
        CM_ASSERTMESSAGE("Error: Falied to create sampler8x8 surface.");
        return result;
    }

    if(pCmSurfaceSampler8x8)
    {
        m_surfaceArray[ index ] = pCmSurfaceSampler8x8; 
        pCmSurfaceSampler8x8->GetIndex( sampler8x8SurfaceIndex );
        return CM_SUCCESS;
    }
    else
    {
        return CM_OUT_OF_HOST_MEMORY;
    }
}

//*-----------------------------------------------------------------------------
//| Purpose:    Destroy Sampler8x8 surface
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::DestroySampler8x8Surface(SurfaceIndex* & sampler8x8SurfaceIndex )
{
    if(!sampler8x8SurfaceIndex) {
        return CM_FAILURE;
    }

    uint32_t index = sampler8x8SurfaceIndex->get_data();
    CmSurface* pSurface = m_surfaceArray[ index ]; 
    CmSurfaceSampler8x8* pSurfSampler8x8 = nullptr;
    if (pSurface && ( pSurface->Type() == CM_ENUM_CLASS_TYPE_CMSURFACESAMPLER8X8 ))
    {
        pSurfSampler8x8 = static_cast< CmSurfaceSampler8x8* >( pSurface );
    }

    if(pSurfSampler8x8)
    {
        int32_t result = DestroySurface( pSurfSampler8x8 );
        if( result == CM_SUCCESS )
        {
            sampler8x8SurfaceIndex = nullptr;
            return CM_SUCCESS;
        }
        else
        {
            return CM_FAILURE;
        }
    }
    else
    {
        return CM_FAILURE;
    }
}

//*-----------------------------------------------------------------------------
//| Purpose:    Create Sampler8x8 surface
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::DestroySurface( CmSurfaceSampler8x8* & sampler8x8Surface )
{
    if( !sampler8x8Surface )
    {
        return CM_FAILURE;
    }

    SurfaceIndex* pIndex = nullptr;
    sampler8x8Surface->GetIndex( pIndex );
    CM_ASSERT( pIndex );
    uint32_t index = pIndex->get_data();

    CM_ASSERT( m_surfaceArray[ index ] == sampler8x8Surface ); 

    UpdateStateForRealDestroy(index, CM_ENUM_CLASS_TYPE_CMSURFACESAMPLER8X8);

    CmSurface* pSurface = sampler8x8Surface;
    CmSurface::Destroy( pSurface ) ;
    return CM_SUCCESS;
}


//*-----------------------------------------------------------------------------
//| Purpose:    Destroy Vme surface
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::DestroyVmeSurface( SurfaceIndex* & vmeSurfaceIndex )
{
    if( !vmeSurfaceIndex  )
    {
        return CM_INVALID_ARG_VALUE;
    }

    uint32_t index = vmeSurfaceIndex->get_data();

    CmSurface* pSurface = m_surfaceArray[ index ]; 
    CmSurfaceVme* pSurfVme = nullptr;
    if (pSurface && ( pSurface->Type() ==  CM_ENUM_CLASS_TYPE_CMSURFACEVME ))
    {
        pSurfVme = static_cast< CmSurfaceVme* >( pSurface );
    }

    if( pSurfVme )
    {
        int32_t result = DestroySurface( pSurfVme);
        if( result == CM_SUCCESS )
        {
            vmeSurfaceIndex = nullptr;
        }
		return result;
    }
    else
    {
        return CM_INVALID_ARG_VALUE;
    }
}

int32_t CmSurfaceManager::CreateVmeSurface( CmSurface2DRT* currentSurface,
                                        CmSurface2DRT** forwardSurfaceArray, 
                                        CmSurface2DRT** backwardSurfaceArray,
                                        const uint32_t surfaceCountForward, 
                                        const uint32_t surfaceCountBackward,
                                        SurfaceIndex* & vmeSurfaceIndex )
{
    uint32_t index = ValidSurfaceIndexStart();
    CmSurfaceVme* pCmSurfaceVme = nullptr;

    if (AllocateSurfaceIndex(0, 0, 0, CM_SURFACE_FORMAT_INVALID, index, nullptr) != CM_SUCCESS)
    {
        return CM_EXCEED_SURFACE_AMOUNT;
    }

    uint32_t indexFor2DCurrent = CM_INVALID_VME_SURFACE;
    uint32_t *indexFor2DForward = nullptr;
    uint32_t *indexFor2DBackward = nullptr;

    uint32_t indexCurrent = CM_INVALID_VME_SURFACE;
    uint32_t *indexForward = nullptr;
    uint32_t *indexBackward = nullptr;
    SurfaceIndex * surfCurrent = nullptr;
    SurfaceIndex * surfForward = nullptr;
    SurfaceIndex * surfBackward = nullptr;

    currentSurface->GetIndexFor2D( indexFor2DCurrent );
    currentSurface->GetIndex( surfCurrent );
    indexCurrent = surfCurrent->get_data();

    if( forwardSurfaceArray )
    {
        
        indexFor2DForward = MOS_NewArray(uint32_t, surfaceCountForward);
        indexForward = MOS_NewArray(uint32_t, surfaceCountForward);

        if (!indexFor2DForward || !indexForward)
        {
            CM_ASSERTMESSAGE("Error: Out of system memory.");
            MosSafeDeleteArray( indexFor2DForward );
            MosSafeDeleteArray( indexForward );
            return CM_OUT_OF_HOST_MEMORY;
        }
        for (uint32_t i = 0; i < surfaceCountForward; i++) {
            forwardSurfaceArray[i]->GetIndexFor2D( indexFor2DForward[i]);
            forwardSurfaceArray[i]->GetIndex(surfForward);
            indexForward[i] = surfForward->get_data();
        }
    }

    if( backwardSurfaceArray )
    {
        indexFor2DBackward = MOS_NewArray(uint32_t, surfaceCountBackward);
        indexBackward = MOS_NewArray(uint32_t, surfaceCountBackward);

        if (!indexFor2DBackward || !indexBackward )
        {
            CM_ASSERTMESSAGE("Error: Out of system memory.");
            MosSafeDeleteArray( indexFor2DForward );
            MosSafeDeleteArray( indexForward );
            MosSafeDeleteArray( indexFor2DBackward );
            MosSafeDeleteArray( indexBackward );
            return CM_OUT_OF_HOST_MEMORY;
        }

        for (uint32_t i = 0; i < surfaceCountBackward; i++) 
        {
            backwardSurfaceArray[i]->GetIndexFor2D( indexFor2DBackward[i]);
            backwardSurfaceArray[i]->GetIndex(surfBackward);
            indexBackward[i] = surfBackward->get_data();
        }
    }

    int32_t result = CmSurfaceVme::Create( index, indexFor2DCurrent, indexFor2DForward, indexFor2DBackward, indexCurrent, indexForward, indexBackward, surfaceCountForward, surfaceCountBackward, this, pCmSurfaceVme );
    if( result != CM_SUCCESS )
    {
        CM_ASSERTMESSAGE("Error: Failed to create CmSurfaceVme.");
        MosSafeDeleteArray( indexFor2DBackward );
        MosSafeDeleteArray( indexBackward );
        MosSafeDeleteArray( indexFor2DForward );
        MosSafeDeleteArray( indexForward );
        return result;
    }

    m_surfaceArray[ index ] = pCmSurfaceVme; 
    pCmSurfaceVme->GetIndex( vmeSurfaceIndex );

    return CM_SUCCESS;
}

    
//*-----------------------------------------------------------------------------
//| Purpose:    Destroy CMBuffer in SurfaceManager
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::DestroySurface( CmBuffer_RT* & buffer, SURFACE_DESTROY_KIND destroyKind)
{
    uint32_t handle = 0;
    void  *pAddress = nullptr;
    int32_t result =  CM_SUCCESS;
    
    if ( !buffer )
    {
        return CM_FAILURE;
    }
    SurfaceIndex* pIndex = nullptr;
    buffer->GetIndex( pIndex );
    CM_ASSERT( pIndex );
    uint32_t index = pIndex->get_data();
    CM_ASSERT( m_surfaceArray[ index ] == buffer ); 

    if (destroyKind == FORCE_DESTROY)
    {
        buffer->WaitForReferenceFree();
    }
    else
    {
        //Delayed destroy
        result = UpdateStateForDelayedDestroy(destroyKind, index);

        if (result != CM_SUCCESS)
        {
            return result;
        }
    }

    //Destroy surface
    result = buffer->GetHandle( handle );
    if( result != CM_SUCCESS )
    {
        return result;
    }

    result = FreeBuffer( handle );
    if( result != CM_SUCCESS )
    {
        return result;
    }

    buffer->GetAddress(pAddress);
    if ((buffer->GetBufferType() == CM_BUFFER_SVM) && pAddress && buffer->IsCMRTAllocatedSVMBuffer()) 
    {
        MOS_AlignedFreeMemory(pAddress);
    }

    CmSurface* pSurface = buffer;
    CmSurface::Destroy( pSurface ) ;

    UpdateStateForRealDestroy(index, CM_ENUM_CLASS_TYPE_CMBUFFER_RT);

    return CM_SUCCESS;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Destroy Surface2d up in SurfaceManager
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::DestroySurface( CmSurface2DUPRT* & surface2dUP, SURFACE_DESTROY_KIND destroyKind)
{
    uint32_t handle = 0;
    int32_t result = CM_SUCCESS;
    
    if ( !surface2dUP  )
    {
        return CM_FAILURE;
    }

    SurfaceIndex* pIndex = nullptr;
    surface2dUP->GetIndex( pIndex );
    CM_ASSERT( pIndex );
    uint32_t index = pIndex->get_data();

    CM_ASSERT( m_surfaceArray[ index ] == surface2dUP ); 

    
    if (destroyKind == FORCE_DESTROY)
    {
        surface2dUP->WaitForReferenceFree();
    }
    else
    {
        result = UpdateStateForDelayedDestroy(destroyKind, index);
        if (result != CM_SUCCESS)
        {
            return result;
        }
    }

    result = surface2dUP->GetHandle( handle );
    if( result != CM_SUCCESS )
    {
        return result;
    }

    result = FreeSurface2DUP( handle );
    if( result != CM_SUCCESS )
    {
        return result;
    }


    CmSurface* pSurface = surface2dUP;
    CmSurface::Destroy( pSurface ) ;

    UpdateStateForRealDestroy(index, CM_ENUM_CLASS_TYPE_CMSURFACE2DUP);

    return CM_SUCCESS;
}


//*-----------------------------------------------------------------------------
//| Purpose:    Destroy surface  in SurfaceManager
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::DestroySurface( CmSurface2DRT* & surface2d,  SURFACE_DESTROY_KIND destroyKind)
{
    uint32_t handle = 0;
    SurfaceIndex* pIndex = nullptr;
    surface2d->GetIndex( pIndex );
    CM_ASSERT( pIndex );
    uint32_t index = pIndex->get_data();
    int32_t result  =  CM_SUCCESS;
    
    CM_ASSERT( m_surfaceArray[ index ] == surface2d ); 

    if (destroyKind == FORCE_DESTROY )
    {
        //For none-cm created surface, no caching, sync is required instead.
        surface2d->WaitForReferenceFree();
    }
    else
    {

        result = UpdateStateForDelayedDestroy(destroyKind, index);

        if (result != CM_SUCCESS)
        {
            return result;
        }
    }

    result = surface2d->GetHandle( handle );
    if( result != CM_SUCCESS )
    {
        return result;
    }

    result = FreeSurface2D( handle );
    if( result != CM_SUCCESS )
    {
        return result;
    }

    CmSurface* pSurface = surface2d;
    CmSurface::Destroy( pSurface ) ;
    
    UpdateStateForRealDestroy(index, CM_ENUM_CLASS_TYPE_CMSURFACE2D);

    return CM_SUCCESS;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Destroy surface vme in SurfaceManager
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::DestroySurface( CmSurfaceVme* & vmeSurface )
{
    if( !vmeSurface )
    {
        return CM_FAILURE;
    }

    SurfaceIndex* pIndex = nullptr;
    vmeSurface->GetIndex( pIndex );
    CM_ASSERT( pIndex );

    uint32_t index = pIndex->get_data();
    CM_ASSERT( m_surfaceArray[ index ] == vmeSurface );

    UpdateStateForRealDestroy(index, CM_ENUM_CLASS_TYPE_CMSURFACEVME);

    CmSurface* pSurface = vmeSurface;
    CmSurface::Destroy( pSurface ) ;

    return CM_SUCCESS;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Get Cm surface pointer
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::GetSurface( const uint32_t index, CmSurface* & surface )
{
    if( index < m_surfaceArraySize )
    {
        surface = m_surfaceArray[ index ];
    }
    else
    {
        // check if alias surface
        surface = m_surfaceArray[(index % m_surfaceArraySize)];
        if (surface->Type() == CM_ENUM_CLASS_TYPE_CMSURFACE2D)
        {
            CmSurface2DRT* pSurf2D = nullptr;
            uint32_t numAliases = 0;
            pSurf2D = static_cast< CmSurface2DRT* >(surface);
            pSurf2D->GetNumAliases(numAliases);
            if (numAliases == 0)
            {
                // index was out of bounds and not alias surface
                surface = nullptr;
                return CM_FAILURE;
            }
        }
        else if (surface->Type() == CM_ENUM_CLASS_TYPE_CMBUFFER_RT)
        {
            CmBuffer_RT* pBufRT = nullptr;
            uint32_t numAliases = 0;
            pBufRT = static_cast< CmBuffer_RT* >(surface);
            pBufRT->GetNumAliases(numAliases);
            if (numAliases == 0)
            {
                // index was out of bounds and not alias surface
                surface = nullptr;
                return CM_FAILURE;
            }
        }
        else
        {
            surface = nullptr;
            return CM_FAILURE;
        }
    }

    return CM_SUCCESS;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Get Cm Device pointer
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::GetCmDevice( CmDeviceRT* & device )
{
  device = m_device;
  return CM_SUCCESS;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Get bytes per pixel and height
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::GetPixelBytesAndHeight(uint32_t width, uint32_t height, CM_SURFACE_FORMAT format, uint32_t& sizePerPixel, uint32_t& updatedHeight)
{
    UNUSED(width);
    updatedHeight = height;
    switch( format )
    {
        case CM_SURFACE_FORMAT_A16B16G16R16:
        case CM_SURFACE_FORMAT_A16B16G16R16F:
        case CM_SURFACE_FORMAT_Y416:
            sizePerPixel = 8;
            break;

        case CM_SURFACE_FORMAT_X8R8G8B8:
        case CM_SURFACE_FORMAT_A8R8G8B8:
        case CM_SURFACE_FORMAT_A8B8G8R8:
        case CM_SURFACE_FORMAT_R32F:
        case CM_SURFACE_FORMAT_R32_UINT:
        case CM_SURFACE_FORMAT_R32_SINT:
        case CM_SURFACE_FORMAT_R10G10B10A2:
        case CM_SURFACE_FORMAT_AYUV:
        case CM_SURFACE_FORMAT_R16G16_UNORM:
        case CM_SURFACE_FORMAT_Y410:
        case CM_SURFACE_FORMAT_Y216:
        case CM_SURFACE_FORMAT_Y210:
            sizePerPixel = 4;
            break;

        case CM_SURFACE_FORMAT_R8G8_SNORM:
        case CM_SURFACE_FORMAT_R16_UINT:
        case CM_SURFACE_FORMAT_R16_SINT:
        case CM_SURFACE_FORMAT_R16_UNORM:
        case CM_SURFACE_FORMAT_D16:
        case CM_SURFACE_FORMAT_L16:
        case CM_SURFACE_FORMAT_R8G8_UNORM:
        case CM_SURFACE_FORMAT_UYVY:
        case CM_SURFACE_FORMAT_VYUY:
        case CM_SURFACE_FORMAT_YUY2:
        case CM_SURFACE_FORMAT_Y16_SNORM:
        case CM_SURFACE_FORMAT_Y16_UNORM:
        case CM_SURFACE_FORMAT_IRW0:
        case CM_SURFACE_FORMAT_IRW1:
        case CM_SURFACE_FORMAT_IRW2:
        case CM_SURFACE_FORMAT_IRW3:
        case CM_SURFACE_FORMAT_R16_FLOAT:
        case CM_SURFACE_FORMAT_V8U8:
        case CM_SURFACE_FORMAT_A8P8:
            sizePerPixel = 2;
            break;

        case CM_SURFACE_FORMAT_P016:
        case CM_SURFACE_FORMAT_P010:
            sizePerPixel = 2;
            updatedHeight += updatedHeight/2; 
            break;

        case CM_SURFACE_FORMAT_A8:
        case CM_SURFACE_FORMAT_P8:
        case CM_SURFACE_FORMAT_R8_UINT:
        case CM_SURFACE_FORMAT_Y8_UNORM:
        case CM_SURFACE_FORMAT_L8:
        case CM_SURFACE_FORMAT_IA44:
        case CM_SURFACE_FORMAT_AI44:
        case CM_SURFACE_FORMAT_400P:
        case CM_SURFACE_FORMAT_BUFFER_2D:
            sizePerPixel = 1;
            break;

        case CM_SURFACE_FORMAT_NV12:            //NV12
            sizePerPixel = 1;
            //To support NV12 format with odd height here.
            //if original height is even, the UV plane's height is set as updatedHeight/2, which equals to (updatedHeight+1)/2 
            //if original height is odd, the UV plane's height is set as roundup(updatedHeight/2), which equals to (updatedHeight+1)/2 too
            updatedHeight += (updatedHeight + 1) / 2;
            break;

        // 4:1:1 (12-bits per pixel)      // 4:2:2 (16-bits per pixel)
        // 411P                           // 422H
        // ----------------->             // ----------------->
        // ________________________       // ________________________
        //|Y0|Y1|                  |      //|Y0|Y1|                  |
        //|__|__|                  |      //|__|__|                  |
        //|                        |      //|                        |
        //|                        |      //|                        |
        //|                        |      //|                        |
        //|                        |      //|                        |
        //|                        |      //|                        |
        //|________________________|      //|________________________|
        //|U0|U1||                 |      //|U0|U1|      |           |
        //|__|__||                 |      //|__|__|      |           |
        //|      |                 |      //|            |           |
        //|      |      PAD        |      //|            |    PAD    |
        //|      |                 |      //|            |           |
        //|      |                 |      //|            |           |
        //|      |                 |      //|            |           |
        //|______|_________________|      //|____________|___________|
        //|V0|V1||                 |      //|V0|V1|      |           |
        //|__|__||                 |      //|__|__|      |           |
        //|      |                 |      //|            |           |
        //|      |      PAD        |      //|            |    PAD    |
        //|      |                 |      //|            |           |
        //|      |                 |      //|            |           |
        //|      |                 |      //|            |           |
        //|______|_________________|      //|____________|___________|

        // 4:4:4 (24-bits per pixel)
        // 444P
        // ----------------->
        // ________________________
        //|Y0|Y1|                  |
        //|__|__|                  |
        //|                        |
        //|                        |
        //|                        |
        //|                        |
        //|                        |
        //|________________________|
        //|U0|U1|                  |
        //|__|__|                  |
        //|                        |
        //|                        |
        //|                        |
        //|                        |
        //|                        |
        //|________________________|
        //|V0|V1|                  |
        //|__|__|                  |
        //|                        |
        //|                        |
        //|                        |
        //|                        |
        //|                        |
        //|________________________|

        case CM_SURFACE_FORMAT_411P:
        case CM_SURFACE_FORMAT_422H:
        case CM_SURFACE_FORMAT_444P:
        case CM_SURFACE_FORMAT_RGBP:
        case CM_SURFACE_FORMAT_BGRP:
            sizePerPixel = 1;
            updatedHeight = height * 3; 
            break;

        // 4:2:0 (12-bits per pixel)
        // IMC1                           // IMC3
        // ----------------->             // ----------------->
        // ________________________       // ________________________
        //|Y0|Y1|                  |      //|Y0|Y1|                  |
        //|__|__|                  |      //|__|__|                  |
        //|                        |      //|                        |
        //|                        |      //|                        |
        //|                        |      //|                        |
        //|                        |      //|                        |
        //|                        |      //|                        |
        //|________________________|      //|________________________|
        //|V0|V1|      |           |      //|U0|U1|      |           |
        //|__|__|      |           |      //|__|__|      |           |
        //|            |           |      //|            |           |
        //|____________|  PAD      |      //|____________|  PAD      |
        //|U0|U1|      |           |      //|V0|V1|      |           |
        //|__|__|      |           |      //|__|__|      |           |
        //|            |           |      //|            |           |
        //|____________|___________|      //|____________|___________|
        case CM_SURFACE_FORMAT_IMC3:
            sizePerPixel = 1;
            updatedHeight = height * 2; 
            break;

        // 4:2:2V (16-bits per pixel)
        // 422V
        // ----------------->
        // ________________________
        //|Y0|Y1|                  |
        //|__|__|                  |
        //|                        |
        //|                        |
        //|                        |
        //|                        |
        //|                        |
        //|________________________|
        //|U0|U1|                  |
        //|__|__|                  |
        //|                        |
        //|________________________|
        //|V0|V1|                  |
        //|__|__|                  |
        //|                        |
        //|________________________|
        case CM_SURFACE_FORMAT_422V:
            sizePerPixel = 1;
            updatedHeight = height * 2; 
            break;

        case CM_SURFACE_FORMAT_YV12:
        case CM_SURFACE_FORMAT_411R://411R
        case CM_SURFACE_FORMAT_I420:            //I420
            sizePerPixel = 1;
            updatedHeight += updatedHeight/2; 
            break;

        default:
            CM_ASSERTMESSAGE("Error: Unsupported surface format.");
            return CM_SURFACE_FORMAT_NOT_SUPPORTED;
    }

    return CM_SUCCESS;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Create Surface 3D
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::CreateSurface3D(uint32_t width, uint32_t height, uint32_t depth, CM_SURFACE_FORMAT format, CmSurface3DRT* & surface3d )
{
    switch( format )
    {
        case CM_SURFACE_FORMAT_X8R8G8B8:
        case CM_SURFACE_FORMAT_A8R8G8B8:
        case CM_SURFACE_FORMAT_A16B16G16R16:
            break;
        default:
            CM_ASSERTMESSAGE("Error: Unsupported surface format.");
            return CM_SURFACE_FORMAT_NOT_SUPPORTED;
    }

    uint32_t size = 0;
    uint32_t sizeperpixel = 1;

    uint32_t index = ValidSurfaceIndexStart();
    int32_t result = 0;
    result = GetFormatSize(format, sizeperpixel);
    if (result != CM_SUCCESS)
    {
        CM_ASSERTMESSAGE("Error: Faied to get correct surface info.");
        return  result;
    }
    size = width * height * depth * sizeperpixel;
    surface3d = nullptr;

    if (AllocateSurfaceIndex(width, height, depth, format, index, nullptr) != CM_SUCCESS)
    {
        return CM_EXCEED_SURFACE_AMOUNT;
    }

    if( m_3DSurfaceCount >= m_max3DSurfaceCount )
    {
        CM_ASSERTMESSAGE("Error: Exceed the maximum surface 3d count.");
        return CM_EXCEED_SURFACE_AMOUNT;
    }

    uint32_t handle = 0;

    result = Allocate3DSurface( width, height, depth, format, handle );
    if( result != CM_SUCCESS )
    {
        CM_ASSERTMESSAGE("Error: Failed to allocate surface 3d.");
        return result;
    }

    result = CmSurface3DRT::Create( index, handle, width, height, depth, format, this, surface3d );
    if( result != CM_SUCCESS )
    {
        Free3DSurface( handle );
        CM_ASSERTMESSAGE("Error: Failed to create CM surface 3d.");
        return result;
    }

    m_surfaceArray[ index ] = surface3d;

    result = UpdateProfileFor3DSurface(index, width, height, depth, format);
    if (result != CM_SUCCESS)
    {
        Free3DSurface(handle);
        CM_ASSERTMESSAGE("Error: Failed to update profile for surface 3d.");
        return  result;
    }

    return CM_SUCCESS;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Destory Surface 3D
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::DestroySurface( CmSurface3DRT* & surface3d, SURFACE_DESTROY_KIND destroyKind)
{
    uint32_t handle = 0;
    SurfaceIndex* pIndex = nullptr;
    surface3d->GetIndex( pIndex );
    CM_ASSERT( pIndex );
    uint32_t index = pIndex->get_data();
    int32_t result = CM_SUCCESS;
    
    CM_ASSERT( m_surfaceArray[ index ] == surface3d ); 

    if (destroyKind == FORCE_DESTROY)
    {
        surface3d->WaitForReferenceFree();
    }
    else
    {

        result = UpdateStateForDelayedDestroy(destroyKind, index);

        if (result != CM_SUCCESS)
        {
            return result;
        }
    }

    result = surface3d->GetHandle( handle );
    if( result != CM_SUCCESS )
    {
        return result;
    }

    result = Free3DSurface( handle );
    if( result != CM_SUCCESS )
    {
        return result;
    }

    CmSurface* pSurface = surface3d;
    CmSurface::Destroy( pSurface ) ;
    
    UpdateStateForRealDestroy(index, CM_ENUM_CLASS_TYPE_CMSURFACE3D);

    return CM_SUCCESS;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Allocate 3D surface
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::Allocate3DSurface(uint32_t width, uint32_t height, uint32_t depth, CM_SURFACE_FORMAT format, uint32_t & handle )
{
    CM_RETURN_CODE  hr          = CM_SUCCESS;
    MOS_STATUS      mos_status  = MOS_STATUS_SUCCESS;

    handle = 0;

    CM_HAL_3DRESOURCE_PARAM inParam;
    CmSafeMemSet( &inParam, 0, sizeof( CM_HAL_3DRESOURCE_PARAM ) );
    inParam.iWidth = width;
    inParam.iHeight = height;
    inParam.iDepth = depth;
    inParam.format = format;

    PCM_CONTEXT_DATA pCmData = (PCM_CONTEXT_DATA)m_device->GetAccelData();

    mos_status = pCmData->pCmHalState->pfnAllocate3DResource(pCmData->pCmHalState,&inParam);
    while (mos_status == MOS_STATUS_NO_SPACE)
    {
        if (!TouchSurfaceInPoolForDestroy())
        {
            CM_ASSERTMESSAGE("Error: Failed to flush surface in pool for destroy.");
            return CM_SURFACE_ALLOCATION_FAILURE;
        }
        mos_status = pCmData->pCmHalState->pfnAllocate3DResource(pCmData->pCmHalState,&inParam);
    }
    MOSSTATUS2CM_AND_CHECK(mos_status, hr);

    handle = inParam.dwHandle;

finish:
    return hr;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Free 3D surface
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::Free3DSurface( uint32_t handle )
{
    CM_RETURN_CODE  hr          = CM_SUCCESS;

    PCM_CONTEXT_DATA pCmData = (PCM_CONTEXT_DATA)m_device->GetAccelData();
    
    CHK_MOSSTATUS_RETURN_CMERROR(pCmData->pCmHalState->pfnFree3DResource(pCmData->pCmHalState, (uint32_t)handle));

finish:
    return hr;
}

int32_t CmSurfaceManager::CreateSamplerSurface(CmSurface2DRT* currentSurface2d, SurfaceIndex* & samplerSurfaceIndex, CM_FLAG* flag)
{
    uint32_t index = ValidSurfaceIndexStart();
    CmSurfaceSampler* pCmSurfaceSampler = nullptr;
    SurfaceIndex * surfCurrent = nullptr;
    uint32_t indexForCurrent = 0xFFFFFFFF;

    if (AllocateSurfaceIndex(0, 0, 0, CM_SURFACE_FORMAT_INVALID, index, nullptr) != CM_SUCCESS)
    {
        return CM_EXCEED_SURFACE_AMOUNT;
    }

    uint32_t indexFor2D = 0xFFFFFFFF;

    currentSurface2d->GetIndexFor2D( indexFor2D );
    currentSurface2d->GetIndex(surfCurrent);
    indexForCurrent = surfCurrent->get_data();


    int32_t result = CmSurfaceSampler::Create( index, indexFor2D, indexForCurrent, SAMPLER_SURFACE_TYPE_2D, this, pCmSurfaceSampler, flag);
    if( result != CM_SUCCESS )
    {
        CM_ASSERTMESSAGE("Error: Failed to create CM sampler surface.");
        return result;
    }

    m_surfaceArray[ index ] = pCmSurfaceSampler; 
    pCmSurfaceSampler->GetSurfaceIndex( samplerSurfaceIndex );

    return CM_SUCCESS;
}

int32_t CmSurfaceManager::CreateSamplerSurface(CmSurface2DUPRT* currentSurface2dUP, SurfaceIndex* & samplerSurfaceIndex)
{
    uint32_t index = ValidSurfaceIndexStart();
    CmSurfaceSampler* pCmSurfaceSampler = nullptr;
    SurfaceIndex * surfCurrent = nullptr;
    uint32_t indexForCurrent = 0xFFFFFFFF;

    if (AllocateSurfaceIndex(0, 0, 0, CM_SURFACE_FORMAT_INVALID, index, nullptr) != CM_SUCCESS)
    {
        return CM_EXCEED_SURFACE_AMOUNT;
    }

    uint32_t indexFor2D = 0xFFFFFFFF;

    currentSurface2dUP->GetHandle( indexFor2D );
    currentSurface2dUP->GetIndex(surfCurrent);
    indexForCurrent = surfCurrent->get_data();

    int32_t result = CmSurfaceSampler::Create( index, indexFor2D, indexForCurrent, SAMPLER_SURFACE_TYPE_2DUP, this, pCmSurfaceSampler, nullptr);
    if( result != CM_SUCCESS )
    {
        CM_ASSERTMESSAGE("Error: Failed to create CM sampler surface.");
        return result;
    }

    m_surfaceArray[ index ] = pCmSurfaceSampler; 
    pCmSurfaceSampler->GetSurfaceIndex( samplerSurfaceIndex );

    return CM_SUCCESS;
}

int32_t CmSurfaceManager::CreateSamplerSurface(CmSurface3DRT* currentSurface3d, SurfaceIndex* & samplerSurfaceIndex)
{
    uint32_t index = ValidSurfaceIndexStart();
    CmSurfaceSampler* pCmSurfaceSampler = nullptr;
    SurfaceIndex * surfCurrent = nullptr;
    uint32_t indexForCurrent = 0xFFFFFFFF;

    if (AllocateSurfaceIndex(0, 0, 0, CM_SURFACE_FORMAT_INVALID, index, nullptr) != CM_SUCCESS)
    {
        return CM_EXCEED_SURFACE_AMOUNT;
    }

    uint32_t handleFor3D = 0xFFFFFFFF;
    currentSurface3d->GetHandle( handleFor3D );
    currentSurface3d->GetIndex(surfCurrent);
    indexForCurrent = surfCurrent->get_data();
    int32_t result = CmSurfaceSampler::Create( index, handleFor3D, indexForCurrent, SAMPLER_SURFACE_TYPE_3D, this, pCmSurfaceSampler, nullptr);
    if( result != CM_SUCCESS )  {
        CM_ASSERTMESSAGE("Error: Failed to create CM sampler surface.");
        return result;
    }

    m_surfaceArray[ index ] = pCmSurfaceSampler; 
    pCmSurfaceSampler->GetSurfaceIndex( samplerSurfaceIndex );

    return CM_SUCCESS;
}

int32_t CmSurfaceManager::DestroySamplerSurface(SurfaceIndex* &samplerSurfaceIndex)
{
    if(!samplerSurfaceIndex) {
        return CM_FAILURE;
    }

    uint32_t index = samplerSurfaceIndex->get_data();
    CmSurface* pSurface = m_surfaceArray[ index ]; 

    CmSurfaceSampler* pSurfSampler = nullptr;
    if (pSurface && ( pSurface->Type() == CM_ENUM_CLASS_TYPE_CMSURFACESAMPLER ))
    {
        pSurfSampler = static_cast< CmSurfaceSampler* >( pSurface );
    }

    if(pSurfSampler)  {
        int32_t result = DestroySurface( pSurfSampler);
        if( result == CM_SUCCESS )
        {
            samplerSurfaceIndex = nullptr;
            return CM_SUCCESS;
        }
        else
        {
            return CM_FAILURE;
        }
    }
    else
    {
        return CM_FAILURE;
    }
}

int32_t CmSurfaceManager::DestroySurface( CmSurfaceSampler* & surfaceSampler )
{
    if( !surfaceSampler )
    {
        return CM_FAILURE;
    }

    SurfaceIndex* pIndex = nullptr;
    surfaceSampler->GetSurfaceIndex( pIndex );
    CM_ASSERT( pIndex );
    uint32_t index = pIndex->get_data();

    CM_ASSERT( m_surfaceArray[ index ] == surfaceSampler ); 

    CmSurface* pSurface = surfaceSampler;
    CmSurface::Destroy( pSurface ) ;
    
    UpdateStateForRealDestroy(index, CM_ENUM_CLASS_TYPE_CMSURFACESAMPLER);

    return CM_SUCCESS;
}

uint32_t CmSurfaceManager::GetSurfacePoolSize()
{
    return m_surfaceArraySize;
}

uint32_t CmSurfaceManager::GetSurfaceState(int32_t * &surfState)
{
    surfState =  m_surfaceStates;

    return CM_SUCCESS;
}

int32_t CmSurfaceManager::UpdateSurface2DTableMosResource( uint32_t index, MOS_RESOURCE * mosResource )
{
    PCM_CONTEXT_DATA pCmData = ( PCM_CONTEXT_DATA )m_device->GetAccelData();
    PCM_HAL_STATE pState = pCmData->pCmHalState;

    PCM_HAL_SURFACE2D_ENTRY pEntry = nullptr;
    pEntry = &pState->pUmdSurf2DTable[ index ];
    pEntry->OsResource = *mosResource;
    HalCm_OsResource_Reference( &pEntry->OsResource );

    for ( int i = 0; i < CM_HAL_GPU_CONTEXT_COUNT; i++ )
    {
        pEntry->bReadSync[ i ] = false;
    }
    return CM_SUCCESS;
}

//*-----------------------------------------------------------------------------
//| Purpose: convert from CM_ROTATION TO MHW_ROTATION
//| Returns: Result of the operation
//*-----------------------------------------------------------------------------
MHW_ROTATION CmRotationToMhwRotation(CM_ROTATION cmRotation)
{
    switch (cmRotation)
    {
    case CM_ROTATION_IDENTITY:
        return MHW_ROTATION_IDENTITY;
    case CM_ROTATION_90:
        return MHW_ROTATION_90;
    case CM_ROTATION_180:
        return MHW_ROTATION_180;
    case CM_ROTATION_270:
        return MHW_ROTATION_270;
    default:
        return MHW_ROTATION_IDENTITY;
    }
}

int32_t CmSurfaceManager::UpdateSurface2DTableRotation(uint32_t index, CM_ROTATION rotationFlag)
{
    PCM_CONTEXT_DATA pCmData = (PCM_CONTEXT_DATA)m_device->GetAccelData();
    PCM_HAL_STATE pState = pCmData->pCmHalState;

    PCM_HAL_SURFACE2D_ENTRY pEntry = nullptr;
    pEntry = &pState->pUmdSurf2DTable[index];
    pEntry->rotationFlag = CmRotationToMhwRotation(rotationFlag);

    return CM_SUCCESS;
}

int32_t CmSurfaceManager::UpdateSurface2DTableFrameType(uint32_t index, CM_FRAME_TYPE frameType)
{
    PCM_CONTEXT_DATA pCmData = (PCM_CONTEXT_DATA)m_device->GetAccelData();
    PCM_HAL_STATE pState = pCmData->pCmHalState;

    PCM_HAL_SURFACE2D_ENTRY pEntry = nullptr;
    pEntry = &pState->pUmdSurf2DTable[index];
    pEntry->frameType = frameType;

    return CM_SUCCESS;
}

int32_t CmSurfaceManager::UpdateSurface2DTableChromaSiting(uint32_t index, int32_t chromaSiting)
{
    PCM_CONTEXT_DATA pCmData = (PCM_CONTEXT_DATA)m_device->GetAccelData();
    PCM_HAL_STATE pState = pCmData->pCmHalState;
    PCM_HAL_SURFACE2D_ENTRY pEntry = nullptr;
    pEntry = &pState->pUmdSurf2DTable[index];
    pEntry->chromaSiting = chromaSiting;
    return CM_SUCCESS;
}

int32_t CmSurfaceManager::GetSurfaceBTIInfo()
{
    PCM_HAL_STATE           pCmHalState;
    pCmHalState = ((PCM_CONTEXT_DATA)m_device->GetAccelData())->pCmHalState;

    return pCmHalState->pCmHalInterface->GetHwSurfaceBTIInfo(&m_surfaceBTIInfo);
}

uint32_t CmSurfaceManager::ValidSurfaceIndexStart()
{
    return m_surfaceBTIInfo.dwNormalSurfaceStart;
}

uint32_t CmSurfaceManager::MaxIndirectSurfaceCount()
{
    return (m_surfaceBTIInfo.dwNormalSurfaceEnd - 
            m_surfaceBTIInfo.dwNormalSurfaceStart + 1);
}

//*-----------------------------------------------------------------------------
//| Purpose:    To check if the surface index is CM reserved or not
//| Returns:    Result of the operation.
//*----------------------------------------------------------------------------- 
bool CmSurfaceManager::IsCmReservedSurfaceIndex(uint32_t surfaceBTI)
{
    if(surfaceBTI >= m_surfaceBTIInfo.dwReservedSurfaceStart && 
        surfaceBTI <= m_surfaceBTIInfo.dwReservedSurfaceEnd)
    {
        return true;
    }
    else
    {
        return false;

    }
}

//*-----------------------------------------------------------------------------
//| Purpose:    To check if the surface index a normal surface index
//| Returns:    Result of the operation.
//*----------------------------------------------------------------------------- 
bool CmSurfaceManager::IsValidSurfaceIndex(uint32_t surfaceBTI)
{
    if(surfaceBTI >= m_surfaceBTIInfo.dwNormalSurfaceStart && 
       surfaceBTI <= m_surfaceBTIInfo.dwNormalSurfaceEnd)
    {
        return true;
    }
    else
    {
        return false;

    }
}

int32_t CmSurfaceManager::CreateStateBuffer( CM_STATE_BUFFER_TYPE stateBufferType, uint32_t size, void  *mediaState, CmKernelRT *kernel, CmStateBuffer *&buffer )
{
    int32_t result = CM_SUCCESS;

    uint32_t index = ValidSurfaceIndexStart();
    buffer = nullptr;

    uint32_t handle = 0;

    if ( AllocateSurfaceIndex( size, 0, 0, CM_SURFACE_FORMAT_INVALID, index, nullptr ) != CM_SUCCESS )
    {
        return CM_EXCEED_SURFACE_AMOUNT;
    }

    if ( m_bufferCount >= m_maxBufferCount )
    {
        CM_ASSERTMESSAGE("Error: Exceed the maximum buffer count.");
        return CM_EXCEED_SURFACE_AMOUNT;
    }

    result = AllocateBuffer( size, CM_BUFFER_STATE, handle, nullptr, nullptr );
    if ( result != CM_SUCCESS )
    {
        CM_ASSERTMESSAGE("Error: Failed to allocate state buffer.");
        return result;
    }

    result = CmStateBuffer::Create( index, handle, size, this, stateBufferType, buffer );
    if ( result != CM_SUCCESS )
    {
        CM_ASSERTMESSAGE("Error: Failed to allocate CmStateBuffer.");
        return result;
    }

    m_surfaceArray[ index ] = buffer;
    UpdateProfileFor1DSurface( index, size);

    switch ( stateBufferType )
    {
        case CM_STATE_BUFFER_CURBE:
            break;
        default:
            result = CM_NOT_IMPLEMENTED;
            break;
    }

    return result;
}

//*-----------------------------------------------------------------------------
//| Purpose:    Destroy CmBuffer of state heap in SurfaceManager
//| Returns:    Result of the operation.
//*-----------------------------------------------------------------------------
int32_t CmSurfaceManager::DestroyStateBuffer( CmStateBuffer *&buffer, SURFACE_DESTROY_KIND destroyKind )
{
    int32_t result = CM_SUCCESS;

    if ( !buffer )
    {
        return CM_FAILURE;
    }
    SurfaceIndex* pIndex = nullptr;
    buffer->GetIndex( pIndex );
    CM_ASSERT( pIndex );
    uint32_t index = pIndex->get_data();
    CM_ASSERT( m_surfaceArray[ index ] == buffer );

    if ( destroyKind == FORCE_DESTROY )
    {
        buffer->WaitForReferenceFree();
    }
    else
    {
        //Delayed destroy
        result = UpdateStateForDelayedDestroy( destroyKind, index );

        if ( result != CM_SUCCESS )
        {
            return result;
        }
    }

    //Destroy surface
    CmSurface* pSurface = buffer;
    CmSurface::Destroy( pSurface );

    UpdateStateForRealDestroy( index, CM_ENUM_CLASS_TYPE_CMBUFFER_RT );

    return result;
}

int32_t CMRT_UMD::CmSurfaceManager::CreateMediaStateByCurbeSize( void  *& mediaState, uint32_t curbeSize )
{
    int32_t result = CM_SUCCESS;

    // create media state heap
    PCM_CONTEXT_DATA pCmData = ( PCM_CONTEXT_DATA )m_device->GetAccelData();
    PCM_HAL_STATE pState = pCmData->pCmHalState;

    // Max media state configuration - Curbe, Samplers (3d/AVS/VA), 8x8 sampler table, Media IDs, Kernel Spill area
    RENDERHAL_DYNAMIC_MEDIA_STATE_PARAMS Params;
    Params.iMax8x8Tables = CM_MAX_SAMPLER_8X8_TABLE_SIZE;
    Params.iMaxCurbeOffset = curbeSize;
    Params.iMaxCurbeSize = curbeSize;
    Params.iMaxMediaIDs = CM_MAX_KERNELS_PER_TASK;
    Params.iMaxSamplerIndex3D = CM_MAX_3D_SAMPLER_SIZE;
    Params.iMaxSamplerIndexAVS = CM_MAX_SAMPLER_8X8_TABLE_SIZE;
    Params.iMaxSamplerIndexConv = 0;
    Params.iMaxSamplerIndexMisc = 0;
    Params.iMaxSpillSize = CM_MAX_SPILL_SIZE_PER_THREAD_HEVC;
    Params.iMaxThreads = CM_MAX_USER_THREADS;

    // Prepare Media States to accommodate all parameters - Curbe, Samplers (3d/AVS/VA), 8x8 sampler table, Media IDs
    mediaState = pState->pRenderHal->pfnAssignDynamicState( pState->pRenderHal, &Params, RENDERHAL_COMPONENT_CM );

    return result;
}

int32_t CMRT_UMD::CmSurfaceManager::DestroyMediaState( void  *mediaState )
{
    int32_t result = CM_SUCCESS;

    return result;
}

bool CMRT_UMD::CmSurfaceManager::IsSupportedForSamplerSurface2D(CM_SURFACE_FORMAT format)
{
    switch (format)
    {
        case CM_SURFACE_FORMAT_A16B16G16R16:
        case CM_SURFACE_FORMAT_A16B16G16R16F:
        case CM_SURFACE_FORMAT_A8:
        case CM_SURFACE_FORMAT_A8R8G8B8:
        case CM_SURFACE_FORMAT_YUY2:
        case CM_SURFACE_FORMAT_R32F:
        case CM_SURFACE_FORMAT_R32_UINT:
        case CM_SURFACE_FORMAT_L16:
        case CM_SURFACE_FORMAT_R16_FLOAT:
        case CM_SURFACE_FORMAT_R16G16_UNORM:
        case CM_SURFACE_FORMAT_NV12:
        case CM_SURFACE_FORMAT_L8:
        case CM_SURFACE_FORMAT_AYUV:
        case CM_SURFACE_FORMAT_Y410:
        case CM_SURFACE_FORMAT_Y416:
        case CM_SURFACE_FORMAT_Y210:
        case CM_SURFACE_FORMAT_Y216:
        case CM_SURFACE_FORMAT_P010:
        case CM_SURFACE_FORMAT_P016:
        case CM_SURFACE_FORMAT_YV12:
        case CM_SURFACE_FORMAT_411P:
        case CM_SURFACE_FORMAT_411R:
        case CM_SURFACE_FORMAT_IMC3:
        case CM_SURFACE_FORMAT_I420:
        case CM_SURFACE_FORMAT_422H:
        case CM_SURFACE_FORMAT_422V:
        case CM_SURFACE_FORMAT_444P:
        case CM_SURFACE_FORMAT_RGBP:
        case CM_SURFACE_FORMAT_BGRP:
        case CM_SURFACE_FORMAT_BUFFER_2D:
        case CM_SURFACE_FORMAT_R10G10B10A2:
            return true;

        default:
            CM_ASSERTMESSAGE("Error: Unsupported surface format.");
            return false;
    }
}
}