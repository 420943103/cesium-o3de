#pragma once

#include <Cesium/Components/RasterOverlayComponent.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/optional.h>
#include <AzCore/std/containers/map.h>
#include <cstdint>
#include <memory>

namespace Cesium
{
    struct UrlTemplateRasterOverlaySource final
    {
        AZ_RTTI(UrlTemplateRasterOverlaySource, "{0A2B78F3-A2B6-44FE-8091-2A3CCBA07FC7}");
        AZ_CLASS_ALLOCATOR(UrlTemplateRasterOverlaySource, AZ::SystemAllocator, 0);

        static void Reflect(AZ::ReflectContext* context);

        UrlTemplateRasterOverlaySource();

        AZStd::string m_url;
        AZStd::unordered_map<AZStd::string, AZStd::string> m_headers;
        AZStd::string m_fileExtension;
        uint32_t m_minimumLevel;
        uint32_t m_maximumLevel;
    };

    class UrlTemplateRasterOverlayComponent : public RasterOverlayComponent
    {
    public:
        AZ_COMPONENT(UrlTemplateRasterOverlayComponent, "{0F13DA9A-A46A-431C-B7A4-69440E70DFF4}", RasterOverlayComponent)

        static void Reflect(AZ::ReflectContext* context);

        void LoadRasterOverlay(const UrlTemplateRasterOverlaySource& source);

    private:
        std::unique_ptr<Cesium3DTilesSelection::RasterOverlay> LoadRasterOverlayImpl() override;

        UrlTemplateRasterOverlaySource m_source;
    };
} // namespace Cesium
