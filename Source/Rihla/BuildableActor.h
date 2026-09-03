#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BuildableActor.generated.h"

UCLASS()
class RIHLA_API ABuildableActor : public AActor
{
    GENERATED_BODY()

public:
    ABuildableActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UFUNCTION(BlueprintCallable, Category = "Build")
    TArray<FName> GetSnapSocketNames() const;

    // Inventory-originated build pieces keep their source instance identity on the actor.
    // Inventory remains authoritative for consuming/removing that instance.
    UFUNCTION(BlueprintCallable, Category = "Build|Inventory")
    void SetSourceItemInstanceID(const FGuid& InItemInstanceID);

    UFUNCTION(BlueprintPure, Category = "Build|Inventory")
    FGuid GetSourceItemInstanceID() const;

    UFUNCTION(BlueprintPure, Category = "Build|Inventory")
    bool HasSourceItemInstanceID() const;

private:
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Build|Inventory", meta = (AllowPrivateAccess = "true"))
    FGuid SourceItemInstanceID;
};
