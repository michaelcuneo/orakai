#include "CubusCore/Data/CubusMaterialRegistry.h"

#include "CubusCore/Editor/CubusMaterialBuilderLibrary.h"

#include "Materials/Material.h"

void UCubusMaterialRegistry::BuildDensityMaterial()
{
#if WITH_EDITOR
    UMaterial* BuiltMaterial =
        UCubusMaterialBuilderLibrary::BuildCubusDensityPbrMaterial();

    if (!IsValid(BuiltMaterial))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Cubus material registry failed to build M_CubusDensityPBR.")
        );
        return;
    }

    Modify();
    DensityMaterial = BuiltMaterial;
    DensityRuntimeMaterialByKey.Reset();
    MarkPackageDirty();
    PostEditChange();

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Cubus material registry built and assigned M_CubusDensityPBR.")
    );
#else
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("BuildDensityMaterial is only available in the Unreal Editor.")
    );
#endif
}
