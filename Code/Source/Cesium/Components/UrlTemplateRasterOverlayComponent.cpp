#include <Cesium/Components/UrlTemplateRasterOverlayComponent.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <CesiumRasterOverlays/RasterOverlay.h>
#include <CesiumRasterOverlays/UrlTemplateRasterOverlay.h>

namespace Cesium
{
    void UrlTemplateRasterOverlaySource::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<UrlTemplateRasterOverlaySource>()
                ->Version(0)
                ->Field("Url", &UrlTemplateRasterOverlaySource::m_url)
                ->Field("Headers", &UrlTemplateRasterOverlaySource::m_headers)
                ->Field("FileExtension", &UrlTemplateRasterOverlaySource::m_fileExtension)
                ->Field("MinimumLevel", &UrlTemplateRasterOverlaySource::m_minimumLevel)
                ->Field("MaximumLevel", &UrlTemplateRasterOverlaySource::m_maximumLevel);
        }

        if (auto behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<UrlTemplateRasterOverlaySource>("UrlTemplateRasterOverlaySource")
                ->Property("Url", BehaviorValueProperty(&UrlTemplateRasterOverlaySource::m_url))
                ->Property("Headers", BehaviorValueProperty(&UrlTemplateRasterOverlaySource::m_headers))
                ->Property("FileExtension", BehaviorValueProperty(&UrlTemplateRasterOverlaySource::m_fileExtension))
                ->Property("MinimumLevel", BehaviorValueProperty(&UrlTemplateRasterOverlaySource::m_minimumLevel))
                ->Property("MaximumLevel", BehaviorValueProperty(&UrlTemplateRasterOverlaySource::m_maximumLevel));
        }
    }

    UrlTemplateRasterOverlaySource::UrlTemplateRasterOverlaySource()
        : m_fileExtension{ "png" }
        , m_minimumLevel{ 0 }
        , m_maximumLevel{ 25 }
    {
    }

    void UrlTemplateRasterOverlayComponent::Reflect(AZ::ReflectContext* context)
    {
        UrlTemplateRasterOverlaySource::Reflect(context);

        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<UrlTemplateRasterOverlayComponent, AZ::Component, RasterOverlayComponent>()->Version(0)->Field(
                "source", &UrlTemplateRasterOverlayComponent::m_source);
        }

        if (auto behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<UrlTemplateRasterOverlayComponent>("UrltemplateRasterOverlayComponent")
                ->Attribute(AZ::Script::Attributes::Category, "Cesium/RasterOverlays")
                ->Method(
                    "SetConfiguration",
                    [](UrlTemplateRasterOverlayComponent& component, const RasterOverlayConfiguration& config)
                    {
                        component.SetConfiguration(config);
                    })
                ->Method(
                    "GetConfiguration",
                    [](const UrlTemplateRasterOverlayComponent& component)
                    {
                        return component.GetConfiguration();
                    })
                ->Method("LoadRasterOverlay", &UrlTemplateRasterOverlayComponent::LoadRasterOverlay);
        }
    }

    void UrlTemplateRasterOverlayComponent::LoadRasterOverlay(const UrlTemplateRasterOverlaySource& source)
    {
        m_source = source;
        RasterOverlayComponent::LoadRasterOverlay();
    }

    std::unique_ptr<CesiumRasterOverlays::RasterOverlay> UrlTemplateRasterOverlayComponent::LoadRasterOverlayImpl()
    {
        // setup TMS option
        CesiumRasterOverlays::UrlTemplateRasterOverlayOptions options{};
        if (m_source.m_maximumLevel > m_source.m_minimumLevel)
        {
            options.minimumLevel = m_source.m_minimumLevel;
            options.maximumLevel = m_source.m_maximumLevel;
        }
        //options.fileExtension = m_source.m_fileExtension.c_str();

        // setup TMS headers
        std::vector<CesiumAsync::IAssetAccessor::THeader> headers;
        for (const auto& header : m_source.m_headers)
        {
            headers.emplace_back(header.first.c_str(), header.second.c_str());
        }

        return std::make_unique<CesiumRasterOverlays::UrlTemplateRasterOverlay>(
            "UrlTemplateRasterOverlay", m_source.m_url.c_str(), headers, options);
    }
} // namespace Cesium
