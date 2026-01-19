
# API files for Cesium Gem
# These are the public header files that other gems can depend on

set(FILES
    # Math API
    Include/Cesium/Math/Cartographic.h
    Include/Cesium/Math/OrientedBoundingBox.h
    Include/Cesium/Math/BoundingRegion.h
    Include/Cesium/Math/BoundingSphere.h
    Include/Cesium/Math/TilesetBoundingVolume.h
    Include/Cesium/Math/MathReflect.h
    Include/Cesium/Math/GeospatialHelper.h

    # EBus API
    Include/Cesium/EBus/OriginShiftComponentBus.h
    Include/Cesium/EBus/OriginShiftAnchorComponentBus.h
    Include/Cesium/EBus/GeoReferenceCameraFlyControllerBus.h
    Include/Cesium/EBus/GltfModelComponentBus.h
    Include/Cesium/EBus/TilesetComponentBus.h

    # Components API
    Include/Cesium/Components/RasterOverlayComponent.h
    Include/Cesium/Components/CesiumIonRasterOverlayComponent.h
    Include/Cesium/Components/BingRasterOverlayComponent.h
    Include/Cesium/Components/TMSRasterOverlayComponent.h
    Include/Cesium/Components/OriginShiftComponent.h
    Include/Cesium/Components/GeoreferenceAnchorComponent.h
    Include/Cesium/Components/GeoReferenceCameraFlyController.h
    Include/Cesium/Components/TilesetCreditComponent.h
    Include/Cesium/Components/GltfModelComponent.h
    Include/Cesium/Components/TilesetComponent.h
)
