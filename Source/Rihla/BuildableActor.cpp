#include "BuildableActor.h"

#include "Components/StaticMeshComponent.h"

ABuildableActor::ABuildableActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;
    Mesh->SetSimulatePhysics(true);
    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));

    Tags.AddUnique(TEXT("Buildable"));
    SourceItemInstanceID.Invalidate();
}

TArray<FName> ABuildableActor::GetSnapSocketNames() const
{
    TArray<FName> Result;

    if (!Mesh)
    {
        return Result;
    }

    for (const FName& SocketName : Mesh->GetAllSocketNames())
    {
        if (SocketName.ToString().StartsWith(TEXT("Snap_")))
        {
            Result.Add(SocketName);
        }
    }

    return Result;
}

void ABuildableActor::SetSourceItemInstanceID(const FGuid& InItemInstanceID)
{
    SourceItemInstanceID = InItemInstanceID;
}

FGuid ABuildableActor::GetSourceItemInstanceID() const
{
    return SourceItemInstanceID;
}

bool ABuildableActor::HasSourceItemInstanceID() const
{
    return SourceItemInstanceID.IsValid();
}
