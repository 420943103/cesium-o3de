#include "Cesium/Systems/CriticalAssetManager.h"
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <AzCore/std/chrono/chrono.h>

namespace Cesium
{
    CriticalAssetManager::CriticalAssetManager()
    {
        AzFramework::AssetCatalogEventBus::Handler::BusConnect();
    }

    CriticalAssetManager::~CriticalAssetManager() noexcept
    {
        m_standardPbrMaterialType.Release();
        m_rasterMaterialType.Release();
    }

    void CriticalAssetManager::OnCatalogLoaded([[maybe_unused]] const char* catalogFile)
    {
        m_standardPbrMaterialType = AZ::RPI::AssetUtils::LoadCriticalAsset<AZ::RPI::MaterialTypeAsset>(STANDARD_PBR_MAT_TYPE);
        m_rasterMaterialType = AZ::RPI::AssetUtils::LoadCriticalAsset<AZ::RPI::MaterialTypeAsset>(RASTER_MAT_TYPE);
        m_materialsLoaded = true;
        AzFramework::AssetCatalogEventBus::Handler::BusDisconnect();
    }

    void CriticalAssetManager::EnsureMaterialTypesLoaded()
    {
        if (m_materialsLoaded)
        {
            return;
        }

        // Kick off loads if they haven't started yet.
        if (!m_standardPbrMaterialType)
        {
            m_standardPbrMaterialType = AZ::RPI::AssetUtils::LoadCriticalAsset<AZ::RPI::MaterialTypeAsset>(STANDARD_PBR_MAT_TYPE);
        }

        if (!m_rasterMaterialType)
        {
            m_rasterMaterialType = AZ::RPI::AssetUtils::LoadCriticalAsset<AZ::RPI::MaterialTypeAsset>(RASTER_MAT_TYPE);
        }

        // Wait a short, bounded time for readiness. Cesium may begin loading tiles
        // before the AssetCatalogLoaded event fires.
        const auto waitFor = [](AZ::Data::Asset<AZ::RPI::MaterialTypeAsset>& asset, AZStd::chrono::milliseconds timeout)
        {
            const auto start = AZStd::chrono::steady_clock::now();
            while (asset && !asset.IsReady())
            {
                AZ::Data::AssetBus::ExecuteQueuedEvents();
                AZStd::this_thread::sleep_for(AZStd::chrono::milliseconds(10));
                if (AZStd::chrono::steady_clock::now() - start > timeout)
                {
                    break;
                }
            }
        };

        waitFor(m_standardPbrMaterialType, AZStd::chrono::milliseconds(3000));
        waitFor(m_rasterMaterialType, AZStd::chrono::milliseconds(3000));

        m_materialsLoaded = m_standardPbrMaterialType && m_standardPbrMaterialType.IsReady();
    }

    AZ::Data::AssetId CriticalAssetManager::GenerateRandomAssetId() const
    {
        static std::atomic_uint32_t subId = 0;
        return AZ::Data::AssetId(AZ::Uuid::CreateRandom(), subId.fetch_add(1, std::memory_order_relaxed));
    }
} // namespace Cesium
