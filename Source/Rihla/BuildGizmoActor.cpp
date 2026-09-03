#include "BuildGizmoActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ABuildGizmoActor::ABuildGizmoActor()
{
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    XArrow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("XArrow"));
    XArrow->SetupAttachment(RootComponent);
    XArrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    YArrow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("YArrow"));
    YArrow->SetupAttachment(RootComponent);
    YArrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ZArrow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZArrow"));
    ZArrow->SetupAttachment(RootComponent);
    ZArrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
