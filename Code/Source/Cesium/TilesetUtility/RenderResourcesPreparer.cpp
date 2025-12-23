#include "Cesium/TilesetUtility/RenderResourcesPreparer.h"
#include "Cesium/TilesetUtility/GltfRasterMaterialBuilder.h"
#include "Cesium/Gltf/GltfModelBuilder.h"
#include "Cesium/Gltf/GltfLoadContext.h"
#include "Cesium/Gltf/GltfPBRMaterialBuilder.h"
#include <Atom/Feature/Mesh/MeshFeatureProcessorInterface.h>
#include <Atom/RPI.Reflect/Image/StreamingImageAssetCreator.h>
#include <Atom/RPI.Reflect/Image/ImageMipChainAssetCreator.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/Debug/Trace.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstring>

// Window 10 wingdi.h header defines OPAQUE macro which mess up with CesiumGltf::Material::AlphaMode::OPAQUE.
// This only happens with unity build
#include <AzCore/PlatformDef.h>
#ifdef AZ_COMPILER_MSVC
#pragma push_macro("OPAQUE")
#undef OPAQUE
#endif

#include <Cesium3DTilesSelection/Tile.h>
#include <Cesium3DTilesSelection/Tileset.h>
#include <CesiumGltf/Model.h>
#include <CesiumUtility/JsonValue.h>

#ifdef AZ_COMPILER_MSVC
#pragma pop_macro("OPAQUE")
#endif

namespace Cesium
{
    RenderResourcesPreparer::RenderResourcesPreparer(AZ::Render::MeshFeatureProcessorInterface* meshFeatureProcessor)
        : m_meshFeatureProcessor{ meshFeatureProcessor }
        , m_transform{ 1.0 }
        , m_modelBuilder{ AZStd::make_unique<GltfModelBuilder>(AZStd::make_unique<GltfPBRMaterialBuilder>()) }
    {
        m_freeRasterLayers.reserve(GltfRasterMaterialBuilder::MAX_RASTER_LAYERS);
        for (std::uint32_t i = 0; i < GltfRasterMaterialBuilder::MAX_RASTER_LAYERS; ++i)
        {
            m_freeRasterLayers.emplace_back(i);
        }

        AZ::TickBus::Handler::BusConnect();
    }

    RenderResourcesPreparer::~RenderResourcesPreparer() noexcept
    {
        AZ::TickBus::Handler::BusDisconnect();

        for (auto& intrusiveModel : m_intrusiveModels)
        {
            // move the handler out before free it. Otherwise, stack overflow
            auto handler = std::move(intrusiveModel.m_self);
            handler.Free();
        }
    }

