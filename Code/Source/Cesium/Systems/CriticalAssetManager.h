#pragma once

#include <Atom/RPI.Reflect/Material/MaterialTypeAsset.h>
#include <AzFramework/Asset/AssetCatalogBus.h>
#include <AzCore/Asset/AssetCommon.h>
#include <atomic>

namespace Cesium
{
    class CriticalAssetManager : public AzFramework::AssetCatalogEventBus::Handler
    {
    public:
        CriticalAssetManager();

        ~CriticalAssetManager() noexcept;

        void OnCatalogLoaded(const char* catalogFile) override;

        // Ensure material types are loaded even if catalog event hasn't fired yet
        void EnsureMaterialTypesLoaded();

        AZ::Data::AssetId GenerateRandomAssetId() const;

        AZ::Data::Asset<AZ::RPI::MaterialTypeAsset> m_standardPbrMaterialType;
        AZ::Data::Asset<AZ::RPI::MaterialTypeAsset> m_rasterMaterialType;

    private:
        // StandardPBR for general geometry, GltfStandardPBR for TMS imagery with raster overlays
        static constexpr const char* const STANDARD_PBR_MAT_TYPE = "Materials/Types/GltfStandardPBR.azmaterialtype";
        static constexpr const char* const RASTER_MAT_TYPE = "Materials/Types/GltfStandardPBR.azmaterialtype";
        bool m_materialsLoaded = false;
    };
} // namespace Cesium
