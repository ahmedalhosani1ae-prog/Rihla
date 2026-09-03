#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BuildGizmoActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class RIHLA_API ABuildGizmoActor : public AActor
{
    GENERATED_BODY()

public:
    ABuildGizmoActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gizmo")
    TObjectPtr<UStaticMeshComponent> XArrow;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gizmo")
    TObjectPtr<UStaticMeshComponent> YArrow;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gizmo")
    TObjectPtr<UStaticMeshComponent> ZArrow;
};