    void RenderResourcesPreparer::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        auto it = AZStd::remove_if(
            m_compileMaterialsQueue.begin(), m_compileMaterialsQueue.end(),
            [](auto& material)
            {
                return !material->NeedsCompile() || material->Compile();
            });
        m_compileMaterialsQueue.erase(it, m_compileMaterialsQueue.end());
    }

    void RenderResourcesPreparer::SetTransform(const glm::dmat4& transform)
    {
        m_transform = transform;
        for (auto& intrusiveModel : m_intrusiveModels)
        {
            intrusiveModel.m_model.SetTransform(transform);
        }
    }

    const glm::dmat4& RenderResourcesPreparer::GetTransform() const
    {
        return m_transform;
    }

    void RenderResourcesPreparer::SetVisible(void* renderResources, bool visible)
    {
        if (renderResources)
        {
            IntrusiveGltfModel* intrusiveModel = reinterpret_cast<IntrusiveGltfModel*>(renderResources);
            if (intrusiveModel->m_model.IsVisible() != visible)
            {
                intrusiveModel->m_model.SetVisible(visible);
            }
        }
    }

    bool RenderResourcesPreparer::AddRasterLayer(const CesiumRasterOverlays::RasterOverlay* rasterOverlay)
    {
        AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] AddRasterLayer called - RasterOverlay: %p\n", rasterOverlay);
        if (m_freeRasterLayers.empty())
        {
            AZ_Warning("Cesium", false, "[RenderResourcesPreparer] AddRasterLayer FAILED - No free raster layers available\n");
            return false;
        }

        if (m_rasterOverlayLayers.find(rasterOverlay) == m_rasterOverlayLayers.end())
        {
            m_rasterOverlayLayers.insert(AZStd::make_pair(rasterOverlay, m_freeRasterLayers.back()));
            m_freeRasterLayers.pop_back();
        }

        AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] SUCCESS - RasterOverlay added to layers\n");
        return true;
    }

    void RenderResourcesPreparer::RemoveRasterLayer(const CesiumRasterOverlays::RasterOverlay* rasterOverlay)
    {
        auto layerIt = m_rasterOverlayLayers.find(rasterOverlay);
        if (layerIt == m_rasterOverlayLayers.end())
        {
            return;
        }

        const std::uint32_t freedLayer = layerIt->second;
        m_rasterOverlayLayers.erase(layerIt);
        m_freeRasterLayers.emplace_back(freedLayer);
    }

    CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources> RenderResourcesPreparer::prepareInLoadThread(
        const CesiumAsync::AsyncSystem& asyncSystem,
        Cesium3DTilesSelection::TileLoadResult&& tileLoadResult,
        const glm::dmat4& transform,
        const std::any& rendererOptions)
    {
        AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] prepareInLoadThread called - Processing tile\n");
        CesiumGltf::Model* pModel = std::get_if<CesiumGltf::Model>(&tileLoadResult.contentKind);
        if (!pModel)
        {
            AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] prepareInLoadThread - Tile content is not a Model\n");
            return asyncSystem.createResolvedFuture(
                Cesium3DTilesSelection::TileLoadResultAndRenderResources{ std::move(tileLoadResult), nullptr });
        }
        
        AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] prepareInLoadThread - Model found, creating GltfLoadModel\n");
        AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] prepareInLoadThread - Terrain tile model: Meshes: %zu, Materials: %zu\n",
            pModel->meshes.size(), pModel->materials.size());
        
        // Create GltfLoadModel from CesiumGltf::Model
        GltfLoadModel* loadModel = new GltfLoadModel();
        GltfModelBuilderOption option{ transform };
        m_modelBuilder->Create(*pModel, option, *loadModel);
        
        AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] prepareInLoadThread - GltfLoadModel created - Meshes: %zu, Materials: %zu, Textures: %zu\n", 
            loadModel->m_meshes.size(), loadModel->m_materials.size(), loadModel->m_textures.size());
        
        // Log mesh information to help identify LOD level
        for (size_t i = 0; i < loadModel->m_meshes.size(); ++i)
        {
            const auto& mesh = loadModel->m_meshes[i];
            AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] prepareInLoadThread - Mesh %zu: Primitives: %zu\n",
                i, mesh.m_primitives.size());
        }
        
        // Check if materials are valid
        for (size_t i = 0; i < loadModel->m_materials.size(); ++i)
        {
            if (!loadModel->m_materials[i].m_materialAsset || !loadModel->m_materials[i].m_materialAsset.IsReady())
            {
                AZ_Warning("Cesium", false, 
                    "[RenderResourcesPreparer] prepareInLoadThread - Material %zu is null or not ready\n", i);
            }
            else
            {
                AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] prepareInLoadThread - Material %zu is valid and ready\n", i);
            }
        }
        
        return asyncSystem.createResolvedFuture(
            Cesium3DTilesSelection::TileLoadResultAndRenderResources{ std::move(tileLoadResult), loadModel });
    }

    void* RenderResourcesPreparer::prepareInMainThread([[maybe_unused]] Cesium3DTilesSelection::Tile& tile, void* pLoadThreadResult)
    {
        AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] prepareInMainThread called - Tile: %p, LoadResult: %p\n", 
            &tile, pLoadThreadResult);
        if (pLoadThreadResult)
        {
            // we destroy loadModel after main thread is done
            AZStd::unique_ptr<GltfLoadModel> loadModel{ reinterpret_cast<GltfLoadModel*>(pLoadThreadResult) };
            
            // Check materials before creating model
            for (size_t i = 0; i < loadModel->m_materials.size(); ++i)
            {
                if (!loadModel->m_materials[i].m_materialAsset || !loadModel->m_materials[i].m_materialAsset.IsReady())
                {
                    AZ_Warning("Cesium", false, 
                        "[RenderResourcesPreparer] prepareInMainThread - Material %zu is null or not ready, model may not render correctly\n", i);
                }
            }
            
            auto handle = m_intrusiveModels.emplace(GltfModel(m_meshFeatureProcessor, *loadModel));
            IntrusiveGltfModel& intrusiveModel = *handle;
            intrusiveModel.m_self = std::move(handle);
            intrusiveModel.m_model.SetTransform(m_transform);
            intrusiveModel.m_model.SetVisible(false);
            AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] prepareInMainThread SUCCESS - Model created and added to intrusive models\n");
            return &intrusiveModel;
        }

        AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] prepareInMainThread - No load result, returning nullptr\n");
        return nullptr;
    }

    void RenderResourcesPreparer::free(
        [[maybe_unused]] Cesium3DTilesSelection::Tile& tile, void* pLoadThreadResult, void* pMainThreadResult) noexcept
    {
        if (pLoadThreadResult)
        {
            GltfLoadModel* loadModel = reinterpret_cast<GltfLoadModel*>(pLoadThreadResult);
            delete loadModel;
        }

        if (pMainThreadResult)
        {
            IntrusiveGltfModel* intrusiveModel = reinterpret_cast<IntrusiveGltfModel*>(pMainThreadResult);
            auto handler = std::move(intrusiveModel->m_self); // move the handler out before free it. Otherwise, stack overflow
            handler.Free();
        }
    }

    void* RenderResourcesPreparer::prepareRasterInLoadThread(CesiumGltf::ImageAsset& image, const std::any& /*rendererOptions*/)
    {
        AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] prepareRasterInLoadThread called - Image: %dx%d, PixelData Size: %zu\n",
            image.width, image.height, image.pixelData.size());
        
        // Validate image dimensions and pixel data
        // Check actual channels and bytesPerChannel from ImageAsset
        AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] prepareRasterInLoadThread - ImageAsset properties:\n");
        AZ_TracePrintf("Cesium", "  - Width: %d, Height: %d\n", image.width, image.height);
        AZ_TracePrintf("Cesium", "  - Channels: %d, BytesPerChannel: %d\n", image.channels, image.bytesPerChannel);
        AZ_TracePrintf("Cesium", "  - PixelData size: %zu bytes\n", image.pixelData.size());
        
        // Validate tile dimensions - warn if non-standard size detected
        // While Cesium can generate variable-sized tiles, non-standard sizes may indicate issues
        if (image.width != 256 || image.height != 256)
        {
            AZ_Warning("Cesium", false,
                "[RenderResourcesPreparer] prepareRasterInLoadThread - WARNING: Non-standard tile size %dx%d detected!\n"
                "  Expected: 256x256 (standard tile size)\n"
                "  Actual: %dx%d\n"
                "  This may cause texture display issues. Please verify:\n"
                "  1. Data source configuration (URL template, tile size settings)\n"
                "  2. LOD level settings (maximumScreenSpaceError)\n"
                "  3. Projection coordinate system settings\n"
                "  4. Whether the image data is correctly loaded from source\n",
                image.width, image.height, image.width, image.height);
        }
        
        // Use actual channels and bytesPerChannel from ImageAsset instead of assuming RGBA
        const uint32_t actualChannels = static_cast<uint32_t>(image.channels);
        const uint32_t actualBytesPerChannel = static_cast<uint32_t>(image.bytesPerChannel);
        const size_t expectedPixelDataSize = static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * actualChannels * actualBytesPerChannel;
        const size_t actualPixelDataSize = image.pixelData.size();
        
        // Warn if channels are not RGBA (4 channels) as expected for textures
        if (actualChannels != 4)
        {
            AZ_Warning("Cesium", false,
                "[RenderResourcesPreparer] prepareRasterInLoadThread - WARNING: Image has %d channels, expected 4 (RGBA). "
                "This may cause incorrect texture rendering.\n", actualChannels);
        }
        
        AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] prepareRasterInLoadThread - Image validation:\n");
        AZ_TracePrintf("Cesium", "  - Dimensions: %dx%d (Cesium DOM tiles may have variable dimensions)\n", image.width, image.height);
        AZ_TracePrintf("Cesium", "  - Channels: %d, BytesPerChannel: %d\n", actualChannels, actualBytesPerChannel);
        AZ_TracePrintf("Cesium", "  - Expected pixel data size: %zu bytes (%dx%dx%dx%d)\n", 
            expectedPixelDataSize, image.width, image.height, actualChannels, actualBytesPerChannel);
        AZ_TracePrintf("Cesium", "  - Actual pixel data size: %zu bytes\n", actualPixelDataSize);
        
        // Note: Cesium DOM (Digital Orthophoto Map) tiles may have variable dimensions
        // based on geographic coverage, not always 256x256. This is normal behavior.
        // We should accept any dimensions as long as pixel data size matches.
        
        if (actualPixelDataSize != expectedPixelDataSize)
        {
            AZ_Error("Cesium", false, 
                "[RenderResourcesPreparer] prepareRasterInLoadThread - ERROR: Pixel data size mismatch! "
                "Expected %zu bytes (%dx%dx%dx%d), but got %zu bytes. "
                "This indicates incorrect image parsing or format assumption and will cause pixel misalignment.\n",
                expectedPixelDataSize, image.width, image.height, actualChannels, actualBytesPerChannel, actualPixelDataSize);
        }
        
        // Check if dimensions are power of 2 (common for texture tiles)
        bool isPowerOf2Width = (image.width & (image.width - 1)) == 0;
        bool isPowerOf2Height = (image.height & (image.height - 1)) == 0;
        if (!isPowerOf2Width || !isPowerOf2Height)
        {
            AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] prepareRasterInLoadThread - Note: Image dimensions are not power of 2 (width: %d, height: %d)\n",
                image.width, image.height);
        }
        
        if (!image.pixelData.empty() && image.width != 0 && image.height != 0)
        {
            // Validate pixel data size matches expected size
            if (actualPixelDataSize != expectedPixelDataSize)
            {
                AZ_Error("Cesium", false, 
                    "[RenderResourcesPreparer] prepareRasterInLoadThread - ERROR: Cannot proceed with mismatched pixel data size. "
                    "Expected %zu bytes (%dx%dx%dx%d), got %zu bytes. "
                    "This will cause pixel misalignment and texture corruption.\n",
                    expectedPixelDataSize, image.width, image.height, actualChannels, actualBytesPerChannel, actualPixelDataSize);
                return nullptr;
            }
            
            // Ensure image has RGBA format (4 channels) for texture rendering
            if (actualChannels != 4)
            {
                AZ_Warning("Cesium", false,
                    "[RenderResourcesPreparer] prepareRasterInLoadThread - Converting image from %d channels to 4 channels (RGBA)\n",
                    actualChannels);
                // Convert to RGBA if not already
                image.changeNumberOfChannels(4, std::byte{255}); // Use 255 (opaque) for alpha channel if missing
            }
            
            // image has 4 channels (RGBA), so we just copy the data over
            AZ::RHI::ImageDescriptor imageDesc;
            imageDesc.m_bindFlags = AZ::RHI::ImageBindFlags::ShaderRead;
            imageDesc.m_dimension = AZ::RHI::ImageDimension::Image2D;
            imageDesc.m_size = AZ::RHI::Size(image.width, image.height, 1);
            imageDesc.m_format = AZ::RHI::Format::R8G8B8A8_UNORM_SRGB;

            auto imageSubresourceLayout =
                AZ::RHI::GetImageSubresourceLayout(imageDesc, AZ::RHI::ImageSubresource{});

            // Calculate expected values for validation
            constexpr size_t bytesPerPixel = 4; // RGBA = 4 bytes per pixel
            const size_t expectedBytesPerRow = static_cast<size_t>(image.width) * bytesPerPixel;
            const size_t expectedTightlyPackedSize = expectedBytesPerRow * static_cast<size_t>(image.height);
            
            // Log detailed upload parameters (critical for debugging row pitch issues)
            AZ_TracePrintf("Cesium", 
                "[RenderResourcesPreparer] prepareRasterInLoadThread - UPLOAD PARAMETERS (CRITICAL FOR DEBUGGING):\n"
                "  - tileWidth: %d, tileHeight: %d\n"
                "  - bytesPerPixel: %zu (RGBA)\n"
                "  - expectedBytesPerRow (tightly packed): %zu (= width * bytesPerPixel)\n"
                "  - actualBytesPerRow (from layout): %zu\n"
                "  - rowCount: %u\n"
                "  - expectedTightlyPackedSize: %zu (= width * height * bytesPerPixel)\n"
                "  - actualPixelDataSize: %zu\n"
                "  - bytesPerImage (from layout): %zu\n"
                "  - rowPadding: %zu bytes (if any)\n",
                image.width, image.height,
                bytesPerPixel,
                expectedBytesPerRow,
                imageSubresourceLayout.m_bytesPerRow,
                imageSubresourceLayout.m_rowCount,
                expectedTightlyPackedSize,
                actualPixelDataSize,
                imageSubresourceLayout.m_bytesPerImage,
                imageSubresourceLayout.m_bytesPerRow > expectedBytesPerRow ? 
                    (imageSubresourceLayout.m_bytesPerRow - expectedBytesPerRow) : 0);

            // Verify layout matches our pixel data size
            // Note: bytesPerImage may include row padding/alignment, so we need to handle row-by-row copy
            if (imageSubresourceLayout.m_bytesPerImage != actualPixelDataSize)
            {
                AZ_TracePrintf("Cesium", 
                    "[RenderResourcesPreparer] prepareRasterInLoadThread - ImageSubresourceLayout: "
                    "bytesPerImage=%zu, bytesPerRow=%zu, rowCount=%u, actualPixelDataSize=%zu\n",
                    imageSubresourceLayout.m_bytesPerImage, 
                    imageSubresourceLayout.m_bytesPerRow,
                    imageSubresourceLayout.m_rowCount,
                    actualPixelDataSize);
            }
            
            // Critical check: bytesPerRow must match expected value or be properly aligned
            // Wrong bytesPerRow causes row misalignment and stripe artifacts
            if (imageSubresourceLayout.m_bytesPerRow < expectedBytesPerRow)
            {
                AZ_Error("Cesium", false,
                    "[RenderResourcesPreparer] prepareRasterInLoadThread - CRITICAL ERROR: bytesPerRow (%zu) < expected (%zu)! "
                    "This will cause severe pixel misalignment and stripe artifacts.\n",
                    imageSubresourceLayout.m_bytesPerRow, expectedBytesPerRow);
                return nullptr;
            }
            
            // If bytesPerRow doesn't match tightly packed data, we need to copy row by row
            if (imageSubresourceLayout.m_bytesPerRow != expectedBytesPerRow)
            {
                AZ_Warning("Cesium", false,
                    "[RenderResourcesPreparer] prepareRasterInLoadThread - WARNING: Row stride mismatch! "
                    "Expected tightly packed data (%zu bytes/row), but layout requires %zu bytes/row. "
                    "This may cause pixel misalignment. Will copy row-by-row with padding.\n",
                    expectedBytesPerRow, imageSubresourceLayout.m_bytesPerRow);
            }

            // Create mip chain - DISABLE MIPMAP to prevent sampling artifacts
            // Mipmaps can cause issues with texture atlases, so we only create level 0
            AZ::RPI::ImageMipChainAssetCreator mipChainCreator;
            mipChainCreator.Begin(AZ::Uuid::CreateRandom(), 1, 1); // Only 1 mip level (no mipmap)
            mipChainCreator.BeginMip(imageSubresourceLayout);
            
            // Copy pixel data row by row if there's row padding, otherwise copy directly
            const size_t srcRowSize = static_cast<size_t>(image.width) * bytesPerPixel;
            const size_t dstRowSize = imageSubresourceLayout.m_bytesPerRow;
            const uint8_t* srcData = reinterpret_cast<const uint8_t*>(image.pixelData.data());
            
            if (dstRowSize == srcRowSize)
            {
                // Tightly packed data - copy directly
                mipChainCreator.AddSubImage(srcData, image.pixelData.size());
            }
            else
            {
                // Row padding exists, copy row by row with proper alignment
                AZStd::vector<uint8_t> paddedData(imageSubresourceLayout.m_bytesPerImage);
                uint8_t* dstData = paddedData.data();
                
                for (uint32_t row = 0; row < static_cast<uint32_t>(image.height); ++row)
                {
                    const uint8_t* srcRow = srcData + row * srcRowSize;
                    uint8_t* dstRow = dstData + row * dstRowSize;
                    memcpy(dstRow, srcRow, srcRowSize);
                    
                    // Zero-fill padding bytes if any
                    if (dstRowSize > srcRowSize)
                    {
                        memset(dstRow + srcRowSize, 0, dstRowSize - srcRowSize);
                    }
                }
                
                mipChainCreator.AddSubImage(paddedData.data(), paddedData.size());
            }
            
            mipChainCreator.EndMip();
            AZ::Data::Asset<AZ::RPI::ImageMipChainAsset> mipChainAsset;
            mipChainCreator.End(mipChainAsset);

            if (!mipChainAsset || !mipChainAsset.IsReady())
            {
                AZ_Error("Cesium", false, "[RenderResourcesPreparer] prepareRasterInLoadThread - FAILED to create ImageMipChainAsset\n");
                return nullptr;
            }

            // Create streaming image
            AZ::RPI::StreamingImageAssetCreator imageCreator;
            imageCreator.Begin(AZ::Uuid::CreateRandom());
            imageCreator.SetImageDescriptor(imageDesc);
            imageCreator.AddMipChainAsset(*mipChainAsset);

            AZ::Data::Asset<AZ::RPI::StreamingImageAsset> imageAsset;
            imageCreator.End(imageAsset);

            if (imageAsset && imageAsset.IsReady())
            {
                AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] prepareRasterInLoadThread SUCCESS - StreamingImageAsset created: %dx%d\n",
                    image.width, image.height);
                auto rasterOverlay = new RasterOverlay();
                rasterOverlay->m_imageAsset = std::move(imageAsset);
                return rasterOverlay;
            }
            else
            {
                AZ_Error("Cesium", false, "[RenderResourcesPreparer] prepareRasterInLoadThread FAILED - StreamingImageAsset creation failed or not ready\n");
            }
        }
        else
        {
            AZ_Warning("Cesium", false, 
                "[RenderResourcesPreparer] prepareRasterInLoadThread FAILED - Image data is empty or dimensions are zero. "
                "Width: %d, Height: %d, PixelData size: %zu\n",
                image.width, image.height, image.pixelData.size());
        }

        return nullptr;
    }

    void* RenderResourcesPreparer::prepareRasterInMainThread(
        [[maybe_unused]] CesiumRasterOverlays::RasterOverlayTile& rasterTile, void* pLoadThreadResult)
    {
        if (pLoadThreadResult)
        {
            auto rasterOverlay = reinterpret_cast<RasterOverlay*>(pLoadThreadResult);
            rasterOverlay->m_image = AZ::RPI::StreamingImage::FindOrCreate(rasterOverlay->m_imageAsset);
            
            // Log raster tile information for LOD debugging
            if (rasterOverlay->m_imageAsset && rasterOverlay->m_imageAsset.IsReady())
            {
                const auto& imageDescriptor = rasterOverlay->m_imageAsset->GetImageDescriptor();
                AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] prepareRasterInMainThread - Raster image created: %dx%d (LOD debugging)\n",
                    imageDescriptor.m_size.m_width, imageDescriptor.m_size.m_height);
            }
            
            return rasterOverlay;
        }

        return nullptr;
    }

    void RenderResourcesPreparer::freeRaster(
        [[maybe_unused]] const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
        void* pLoadThreadResult,
        void* pMainThreadResult) noexcept
    {
        if (pLoadThreadResult)
        {
            RasterOverlay* rasterOverlay = reinterpret_cast<RasterOverlay*>(pLoadThreadResult);
            delete rasterOverlay;
        }

        if (pMainThreadResult)
        {
            RasterOverlay* rasterOverlay = reinterpret_cast<RasterOverlay*>(pMainThreadResult);
            delete rasterOverlay;
        }
    }

    void RenderResourcesPreparer::attachRasterInMainThread(
        const Cesium3DTilesSelection::Tile& tile,
        std::int32_t overlayTextureCoordinateID,
        const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
        void* mainThreadRasterResources,
        const glm::dvec2& translation,
        const glm::dvec2& scale)
    {
        AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] attachRasterInMainThread called - Tile State: %d, OverlayTextureCoordinateID: %d\n", 
            static_cast<int>(tile.getState()), overlayTextureCoordinateID);
        
        // Log translation and scale - these indicate how the raster tile maps to the terrain tile
        // If scale is not (1.0, 1.0), it may indicate LOD mismatch
        AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] attachRasterInMainThread - Translation: (%.6f, %.6f), Scale: (%.6f, %.6f)\n",
            translation.x, translation.y, scale.x, scale.y);
        
        // Calculate estimated LOD level difference from scale
        // Scale = 2^n means raster is n levels higher than terrain
        // Scale = 1/2^n means raster is n levels lower than terrain
        float estimatedLevelDiffX = 0.0f;
        float estimatedLevelDiffY = 0.0f;
        if (scale.x > 1.0)
        {
            estimatedLevelDiffX = static_cast<float>(std::log2(scale.x));
        }
        else if (scale.x > 0.0)
        {
            estimatedLevelDiffX = -static_cast<float>(std::log2(1.0 / scale.x));
        }
        if (scale.y > 1.0)
        {
            estimatedLevelDiffY = static_cast<float>(std::log2(scale.y));
        }
        else if (scale.y > 0.0)
        {
            estimatedLevelDiffY = -static_cast<float>(std::log2(1.0 / scale.y));
        }
        
        // Check if scale indicates LOD mismatch (scale should be close to 1.0 for matching LODs)
        if (scale.x < 0.5 || scale.x > 2.0 || scale.y < 0.5 || scale.y > 2.0)
        {
            AZ_Warning("Cesium", false, 
                "[RenderResourcesPreparer] attachRasterInMainThread - WARNING: Scale values (%.6f, %.6f) suggest LOD mismatch! "
                "Estimated level difference: X=%.1f levels, Y=%.1f levels. "
                "Expected scale close to (1.0, 1.0) for matching LOD levels. "
                "This may cause texture stretching or incorrect imagery display.\n",
                scale.x, scale.y, estimatedLevelDiffX, estimatedLevelDiffY);
        }
        else
        {
            AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] attachRasterInMainThread - Scale values indicate LOD levels are well matched (difference: X=%.1f, Y=%.1f levels)\n",
                estimatedLevelDiffX, estimatedLevelDiffY);
        }
        
        if (tile.getState() == Cesium3DTilesSelection::TileLoadState::Done)
        {
            void* tileRenderResource = nullptr;
            auto renderContent = tile.getContent().getRenderContent();
            if (renderContent)
            {
                tileRenderResource = renderContent->getRenderResources();
            }
            else
            {
                AZ_Warning("Cesium", false, "[RenderResourcesPreparer] attachRasterInMainThread - Tile has no render content\n");
            }
            
            AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] attachRasterInMainThread - TileRenderResource: %p, MainThreadRasterResources: %p\n", 
                tileRenderResource, mainThreadRasterResources);
            
            if (!tileRenderResource)
            {
                AZ_Warning("Cesium", false, "[RenderResourcesPreparer] attachRasterInMainThread - TileRenderResource is null, cannot attach raster\n");
                return;
            }
            
            if (!mainThreadRasterResources)
            {
                AZ_Warning("Cesium", false, "[RenderResourcesPreparer] attachRasterInMainThread - MainThreadRasterResources is null, cannot attach raster\n");
                return;
            }
            
            if (tileRenderResource && mainThreadRasterResources)
            {
                // find the layer of the raster
                const auto& currentRasterOverlay = rasterTile.getOverlay();
                auto layerIt = m_rasterOverlayLayers.find(&currentRasterOverlay);
                if (layerIt == m_rasterOverlayLayers.end())
                {
                    AZ_Warning("Cesium", false, "[RenderResourcesPreparer] attachRasterInMainThread - RasterOverlay not found in layers\n");
                    return;
                }
                
                AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] attachRasterInMainThread - RasterOverlay found, layer: %u\n", layerIt->second);
                std::uint32_t layer = layerIt->second;

                IntrusiveGltfModel* intrusiveGltfModel = reinterpret_cast<IntrusiveGltfModel*>(tileRenderResource);
                RasterOverlay* rasterOverlay = reinterpret_cast<RasterOverlay*>(mainThreadRasterResources);
                GltfRasterMaterialBuilder materialBuilder;
                GltfModel& model = intrusiveGltfModel->m_model;
                
                AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] attachRasterInMainThread - Processing materials, Material count: %zu\n", 
                    model.GetMaterials().size());
                
                for (auto& material : model.GetMaterials())
                {
                    if (!material.m_material)
                    {
                        continue;
                    }

                    AZ::Vector4 uvTranslateScale{ static_cast<float>(translation.x), static_cast<float>(translation.y),
                                                  static_cast<float>(scale.x), static_cast<float>(scale.y) };
                    
                    AZ_TracePrintf("Cesium", "[RenderResourcesPreparer] attachRasterInMainThread - Setting UV TranslateScale: (%.6f, %.6f, %.6f, %.6f) for material\n",
                        uvTranslateScale.GetX(), uvTranslateScale.GetY(), uvTranslateScale.GetZ(), uvTranslateScale.GetW());

                    // Just update material with raster if the current material can compile, so material can be updated right away
                    // in the next frame. Otherwise, we create the new material with the attached raster, so that the primitive is
                    // updated with the new material in the next frame. If we only update the material and not create new material
                    // the terrain can be rendered with old material if that material is still compiling and flickering can happen
                    bool canCompile = material.m_material->CanCompile();
                    if (canCompile)
                    {
                        canCompile = materialBuilder.SetRasterForMaterial(
                            layer, rasterOverlay->m_image, static_cast<std::uint32_t>(overlayTextureCoordinateID), uvTranslateScale,
                            material.m_material);
                    }

                    if (!canCompile)
                    {
                        auto materialAsset = materialBuilder.CreateRasterMaterial(
                            layer, rasterOverlay->m_imageAsset, static_cast<std::uint32_t>(overlayTextureCoordinateID), uvTranslateScale,
                            material.m_material->GetAsset());
                        material.m_material = AZ::RPI::Material::FindOrCreate(materialAsset);
                    }
                }

                for (auto& mesh : model.GetMeshes())
                {
                    for (auto& primitive : mesh.m_primitives)
                    {
                        model.UpdateMaterialForPrimitive(primitive);
                    }
                }
            }
        }
    }

    void RenderResourcesPreparer::detachRasterInMainThread(
        const Cesium3DTilesSelection::Tile& tile,
        [[maybe_unused]] std::int32_t overlayTextureCoordinateID,
        const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
        void* mainThreadRasterResources) noexcept
    {
        if (tile.getState() == Cesium3DTilesSelection::TileLoadState::Done)
        {
            void* tileRenderResource = tile.getContent().getRenderContent()->getRenderResources();
            if (tileRenderResource && mainThreadRasterResources)
            {
                // find the layer of the raster
                const auto& currentRasterOverlay = rasterTile.getOverlay();
                auto layerIt = m_rasterOverlayLayers.find(&currentRasterOverlay);
                if (layerIt == m_rasterOverlayLayers.end())
                {
                    return;
                }
                std::uint32_t layer = layerIt->second;

                IntrusiveGltfModel* intrusiveGltfModel = reinterpret_cast<IntrusiveGltfModel*>(tileRenderResource);
                GltfRasterMaterialBuilder materialBuilder;
                GltfModel& model = intrusiveGltfModel->m_model;
                for (auto& material : model.GetMaterials())
                {
                    if (!material.m_material)
                    {
                        continue;
                    }

                    bool compile = materialBuilder.UnsetRasterForMaterial(layer, material.m_material);

                    // it's not guaranteed that the material will be able to compile right away, so we add it to the queue to compile later
                    if (!compile)
                    {
                        m_compileMaterialsQueue.emplace_back(material.m_material);
                    }
                }
            }
        }
    }

    AZStd::optional<glm::dvec3> RenderResourcesPreparer::GetRTCFromGltf(const CesiumGltf::Model& model)
    {
        const CesiumUtility::JsonValue& extras = model.extras;
        const CesiumUtility::JsonValue* rtcObj = extras.getValuePtrForKey(CESIUM_RTC_CENTER_EXTRA);
        if (!rtcObj)
        {
            return AZStd::nullopt;
        }

        if (!rtcObj->isArray())
        {
            return AZStd::nullopt;
        }

        const auto& array = rtcObj->getArray();
        if (array.size() != 3)
        {
            return AZStd::nullopt;
        }

        glm::dvec3 rtc{ 0.0 };
        rtc.x = array[0].getDoubleOrDefault(0.0);
        rtc.y = array[1].getDoubleOrDefault(0.0);
        rtc.z = array[2].getDoubleOrDefault(0.0);
        return rtc;
    }
} // namespace Cesium
