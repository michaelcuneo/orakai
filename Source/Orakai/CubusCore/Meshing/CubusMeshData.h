#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

/**
 * Engine-independent output produced by a Cubus mesher.
 *
 * This structure contains everything required to submit one mesh section
 * to a UProceduralMeshComponent.
 */
struct ORAKAI_API FCubusMeshData
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FLinearColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;

    void Reset()
    {
        Vertices.Reset();
        Triangles.Reset();
        Normals.Reset();
        UV0.Reset();
        VertexColors.Reset();
        Tangents.Reset();
    }

    void Reserve(
        const int32 VertexCount,
        const int32 TriangleIndexCount
    )
    {
        Vertices.Reserve(VertexCount);
        Triangles.Reserve(TriangleIndexCount);
        Normals.Reserve(VertexCount);
        UV0.Reserve(VertexCount);
        VertexColors.Reserve(VertexCount);
        Tangents.Reserve(VertexCount);
    }

    bool IsEmpty() const
    {
        return Vertices.IsEmpty() || Triangles.IsEmpty();
    }

    int32 GetVertexCount() const
    {
        return Vertices.Num();
    }

    int32 GetTriangleCount() const
    {
        return Triangles.Num() / 3;
    }

    bool IsValid() const
    {
        const int32 VertexCount = Vertices.Num();

        return
            VertexCount > 0 &&
            Triangles.Num() >= 3 &&
            Normals.Num() == VertexCount &&
            UV0.Num() == VertexCount &&
            VertexColors.Num() == VertexCount &&
            Tangents.Num() == VertexCount;
    }
};