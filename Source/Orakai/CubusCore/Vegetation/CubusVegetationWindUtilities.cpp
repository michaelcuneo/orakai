#include "CubusCore/Vegetation/CubusVegetationWindUtilities.h"

#include "Animation/TransformProviderData.h"
#include "Components/ActorComponent.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

    bool FCubusVegetationWindUtilities::TryReadFloatProperty(
        const UObject* Source,
        const FName PropertyName,
        float& OutValue
    )
    {
        if (!IsValid(Source))
        {
            return false;
        }

        const FProperty* Property =
            Source->GetClass()->FindPropertyByName(PropertyName);

        if (const FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
        {
            OutValue = FloatProperty->GetPropertyValue_InContainer(Source);
            return true;
        }

        if (const FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
        {
            OutValue = static_cast<float>(
                DoubleProperty->GetPropertyValue_InContainer(Source)
            );
            return true;
        }

        return false;
    }

    bool FCubusVegetationWindUtilities::TryWriteFloatProperty(
        UObject* Target,
        const FName PropertyName,
        const float Value
    )
    {
        if (!IsValid(Target))
        {
            return false;
        }

        FProperty* Property =
            Target->GetClass()->FindPropertyByName(PropertyName);

        if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
        {
            FloatProperty->SetPropertyValue_InContainer(Target, Value);
            return true;
        }

        if (FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
        {
            DoubleProperty->SetPropertyValue_InContainer(
                Target,
                static_cast<double>(Value)
            );
            return true;
        }

        return false;
    }

    bool FCubusVegetationWindUtilities::TryReadVectorLikeProperty(
        const UObject* Source,
        const FName PropertyName,
        FVector& OutValue
    )
    {
        if (!IsValid(Source))
        {
            return false;
        }

        const FProperty* Property =
            Source->GetClass()->FindPropertyByName(PropertyName);

        const FStructProperty* StructProperty =
            CastField<FStructProperty>(Property);

        if (StructProperty == nullptr)
        {
            return false;
        }

        if (StructProperty->Struct == TBaseStructure<FVector>::Get())
        {
            const FVector* Value =
                StructProperty->ContainerPtrToValuePtr<FVector>(Source);

            if (Value == nullptr)
            {
                return false;
            }

            OutValue = *Value;
            return true;
        }

        if (StructProperty->Struct == TBaseStructure<FRotator>::Get())
        {
            const FRotator* Value =
                StructProperty->ContainerPtrToValuePtr<FRotator>(Source);

            if (Value == nullptr)
            {
                return false;
            }

            OutValue = Value->Vector();
            return true;
        }

        return false;
    }

    bool FCubusVegetationWindUtilities::TryWriteBoolProperty(
        UObject* Target,
        const FName PropertyName,
        const bool Value
    )
    {
        if (!IsValid(Target))
        {
            return false;
        }

        FProperty* Property =
            Target->GetClass()->FindPropertyByName(PropertyName);

        if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
        {
            BoolProperty->SetPropertyValue_InContainer(Target, Value);
            return true;
        }

        return false;
    }

    bool FCubusVegetationWindUtilities::TryWriteVectorLikeProperty(
        UObject* Target,
        const FName PropertyName,
        const FVector& Value
    )
    {
        if (!IsValid(Target))
        {
            return false;
        }

        FProperty* Property =
            Target->GetClass()->FindPropertyByName(PropertyName);

        FStructProperty* StructProperty =
            CastField<FStructProperty>(Property);

        if (StructProperty == nullptr)
        {
            return false;
        }

        if (StructProperty->Struct == TBaseStructure<FVector>::Get())
        {
            FVector* Dest =
                StructProperty->ContainerPtrToValuePtr<FVector>(Target);

            if (Dest == nullptr)
            {
                return false;
            }

            *Dest = Value;
            return true;
        }

        if (StructProperty->Struct == TBaseStructure<FRotator>::Get())
        {
            FRotator* Dest =
                StructProperty->ContainerPtrToValuePtr<FRotator>(Target);

            if (Dest == nullptr)
            {
                return false;
            }

            *Dest = Value.Rotation();
            return true;
        }

        return false;
    }

    AActor* FCubusVegetationWindUtilities::ResolveUltraDynamicWeatherActor(UWorld* World)
    {
        if (!IsValid(World))
        {
            return nullptr;
        }

        for (TActorIterator<AActor> Iterator(World); Iterator; ++Iterator)
        {
            AActor* Candidate = *Iterator;

            if (!IsValid(Candidate))
            {
                continue;
            }

            const FString Name = Candidate->GetName();
            const FString ClassName = Candidate->GetClass()->GetName();

            if (
                Name.Contains(TEXT("Ultra_Dynamic_Weather")) ||
                Name.Contains(TEXT("UltraDynamicWeather")) ||
                ClassName.Contains(TEXT("Ultra_Dynamic_Weather")) ||
                ClassName.Contains(TEXT("UltraDynamicWeather")) ||
                Candidate->ActorHasTag(TEXT("UltraDynamicWeather"))
            )
            {
                return Candidate;
            }
        }

        return nullptr;
    }

    void FCubusVegetationWindUtilities::AssignLikelyWindProviderActor(
        UObject* Target,
        AActor* WindProviderActor
    )
    {
        if (!IsValid(Target) || !IsValid(WindProviderActor))
        {
            return;
        }

        for (TFieldIterator<FProperty> FieldIt(Target->GetClass()); FieldIt; ++FieldIt)
        {
            FProperty* Property = *FieldIt;

            FObjectPropertyBase* ObjectProperty =
                CastField<FObjectPropertyBase>(Property);

            if (ObjectProperty == nullptr)
            {
                continue;
            }

            if (!ObjectProperty->PropertyClass->IsChildOf(AActor::StaticClass()))
            {
                continue;
            }

            const FString PropertyName = Property->GetName();

            const bool bLooksLikeWindProviderField =
                PropertyName.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) &&
                (
                    PropertyName.Contains(TEXT("Provider"), ESearchCase::IgnoreCase) ||
                    PropertyName.Contains(TEXT("Source"), ESearchCase::IgnoreCase) ||
                    PropertyName.Contains(TEXT("Actor"), ESearchCase::IgnoreCase) ||
                    PropertyName.Contains(TEXT("Controller"), ESearchCase::IgnoreCase)
                );

            if (!bLooksLikeWindProviderField)
            {
                continue;
            }

            ObjectProperty->SetObjectPropertyValue_InContainer(
                Target,
                WindProviderActor
            );
        }
    }

        UObject* FCubusVegetationWindUtilities::ResolveTransformProviderDataFromObject(UObject* Candidate)
    {
        if (!IsValid(Candidate))
        {
            return nullptr;
        }

        if (UInstancedSkinnedMeshComponent* InstancedSkinned =
                Cast<UInstancedSkinnedMeshComponent>(Candidate))
        {
            return InstancedSkinned->GetTransformProvider();
        }

        for (TFieldIterator<FObjectPropertyBase> FieldIt(Candidate->GetClass()); FieldIt; ++FieldIt)
        {
            FObjectPropertyBase* Property = *FieldIt;

            if (Property == nullptr)
            {
                continue;
            }

            const FString ClassName =
                IsValid(Property->PropertyClass)
                    ? Property->PropertyClass->GetName()
                    : FString();

            const FString PropertyName = Property->GetName();

            const bool bLooksLikeTransformProvider =
                ClassName.Contains(TEXT("TransformProvider"), ESearchCase::IgnoreCase) ||
                PropertyName.Contains(TEXT("TransformProvider"), ESearchCase::IgnoreCase);

            if (!bLooksLikeTransformProvider)
            {
                continue;
            }

            UObject* Value =
                Property->GetObjectPropertyValue_InContainer(Candidate);

            if (IsValid(Value))
            {
                return Value;
            }
        }

        return nullptr;
    }

    UObject* FCubusVegetationWindUtilities::ResolveWindTransformProviderFromActor(AActor* CandidateActor)
    {
        if (!IsValid(CandidateActor))
        {
            return nullptr;
        }

        if (UObject* DirectProvider = FCubusVegetationWindUtilities::ResolveTransformProviderDataFromObject(CandidateActor))
        {
            return DirectProvider;
        }

        TInlineComponentArray<UActorComponent*> Components(CandidateActor);
        for (UActorComponent* Component : Components)
        {
            if (UObject* Provider = FCubusVegetationWindUtilities::ResolveTransformProviderDataFromObject(Component))
            {
                return Provider;
            }
        }

        return nullptr;
    }

    AActor* FCubusVegetationWindUtilities::ResolveGlobalFoliageActor(UWorld* World)
    {
        if (!IsValid(World))
        {
            return nullptr;
        }

        AActor* BestCandidate = nullptr;
        int32 BestScore = MIN_int32;

        for (TActorIterator<AActor> Iterator(World); Iterator; ++Iterator)
        {
            AActor* Candidate = *Iterator;

            if (!IsValid(Candidate))
            {
                continue;
            }

            const FString Name = Candidate->GetName();
            const FString ClassName = Candidate->GetClass()->GetName();

            int32 Score = 0;

            if (
                Name.Contains(TEXT("GlobalFoliage"), ESearchCase::IgnoreCase) ||
                Name.Contains(TEXT("Global_Foliage"), ESearchCase::IgnoreCase)
            )
            {
                Score += 120;
            }

            if (
                ClassName.Contains(TEXT("GlobalFoliage"), ESearchCase::IgnoreCase) ||
                ClassName.Contains(TEXT("Global_Foliage"), ESearchCase::IgnoreCase)
            )
            {
                Score += 120;
            }

            if (Name.Contains(TEXT("Megascans"), ESearchCase::IgnoreCase))
            {
                Score += 50;
            }

            if (ClassName.Contains(TEXT("Megascans"), ESearchCase::IgnoreCase))
            {
                Score += 50;
            }

            if (Candidate->ActorHasTag(TEXT("GlobalFoliage")))
            {
                Score += 80;
            }

            if (Score > BestScore)
            {
                BestScore = Score;
                BestCandidate = Candidate;
            }
        }

        return BestScore > 0 ? BestCandidate : nullptr;
    }

    int32 FCubusVegetationWindUtilities::ApplyWindToObject(
        UObject* Target,
        const FVector& WindDirection,
        const float WindIntensity
    )
    {
        if (!IsValid(Target))
        {
            return 0;
        }

        int32 UpdatedPropertyCount = 0;

        static const FName DirectionPropertyCandidates[] =
        {
            TEXT("WindDirection"),
            TEXT("Wind Direction"),
            TEXT("Wind_Direction"),
            TEXT("GlobalWindDirection"),
            TEXT("WindDir"),
            TEXT("FoliageWindDirection")
        };

        static const FName ScalarPropertyCandidates[] =
        {
            TEXT("WindIntensity"),
            TEXT("Wind Intensity"),
            TEXT("Wind_Intensity"),
            TEXT("GlobalWindIntensity"),
            TEXT("WindSpeed"),
            TEXT("Wind Speed"),
            TEXT("GlobalWindSpeed"),
            TEXT("WindStrength"),
            TEXT("FoliageWindSpeed"),
            TEXT("FoliageWindIntensity")
        };

        const float WindDirectionYaw =
            FRotator::NormalizeAxis(WindDirection.Rotation().Yaw);

        // Explicit first-pass sync for deterministic UDS->GlobalFoliage speed matching.
        for (const FName PropertyName : DirectionPropertyCandidates)
        {
            if (
                FCubusVegetationWindUtilities::TryWriteVectorLikeProperty(Target, PropertyName, WindDirection) ||
                FCubusVegetationWindUtilities::TryWriteFloatProperty(Target, PropertyName, WindDirectionYaw)
            )
            {
                ++UpdatedPropertyCount;
            }
        }

        for (const FName PropertyName : ScalarPropertyCandidates)
        {
            if (FCubusVegetationWindUtilities::TryWriteFloatProperty(Target, PropertyName, WindIntensity))
            {
                ++UpdatedPropertyCount;
            }
        }

        for (TFieldIterator<FProperty> FieldIt(Target->GetClass()); FieldIt; ++FieldIt)
        {
            FProperty* Property = *FieldIt;

            if (Property == nullptr)
            {
                continue;
            }

            const FString PropertyName = Property->GetName();
            const FName PropertyFName = Property->GetFName();

            const bool bLooksLikeWindDirection =
                PropertyName.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) &&
                (
                    PropertyName.Contains(TEXT("Direction"), ESearchCase::IgnoreCase) ||
                    PropertyName.Contains(TEXT("Dir"), ESearchCase::IgnoreCase)
                );

            if (bLooksLikeWindDirection)
            {
                if (
                    FCubusVegetationWindUtilities::TryWriteVectorLikeProperty(Target, PropertyFName, WindDirection) ||
                    FCubusVegetationWindUtilities::TryWriteFloatProperty(Target, PropertyFName, WindDirectionYaw)
                )
                {
                    ++UpdatedPropertyCount;
                    continue;
                }
            }

            const bool bLooksLikeWindScalar =
                PropertyName.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) &&
                (
                    PropertyName.Contains(TEXT("Intensity"), ESearchCase::IgnoreCase) ||
                    PropertyName.Contains(TEXT("Speed"), ESearchCase::IgnoreCase) ||
                    PropertyName.Contains(TEXT("Strength"), ESearchCase::IgnoreCase) ||
                    PropertyName.Contains(TEXT("Gust"), ESearchCase::IgnoreCase)
                );

            if (bLooksLikeWindScalar)
            {
                if (FCubusVegetationWindUtilities::TryWriteFloatProperty(Target, PropertyFName, WindIntensity))
                {
                    ++UpdatedPropertyCount;
                }
            }
        }

        return UpdatedPropertyCount;
    }

    int32 FCubusVegetationWindUtilities::InvokeLikelyWindRefreshFunctions(UObject* Target)
    {
        if (!IsValid(Target))
        {
            return 0;
        }

        int32 InvokedFunctionCount = 0;

        for (TFieldIterator<UFunction> FunctionIt(Target->GetClass()); FunctionIt; ++FunctionIt)
        {
            UFunction* Function = *FunctionIt;

            if (Function == nullptr || Function->HasAnyFunctionFlags(FUNC_Const | FUNC_BlueprintPure))
            {
                continue;
            }

            const FString FunctionName = Function->GetName();

            const bool bLooksLikeWindRefresh =
                FunctionName.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) &&
                (
                    FunctionName.Contains(TEXT("Update"), ESearchCase::IgnoreCase) ||
                    FunctionName.Contains(TEXT("Refresh"), ESearchCase::IgnoreCase) ||
                    FunctionName.Contains(TEXT("Apply"), ESearchCase::IgnoreCase) ||
                    FunctionName.Contains(TEXT("Sync"), ESearchCase::IgnoreCase)
                );

            if (!bLooksLikeWindRefresh)
            {
                continue;
            }

            bool bHasInputParameters = false;

            for (TFieldIterator<FProperty> PropertyIt(Function); PropertyIt; ++PropertyIt)
            {
                const FProperty* Property = *PropertyIt;

                if (
                    Property != nullptr &&
                    Property->HasAnyPropertyFlags(CPF_Parm) &&
                    !Property->HasAnyPropertyFlags(CPF_ReturnParm)
                )
                {
                    bHasInputParameters = true;
                    break;
                }
            }

            if (bHasInputParameters)
            {
                continue;
            }

            Target->ProcessEvent(Function, nullptr);
            ++InvokedFunctionCount;
        }

        return InvokedFunctionCount;
    }
