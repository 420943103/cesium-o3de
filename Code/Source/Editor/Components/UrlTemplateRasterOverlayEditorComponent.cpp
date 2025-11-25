#include "Editor/Components/UrlTemplateRasterOverlayEditorComponent.h"
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>

namespace Cesium
{
    UrlTemplateRasterOverlayEditorComponent::UrlTemplateRasterOverlayEditorComponent()
    {
    }

    void UrlTemplateRasterOverlayEditorComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<UrlTemplateRasterOverlayEditorComponent, AZ::Component>()
                ->Version(0)
                ->Field("Configuration", &UrlTemplateRasterOverlayEditorComponent::m_configuration)
                ->Field("Source", &UrlTemplateRasterOverlayEditorComponent::m_source);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext
                    ->Class<UrlTemplateRasterOverlayEditorComponent>(
                        "UrlTemplate Service Raster Overlay", "The raster component is used to drap imagery on 3D Tiles")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Cesium")
                    ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/Cesium_logo_only.svg")
                    ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Cesium_logo_only.svg")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game", 0x232b318c))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &UrlTemplateRasterOverlayEditorComponent::m_configuration, "Configuration", "")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &UrlTemplateRasterOverlayEditorComponent::OnConfigurationChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &UrlTemplateRasterOverlayEditorComponent::m_source, "Source", "")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &UrlTemplateRasterOverlayEditorComponent::OnSourceChanged);

                editContext->Class<RasterOverlayConfiguration>("Configuration", "")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Configuration")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &RasterOverlayConfiguration::m_maximumCacheBytes, "Maximum Cache Size", "")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default, &RasterOverlayConfiguration::m_maximumSimultaneousTileLoads,
                        "Maximum Simultaneous TileLoads", "");

                editContext->Class<UrlTemplateRasterOverlaySource>("Source", "")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Source")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &UrlTemplateRasterOverlaySource::m_url, "Url", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &UrlTemplateRasterOverlaySource::m_fileExtension, "File Extension", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &UrlTemplateRasterOverlaySource::m_headers, "Request Headers", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &UrlTemplateRasterOverlaySource::m_minimumLevel, "Minimum Level", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &UrlTemplateRasterOverlaySource::m_maximumLevel, "Maximum Level", "");
            }
        }
    }

    void UrlTemplateRasterOverlayEditorComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("UrlTemplateRasterOverlayEditorSerivce"));
    }

    void UrlTemplateRasterOverlayEditorComponent::GetIncompatibleServices(
        [[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
    }

    void UrlTemplateRasterOverlayEditorComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("3DTilesEditorService"));
    }

    void UrlTemplateRasterOverlayEditorComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        dependent.push_back(AZ_CRC_CE("3DTilesEditorService"));
    }

    void UrlTemplateRasterOverlayEditorComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        auto rasterOverlayComponent = gameEntity->CreateComponent<UrlTemplateRasterOverlayComponent>();
        rasterOverlayComponent->SetEntity(gameEntity);
        rasterOverlayComponent->Init();
        rasterOverlayComponent->Activate();
        rasterOverlayComponent->SetConfiguration(m_configuration);
        rasterOverlayComponent->LoadRasterOverlay(m_source);
    }

    void UrlTemplateRasterOverlayEditorComponent::Init()
    {
        AzToolsFramework::Components::EditorComponentBase::Init();
        if (!m_rasterOverlayComponent)
        {
            m_rasterOverlayComponent = AZStd::make_unique<UrlTemplateRasterOverlayComponent>();
        }
    }

    void UrlTemplateRasterOverlayEditorComponent::Activate()
    {
        m_rasterOverlayComponent->SetEntity(GetEntity());
        m_rasterOverlayComponent->Init();
        m_rasterOverlayComponent->Activate();
        m_rasterOverlayComponent->SetConfiguration(m_configuration);
        m_rasterOverlayComponent->LoadRasterOverlay(m_source);
        m_rasterOverlayComponent->Deactivate();
    }

    void UrlTemplateRasterOverlayEditorComponent::Deactivate()
    {
        m_rasterOverlayComponent->Deactivate();
        m_rasterOverlayComponent->SetEntity(nullptr);
    }

    AZ::u32 UrlTemplateRasterOverlayEditorComponent::OnSourceChanged()
    {
        if (!m_rasterOverlayComponent)
        {
            return AZ::Edit::PropertyRefreshLevels::None;
        }

        m_rasterOverlayComponent->LoadRasterOverlay(m_source);
        return AZ::Edit::PropertyRefreshLevels::None;
    }

    AZ::u32 UrlTemplateRasterOverlayEditorComponent::OnConfigurationChanged()
    {
        if (!m_rasterOverlayComponent)
        {
            return AZ::Edit::PropertyRefreshLevels::None;
        }

        m_rasterOverlayComponent->SetConfiguration(m_configuration);
        return AZ::Edit::PropertyRefreshLevels::None;
    }
} // namespace Cesium
