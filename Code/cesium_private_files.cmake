
# Private implementation files for Cesium Gem
# These files contain the implementation details and should not be used outside this gem

set(FILES
    # Platform Info
    Source/Cesium/PlatformInfo/PlatformInfo.h
    Source/Cesium/PlatformInfo/PlatformInfo.cpp

    # Math Implementation
    Source/Cesium/Math/Cartographic.cpp
    Source/Cesium/Math/OrientedBoundingBox.cpp
    Source/Cesium/Math/BoundingRegion.cpp
    Source/Cesium/Math/BoundingSphere.cpp
    Source/Cesium/Math/TilesetBoundingVolume.cpp
    Source/Cesium/Math/BoundingVolumeConverters.h
    Source/Cesium/Math/BoundingVolumeConverters.cpp
    Source/Cesium/Math/MathReflect.cpp
    Source/Cesium/Math/GeospatialHelper.cpp
    Source/Cesium/Math/MathHelper.h
    Source/Cesium/Math/MathHelper.cpp
    Source/Cesium/Math/Interpolator.h
    Source/Cesium/Math/Interpolator.cpp
    Source/Cesium/Math/GeoReferenceInterpolator.h
    Source/Cesium/Math/GeoReferenceInterpolator.cpp
    Source/Cesium/Math/LinearInterpolator.h
    Source/Cesium/Math/LinearInterpolator.cpp

    # Systems
    Source/Cesium/Systems/GenericIOManager.h
    Source/Cesium/Systems/GenericIOManager.cpp
    Source/Cesium/Systems/HttpManager.h
    Source/Cesium/Systems/HttpManager.cpp
    Source/Cesium/Systems/LocalFileManager.h
    Source/Cesium/Systems/LocalFileManager.cpp
    Source/Cesium/Systems/LoggerSink.h
    Source/Cesium/Systems/LoggerSink.cpp
    Source/Cesium/Systems/TaskProcessor.h
    Source/Cesium/Systems/TaskProcessor.cpp
    Source/Cesium/Systems/HttpAssetAccessor.h
    Source/Cesium/Systems/HttpAssetAccessor.cpp
    Source/Cesium/Systems/GenericAssetAccessor.h
    Source/Cesium/Systems/GenericAssetAccessor.cpp
    Source/Cesium/Systems/CriticalAssetManager.h
    Source/Cesium/Systems/CriticalAssetManager.cpp
    Source/Cesium/Systems/CesiumSystem.h
    Source/Cesium/Systems/CesiumSystem.cpp

    # Gltf
    Source/Cesium/Gltf/BitangentAndTangentGenerator.h
    Source/Cesium/Gltf/BitangentAndTangentGenerator.cpp
    Source/Cesium/Gltf/GltfLoadContext.h
    Source/Cesium/Gltf/GltfLoadContext.cpp
    Source/Cesium/Gltf/GltfModel.h
    Source/Cesium/Gltf/GltfModel.cpp
    Source/Cesium/Gltf/GltfPrimitiveBuilder.h
    Source/Cesium/Gltf/GltfPrimitiveBuilder.cpp
    Source/Cesium/Gltf/GltfMaterialBuilder.h
    Source/Cesium/Gltf/GltfMaterialBuilder.cpp
    Source/Cesium/Gltf/GltfPBRMaterialBuilder.h
    Source/Cesium/Gltf/GltfPBRMaterialBuilder.cpp
    Source/Cesium/Gltf/GltfModelBuilder.h
    Source/Cesium/Gltf/GltfModelBuilder.cpp

    # Tileset Utility
    Source/Cesium/TilesetUtility/TilesetCameraConfigurations.h
    Source/Cesium/TilesetUtility/TilesetCameraConfigurations.cpp
    Source/Cesium/TilesetUtility/GltfRasterMaterialBuilder.h
    Source/Cesium/TilesetUtility/GltfRasterMaterialBuilder.cpp
    Source/Cesium/TilesetUtility/RenderResourcesPreparer.h
    Source/Cesium/TilesetUtility/RenderResourcesPreparer.cpp

    # EBus Implementation
    Source/Cesium/EBus/CesiumSystemComponentBus.h
    Source/Cesium/EBus/CesiumSystemComponentBus.cpp
    Source/Cesium/EBus/DynamicUiImageComponentBus.h
    Source/Cesium/EBus/DynamicUiImageComponentBus.cpp
    Source/Cesium/EBus/RasterOverlayContainerBus.h
    Source/Cesium/EBus/RasterOverlayContainerBus.cpp
    Source/Cesium/EBus/OriginShiftComponentBus.cpp
    Source/Cesium/EBus/OriginShiftAnchorComponentBus.cpp
    Source/Cesium/EBus/GeoReferenceCameraFlyControllerBus.cpp
    Source/Cesium/EBus/GltfModelComponentBus.cpp
    Source/Cesium/EBus/TilesetComponentBus.cpp

    # Components Implementation
    Source/Cesium/Components/CesiumSystemComponent.h
    Source/Cesium/Components/CesiumSystemComponent.cpp
    Source/Cesium/Components/DynamicUiImageComponent.h
    Source/Cesium/Components/DynamicUiImageComponent.cpp
    Source/Cesium/Components/HtmlUiComponentHelper.h
    Source/Cesium/Components/HtmlUiComponentHelper.cpp
    Source/Cesium/Components/RasterOverlayComponent.cpp
    Source/Cesium/Components/CesiumIonRasterOverlayComponent.cpp
    Source/Cesium/Components/BingRasterOverlayComponent.cpp
    Source/Cesium/Components/TMSRasterOverlayComponent.cpp
    Source/Cesium/Components/OriginShiftComponent.cpp
    Source/Cesium/Components/GeoreferenceAnchorComponent.cpp
    Source/Cesium/Components/GeoReferenceCameraFlyController.cpp
    Source/Cesium/Components/TilesetCreditComponent.cpp
    Source/Cesium/Components/GltfModelComponent.cpp
    Source/Cesium/Components/TilesetComponent.cpp
)
