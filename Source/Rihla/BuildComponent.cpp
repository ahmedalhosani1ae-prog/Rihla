#include "BuildComponent.h"
#include "BuildableActor.h"
#include "BuildGizmoActor.h"

#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"

#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Engine/CollisionProfile.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "TimerManager.h"

UBuildComponent::UBuildComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	GrabDistance = 500.f;
	MinHoldDistance = 100.f;
	MaxHoldDistance = 600.f;
	SnapPointRadius = 100.f;
	RotationSnapIncrement = 45.f;
	DefaultHoldDistance = 200.f;
	HoldDistanceScrollSpeed = 25.f;

	GrabbedActor = nullptr;
	bSpawnedFromInventory = false;
	bSnapPreviewActive = false;
	BuildState = EBuildState::Idle;

	PhysicsHandle = nullptr;
	GrabSpline = nullptr;
	GrabSplineMesh = nullptr;
	SnapSpline = nullptr;
	SnapSplineMesh = nullptr;
	SpawnedGizmo = nullptr;

	SnapPreviewTargetActor = nullptr;
	SnapPreviewTargetMesh = nullptr;

	SourceItemInstanceID.Invalidate();
}

void UBuildComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner || !GetWorld())
	{
		return;
	}

	// -------------------------------------------------------------------------
	// Physics handle
	// -------------------------------------------------------------------------

	PhysicsHandle = NewObject<UPhysicsHandleComponent>(Owner, TEXT("BuildPhysicsHandle"));
	if (!PhysicsHandle)
	{
		return;
	}

	Owner->AddInstanceComponent(PhysicsHandle);
	PhysicsHandle->RegisterComponent();

	PhysicsHandle->LinearDamping = 200.f;
	PhysicsHandle->LinearStiffness = 3000.f;
	PhysicsHandle->AngularDamping = 200.f;
	PhysicsHandle->AngularStiffness = 3000.f;
	PhysicsHandle->InterpolationSpeed = 50.f;
	PhysicsHandle->bInterpolateTarget = true;

	// -------------------------------------------------------------------------
	// Guide splines
	// -------------------------------------------------------------------------

	CreateGuideSplineComponents();

	// -------------------------------------------------------------------------
	// Input
	// -------------------------------------------------------------------------

	if (Cast<APawn>(Owner) && !BindBuildInput())
	{
		GetWorld()->GetTimerManager().SetTimer(
			InputBindRetryTimer,
			this,
			&UBuildComponent::RetryBindBuildInput,
			0.1f,
			true);
	}

	CurrentHoldDistance = FMath::Clamp(
		DefaultHoldDistance,
		MinHoldDistance,
		MaxHoldDistance);
}

void UBuildComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(InputBindRetryTimer);
	}

	UnbindBuildInput();
	CleanupBuildState(true);

	Super::EndPlay(EndPlayReason);
}

void UBuildComponent::CreateGuideSplineComponents()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetRootComponent())
	{
		return;
	}

	GrabSpline = NewObject<USplineComponent>(Owner, TEXT("BuildGrabSpline"));
	if (GrabSpline)
	{
		Owner->AddInstanceComponent(GrabSpline);
		GrabSpline->SetupAttachment(Owner->GetRootComponent());
		GrabSpline->RegisterComponent();
		GrabSpline->SetMobility(EComponentMobility::Movable);
		GrabSpline->SetVisibility(false);
		GrabSpline->ClearSplinePoints();
	}

	SnapSpline = NewObject<USplineComponent>(Owner, TEXT("BuildSnapSpline"));
	if (SnapSpline)
	{
		Owner->AddInstanceComponent(SnapSpline);
		SnapSpline->SetupAttachment(Owner->GetRootComponent());
		SnapSpline->RegisterComponent();
		SnapSpline->SetMobility(EComponentMobility::Movable);
		SnapSpline->SetVisibility(false);
		SnapSpline->ClearSplinePoints();
	}

	if (SplineVisualMesh)
	{
		GrabSplineMesh = NewObject<USplineMeshComponent>(Owner, TEXT("BuildGrabSplineMesh"));
		if (GrabSplineMesh)
		{
			Owner->AddInstanceComponent(GrabSplineMesh);
			GrabSplineMesh->SetStaticMesh(SplineVisualMesh);
			if (SplineVisualMaterial)
			{
				GrabSplineMesh->SetMaterial(0, SplineVisualMaterial);
			}
			GrabSplineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			GrabSplineMesh->SetMobility(EComponentMobility::Movable);
			GrabSplineMesh->SetupAttachment(Owner->GetRootComponent());
			GrabSplineMesh->RegisterComponent();
			GrabSplineMesh->SetVisibility(false);
		}

		SnapSplineMesh = NewObject<USplineMeshComponent>(Owner, TEXT("BuildSnapSplineMesh"));
		if (SnapSplineMesh)
		{
			Owner->AddInstanceComponent(SnapSplineMesh);
			SnapSplineMesh->SetStaticMesh(SplineVisualMesh);
			if (SplineVisualMaterial)
			{
				SnapSplineMesh->SetMaterial(0, SplineVisualMaterial);
			}
			SnapSplineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SnapSplineMesh->SetMobility(EComponentMobility::Movable);
			SnapSplineMesh->SetupAttachment(Owner->GetRootComponent());
			SnapSplineMesh->RegisterComponent();
			SnapSplineMesh->SetVisibility(false);
		}
	}
}

void UBuildComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!PhysicsHandle)
	{
		return;
	}

	if (!GrabbedActor || !IsValid(GrabbedActor))
	{
		CleanupBuildState(false);
		return;
	}

	CurrentHoldDistance = FMath::Clamp(
		CurrentHoldDistance,
		MinHoldDistance,
		MaxHoldDistance);

	const FVector Eyes = GetEyesLocation();
	const FRotator EyesRot = GetEyesRotation();
	const FVector Forward = EyesRot.Vector();
	const FVector FreeTargetLocation = Eyes + Forward * CurrentHoldDistance;

	// -------------------------------------------------------------------------
	// Find snap candidate
	// -------------------------------------------------------------------------

	AActor* FoundActor = nullptr;
	UStaticMeshComponent* FoundMesh = nullptr;
	FName FoundTargetSocket = NAME_None;
	FName FoundHeldSocket = NAME_None;
	float FoundDist = 0.f;

	const bool bFoundSnap = FindNearestSnapSocket(
		GrabbedActor,
		FoundActor,
		FoundMesh,
		FoundTargetSocket,
		FoundHeldSocket,
		FoundDist);

	if (bFoundSnap && FoundActor && FoundMesh)
	{
		FTransform DesiredSnapTransform;
		UStaticMeshComponent* HeldMesh =
			GrabbedActor->FindComponentByClass<UStaticMeshComponent>();

		if (HeldMesh &&
			CalculateSnapTransform(
				GrabbedActor,
				HeldMesh,
				FoundHeldSocket,
				FoundMesh,
				FoundTargetSocket,
				DesiredSnapTransform))
		{
			bSnapPreviewActive = true;
			SnapPreviewTargetActor = FoundActor;
			SnapPreviewTargetMesh = FoundMesh;
			SnapPreviewSocketName = FoundTargetSocket;
			SnapPreviewHeldSocketName = FoundHeldSocket;
			SnapPreviewDistance = FoundDist;
			SnapPreviewTransform = DesiredSnapTransform;

			const FVector PreviewActorLocation =
				SnapPreviewTransform.GetLocation();

			UpdateSplineSegment(
				SnapSpline,
				SnapSplineMesh,
				GrabbedActor->GetActorLocation(),
				PreviewActorLocation);

			if (SnapSpline)
			{
				SnapSpline->SetVisibility(true);
			}

			if (SnapSplineMesh)
			{
				SnapSplineMesh->SetVisibility(true);
			}
		}
		else
		{
			ClearSnapPreview();
		}
	}
	else
	{
		ClearSnapPreview();
	}

	// -------------------------------------------------------------------------
	// Physics handle target
	// -------------------------------------------------------------------------

	FTransform DesiredActorTransform(
		CurrentSnappedRotation,
		FreeTargetLocation,
		GrabbedActor->GetActorScale3D());

	if (bSnapPreviewActive)
	{
		DesiredActorTransform = SnapPreviewTransform;
	}

	const FVector HandleTargetLocation =
		DesiredActorTransform.TransformPosition(GrabbedLocalAnchor);

	PhysicsHandle->SetTargetLocationAndRotation(
		HandleTargetLocation,
		DesiredActorTransform.Rotator());

	UpdateSplineSegment(
		GrabSpline,
		GrabSplineMesh,
		Eyes,
		GrabbedActor->GetActorLocation());

	if (GrabbedActor->GetActorLocation() != Eyes &&
		GrabSpline)
	{
		GrabSpline->SetVisibility(true);
	}

	if (GrabSplineMesh)
	{
		GrabSplineMesh->SetVisibility(true);
	}

	if (bRotationModeActive)
	{
		UpdateGizmoTransform();
	}
}

void UBuildComponent::TryGrabOrConfirm()
{
	if (IsHoldingObject())
	{
		ConfirmPlacement();
	}
	else
	{
		TryGrab();
	}
}

bool UBuildComponent::TryGrab()
{
	if (!PhysicsHandle || IsHoldingObject())
	{
		return false;
	}

	FHitResult Hit;
	if (!TraceForward(Hit))
	{
		return false;
	}

	AActor* HitActor = Hit.GetActor();
	if (!HitActor || !IsBuildableActor(HitActor))
	{
		return false;
	}

	AActor* Root = GetAttachRoot(HitActor);
	if (!Root || !IsBuildableActor(Root))
	{
		return false;
	}

	UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(
		Root->GetRootComponent());

	if (!Primitive || !Primitive->IsSimulatingPhysics())
	{
		return false;
	}

	const FVector GrabLocation = Hit.Location;
	GrabbedLocalAnchor =
		Root->GetActorTransform().InverseTransformPosition(GrabLocation);

	PhysicsHandle->GrabComponentAtLocationWithRotation(
		Primitive,
		NAME_None,
		GrabLocation,
		Root->GetActorRotation());

	if (PhysicsHandle->GetGrabbedComponent() != Primitive)
	{
		GrabbedLocalAnchor = FVector::ZeroVector;
		return false;
	}

	GrabbedActor = Root;
	bSpawnedFromInventory = false;
	SourceItemInstanceID.Invalidate();
	BuildState = EBuildState::HoldingWorldObject;

	CurrentSnappedRotation =
		SnapRotatorToIncrement(Root->GetActorRotation());

	CurrentHoldDistance = FMath::Clamp(
		FVector::Dist(GetEyesLocation(), GrabLocation),
		MinHoldDistance,
		MaxHoldDistance);

	ClearSnapPreview();

	if (GrabSpline)
	{
		GrabSpline->SetVisibility(true);
	}

	if (GrabSplineMesh)
	{
		GrabSplineMesh->SetVisibility(true);
	}

	return true;
}

bool UBuildComponent::ConfirmPlacement()
{
    if (!IsHoldingObject() || !GrabbedActor || !PhysicsHandle)
    {
        return false;
    }

    AActor* PlacedActor = GrabbedActor;
    const bool bFromInventory = bSpawnedFromInventory;
    const FGuid InstanceID = SourceItemInstanceID;

    if (bSnapPreviewActive)
    {
        if (!SnapPreviewTargetActor || !IsValid(SnapPreviewTargetActor))
        {
            ClearSnapPreview();
            return false;
        }

        // The preview transform is authoritative for the final placement.
        PlacedActor->SetActorTransform(
            SnapPreviewTransform,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);

        // Never report a successful placement unless the attachment itself succeeds.
        if (!AttachBuiltPiece(PlacedActor, SnapPreviewTargetActor))
        {
            return false;
        }
    }
    else
    {
        const FVector Eyes = GetEyesLocation();
        const FVector Forward = GetEyesRotation().Vector();
        const FVector DesiredLocation = Eyes + Forward * CurrentHoldDistance;

        const FTransform DesiredTransform(
            CurrentSnappedRotation,
            DesiredLocation,
            PlacedActor->GetActorScale3D());

        PlacedActor->SetActorTransform(
            DesiredTransform,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
    }

    PhysicsHandle->ReleaseComponent();

    // A confirmed free-standing build piece should remain where it was placed,
    // rather than immediately falling because the physics handle was released.
    if (!bSnapPreviewActive)
    {
        SetBuildActorPhysicsEnabled(PlacedActor, false);
    }

    // Inventory owns consumption. This event is emitted exactly once for a
    // successful confirmation and before runtime source state is cleared.
    OnBuildConfirmed.Broadcast(PlacedActor, bFromInventory, InstanceID);

    ClearGrabSpline();
    ClearSnapPreview();
    SetRotationModeActive(false);

    GrabbedActor = nullptr;
    bSpawnedFromInventory = false;
    SourceItemInstanceID.Invalidate();
    GrabbedLocalAnchor = FVector::ZeroVector;
    BuildState = EBuildState::Idle;

    return true;
}

bool UBuildComponent::SpawnAndGrabBuildable(
	TSubclassOf<AActor> ActorClass,
	FGuid ItemInstanceID)
{
	if (!ActorClass || !PhysicsHandle || IsHoldingObject())
	{
		return false;
	}

	if (!ItemInstanceID.IsValid())
	{
		// Inventory-originated objects must always identify their source instance.
		return false;
	}

	if (!ActorClass->IsChildOf(ABuildableActor::StaticClass()))
	{
		return false;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !GetWorld())
	{
		return false;
	}

	const FVector SpawnDirection = GetEyesRotation().Vector();
	const FVector SpawnLocation =
		GetEyesLocation() + SpawnDirection * GrabDistance;

	const FRotator SpawnRotation =
		SnapRotatorToIncrement(GetEyesRotation());

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = Pawn;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* NewActor = GetWorld()->SpawnActor<AActor>(
		ActorClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams);

	if (!NewActor)
	{
		return false;
	}

	ABuildableActor* BuildableActor = Cast<ABuildableActor>(NewActor);
	if (!BuildableActor)
	{
		NewActor->Destroy();
		return false;
	}

	UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(
		NewActor->GetRootComponent());

	if (!Primitive)
	{
		NewActor->Destroy();
		return false;
	}

	Primitive->SetSimulatePhysics(true);

	if (!Primitive->IsSimulatingPhysics())
	{
		NewActor->Destroy();
		return false;
	}

	BuildableActor->SetSourceItemInstanceID(ItemInstanceID);

	GrabbedLocalAnchor =
		NewActor->GetActorTransform().InverseTransformPosition(
			SpawnLocation);

	PhysicsHandle->GrabComponentAtLocationWithRotation(
		Primitive,
		NAME_None,
		SpawnLocation,
		SpawnRotation);

	if (PhysicsHandle->GetGrabbedComponent() != Primitive)
	{
		PhysicsHandle->ReleaseComponent();
		NewActor->Destroy();
		GrabbedLocalAnchor = FVector::ZeroVector;
		return false;
	}

	GrabbedActor = NewActor;
	bSpawnedFromInventory = true;
	SourceItemInstanceID = ItemInstanceID;
	BuildState = EBuildState::HoldingInventoryObject;

	CurrentSnappedRotation = SpawnRotation;
	CurrentHoldDistance = FMath::Clamp(
		GrabDistance,
		MinHoldDistance,
		MaxHoldDistance);

	ClearSnapPreview();

	if (GrabSpline)
	{
		GrabSpline->SetVisibility(true);
	}

	if (GrabSplineMesh)
	{
		GrabSplineMesh->SetVisibility(true);
	}

	return true;
}

void UBuildComponent::ReleaseHeldObject()
{
	if (!GrabbedActor || !PhysicsHandle)
	{
		return;
	}

	CleanupBuildState(true);
}

void UBuildComponent::DetachHeldPiece()
{
	FHitResult Hit;
	if (!TraceForward(Hit))
	{
		return;
	}

	AActor* HitActor = Hit.GetActor();
	if (!HitActor || !IsBuildableActor(HitActor))
	{
		return;
	}

	if (!HitActor->GetAttachParentActor())
	{
		return;
	}

	const FVector DetachLocation = HitActor->GetActorLocation();

	HitActor->DetachFromActor(
		FDetachmentTransformRules::KeepWorldTransform);

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	HitActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (Primitive)
		{
			Primitive->SetSimulatePhysics(true);
		}
	}

	OnDetached.Broadcast(DetachLocation);
}

void UBuildComponent::SetRotationModeActive(bool bActive)
{
	bRotationModeActive = bActive;

	if (bActive && GrabbedActor && GizmoActorClass && !SpawnedGizmo)
	{
		if (GetWorld())
		{
			FActorSpawnParameters Params;
			Params.Owner = GetOwner();

			SpawnedGizmo = GetWorld()->SpawnActor<AActor>(
				GizmoActorClass,
				GrabbedActor->GetActorTransform(),
				Params);
		}
	}
	else if (!bActive && SpawnedGizmo)
	{
		SpawnedGizmo->Destroy();
		SpawnedGizmo = nullptr;
	}

	if (bActive && SpawnedGizmo)
	{
		UpdateGizmoTransform();
	}
}

void UBuildComponent::RotateStep(float YawSteps, float PitchSteps)
{
	if (!bRotationModeActive || !GrabbedActor)
	{
		return;
	}

	if (FMath::IsNearlyZero(YawSteps) &&
		FMath::IsNearlyZero(PitchSteps))
	{
		return;
	}

	CurrentSnappedRotation.Yaw +=
		FMath::Sign(YawSteps) * RotationSnapIncrement;

	CurrentSnappedRotation.Pitch +=
		FMath::Sign(PitchSteps) * RotationSnapIncrement;

	CurrentSnappedRotation =
		SnapRotatorToIncrement(CurrentSnappedRotation);

	ClearSnapPreview();
}

void UBuildComponent::SetHoldDistanceDelta(float Delta)
{
	if (!FMath::IsFinite(Delta))
	{
		return;
	}

	CurrentHoldDistance = FMath::Clamp(
		CurrentHoldDistance + Delta,
		MinHoldDistance,
		MaxHoldDistance);
}

FVector UBuildComponent::GetEyesLocation() const
{
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		FVector Location;
		FRotator Rotation;
		Pawn->GetActorEyesViewPoint(Location, Rotation);
		return Location;
	}

	return GetOwner()
		? GetOwner()->GetActorLocation()
		: FVector::ZeroVector;
}

FRotator UBuildComponent::GetEyesRotation() const
{
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		FVector Location;
		FRotator Rotation;
		Pawn->GetActorEyesViewPoint(Location, Rotation);
		return Rotation;
	}

	return GetOwner()
		? GetOwner()->GetActorRotation()
		: FRotator::ZeroRotator;
}

bool UBuildComponent::TraceForward(FHitResult& OutHit) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector Start = GetEyesLocation();
	const FVector End =
		Start + GetEyesRotation().Vector() * GrabDistance;

	FCollisionQueryParams Params(FName(TEXT("RihlaBuildTrace")), false);

	Params.AddIgnoredActor(GetOwner());

	if (GrabbedActor)
	{
		Params.AddIgnoredActor(GrabbedActor);
	}

	return World->LineTraceSingleByChannel(
		OutHit,
		Start,
		End,
		GrabTraceChannel,
		Params);
}

AActor* UBuildComponent::GetAttachRoot(AActor* Start) const
{
	AActor* Root = Start;

	while (Root && Root->GetAttachParentActor())
	{
		Root = Root->GetAttachParentActor();
	}

	return Root;
}

bool UBuildComponent::IsBuildableActor(AActor* Actor) const
{
	return Actor && IsValid(Actor) &&
		Cast<ABuildableActor>(Actor) != nullptr;
}

bool UBuildComponent::FindNearestSnapSocket(
	AActor* Held,
	AActor*& OutActor,
	UStaticMeshComponent*& OutMesh,
	FName& OutTargetSocket,
	FName& OutHeldSocket,
	float& OutDist) const
{
	OutActor = nullptr;
	OutMesh = nullptr;
	OutTargetSocket = NAME_None;
	OutHeldSocket = NAME_None;
	OutDist = TNumericLimits<float>::Max();

	if (!Held || !IsValid(Held) || !GetWorld())
	{
		return false;
	}

	UStaticMeshComponent* HeldMesh =
		Held->FindComponentByClass<UStaticMeshComponent>();

	if (!HeldMesh)
	{
		return false;
	}

	TArray<FName> HeldSnapSockets;
	for (const FName& SocketName : HeldMesh->GetAllSocketNames())
	{
		if (SocketName.ToString().StartsWith(TEXT("Snap_")))
		{
			HeldSnapSockets.Add(SocketName);
		}
	}

	if (HeldSnapSockets.Num() == 0)
	{
		return false;
	}

	// Build candidate actor set around the actual held socket locations rather than
	// only around the actor origin. This prevents large/asymmetric pieces from being missed.
	TSet<AActor*> CandidateActors;

	for (const FName& HeldSocket : HeldSnapSockets)
	{
		const FVector HeldSocketLocation =
			HeldMesh->GetSocketLocation(HeldSocket);

		TArray<FOverlapResult> Overlaps;
		const FCollisionShape Sphere =
			FCollisionShape::MakeSphere(SnapPointRadius);

		FCollisionQueryParams Params(FName(TEXT("RihlaBuildSnapCandidates")), false);

		Params.AddIgnoredActor(GetOwner());
		Params.AddIgnoredActor(Held);

		GetWorld()->OverlapMultiByChannel(
			Overlaps,
			HeldSocketLocation,
			FQuat::Identity,
			SnapCollisionChannel,
			Sphere,
			Params);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* OtherActor = Overlap.GetActor();

			if (!OtherActor ||
				OtherActor == Held ||
				!IsBuildableActor(OtherActor))
			{
				continue;
			}

			if (OtherActor->IsAttachedTo(Held) ||
				Held->IsAttachedTo(OtherActor))
			{
				continue;
			}

			CandidateActors.Add(GetAttachRoot(OtherActor));
		}
	}

	for (AActor* Candidate : CandidateActors)
	{
		if (!Candidate ||
			Candidate == Held ||
			!IsValid(Candidate) ||
			!IsBuildableActor(Candidate))
		{
			continue;
		}

		UStaticMeshComponent* TargetMesh =
			Candidate->FindComponentByClass<UStaticMeshComponent>();

		if (!TargetMesh)
		{
			continue;
		}

		TArray<FName> TargetSnapSockets;
		for (const FName& SocketName : TargetMesh->GetAllSocketNames())
		{
			if (SocketName.ToString().StartsWith(TEXT("Snap_")))
			{
				TargetSnapSockets.Add(SocketName);
			}
		}

		for (const FName& TargetSocket : TargetSnapSockets)
		{
			const FVector TargetLocation =
				TargetMesh->GetSocketLocation(TargetSocket);

			for (const FName& HeldSocket : HeldSnapSockets)
			{
				const FVector HeldLocation =
					HeldMesh->GetSocketLocation(HeldSocket);

				const float Distance =
					FVector::Dist(TargetLocation, HeldLocation);

				if (Distance < SnapPointRadius &&
					Distance < OutDist)
				{
					OutDist = Distance;
					OutActor = Candidate;
					OutMesh = TargetMesh;
					OutTargetSocket = TargetSocket;
					OutHeldSocket = HeldSocket;
				}
			}
		}
	}

	return OutActor != nullptr &&
		OutMesh != nullptr &&
		OutTargetSocket != NAME_None &&
		OutHeldSocket != NAME_None;
}

bool UBuildComponent::CalculateSnapTransform(
	AActor* Held,
	UStaticMeshComponent* HeldMesh,
	FName HeldSocket,
	UStaticMeshComponent* TargetMesh,
	FName TargetSocket,
	FTransform& OutTransform) const
{
	if (!Held ||
		!HeldMesh ||
		!TargetMesh ||
		HeldSocket == NAME_None ||
		TargetSocket == NAME_None)
	{
		return false;
	}

	const FTransform HeldActorTransform = Held->GetActorTransform();
	const FTransform HeldSocketWorld =
		HeldMesh->GetSocketTransform(HeldSocket, RTS_World);
	const FTransform TargetSocketWorld =
		TargetMesh->GetSocketTransform(TargetSocket, RTS_World);

	// Convert the held socket into actor-local space.
	const FVector HeldSocketLocalLocation =
		HeldActorTransform.InverseTransformPosition(
			HeldSocketWorld.GetLocation());

	const FQuat HeldSocketLocalRotation =
		HeldActorTransform.GetRotation().Inverse() *
		HeldSocketWorld.GetRotation();

	// Align the held socket's local frame to the target socket's world frame.
	const FQuat DesiredActorRotation =
		TargetSocketWorld.GetRotation() *
		HeldSocketLocalRotation.Inverse();

	const FQuat SnappedRotation =
		FQuat(SnapRotatorToIncrement(DesiredActorRotation.Rotator()));

	// Recompute actor location using the snapped rotation so the socket remains
	// exactly on the target instead of snapping rotation and leaving a positional error.
	const FVector DesiredActorLocation =
		TargetSocketWorld.GetLocation() -
		SnappedRotation.RotateVector(HeldSocketLocalLocation);

	OutTransform = FTransform(
		SnappedRotation,
		DesiredActorLocation,
		Held->GetActorScale3D());

	return true;
}

bool UBuildComponent::AttachBuiltPiece(AActor* Child, AActor* Parent)
{
	if (!Child ||
		!Parent ||
		Child == Parent ||
		!IsValid(Child) ||
		!IsValid(Parent))
	{
		return false;
	}

	if (Parent->IsAttachedTo(Child) ||
		Child->IsAttachedTo(Parent))
	{
		return false;
	}

	Child->DetachFromActor(
		FDetachmentTransformRules::KeepWorldTransform);

	const FAttachmentTransformRules Rules(
		EAttachmentRule::KeepWorld,
		EAttachmentRule::KeepWorld,
		EAttachmentRule::KeepWorld,
		true);

	if (!Child->AttachToActor(Parent, Rules))
	{
		return false;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Child->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (Primitive)
		{
			Primitive->SetSimulatePhysics(false);
		}
	}

	OnPiecesAttached.Broadcast(Child->GetActorLocation());
	return true;
}

void UBuildComponent::SetBuildActorPhysicsEnabled(AActor* Actor, bool bEnabled) const
{
    if (!Actor || !IsValid(Actor))
    {
        return;
    }

    TArray<UPrimitiveComponent*> PrimitiveComponents;
    Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

    for (UPrimitiveComponent* Primitive : PrimitiveComponents)
    {
        if (Primitive)
        {
            Primitive->SetSimulatePhysics(bEnabled);
        }
    }
}

FRotator UBuildComponent::SnapRotatorToIncrement(
	const FRotator& In) const
{
	const float SafeIncrement =
		FMath::Max(KINDA_SMALL_NUMBER, RotationSnapIncrement);

	auto SnapAxis = [SafeIncrement](float Value) -> float
	{
		return FMath::RoundToFloat(Value / SafeIncrement) * SafeIncrement;
	};

	return FRotator(
		SnapAxis(In.Pitch),
		SnapAxis(In.Yaw),
		SnapAxis(In.Roll)).GetNormalized();
}

void UBuildComponent::UpdateSplineSegment(
	USplineComponent* Spline,
	USplineMeshComponent* SplineMesh,
	const FVector& Start,
	const FVector& End) const
{
	if (!Spline)
	{
		return;
	}

	Spline->ClearSplinePoints(false);
	Spline->AddSplinePoint(
		Start,
		ESplineCoordinateSpace::World,
		false);
	Spline->AddSplinePoint(
		End,
		ESplineCoordinateSpace::World,
		false);
	Spline->UpdateSpline();

	if (!SplineMesh)
	{
		return;
	}

	const FVector StartTangent =
		Spline->GetTangentAtSplinePoint(
			0,
			ESplineCoordinateSpace::Local);

	const FVector EndTangent =
		Spline->GetTangentAtSplinePoint(
			1,
			ESplineCoordinateSpace::Local);

	const FVector LocalStart =
		Spline->GetLocationAtSplinePoint(
			0,
			ESplineCoordinateSpace::Local);

	const FVector LocalEnd =
		Spline->GetLocationAtSplinePoint(
			1,
			ESplineCoordinateSpace::Local);

	SplineMesh->SetStartAndEnd(
		LocalStart,
		StartTangent,
		LocalEnd,
		EndTangent,
		true);
}

void UBuildComponent::ClearGrabSpline()
{
	if (GrabSpline)
	{
		GrabSpline->SetVisibility(false);
		GrabSpline->ClearSplinePoints();
	}

	if (GrabSplineMesh)
	{
		GrabSplineMesh->SetVisibility(false);
	}
}

void UBuildComponent::ClearSnapPreview()
{
	bSnapPreviewActive = false;
	SnapPreviewTargetActor = nullptr;
	SnapPreviewTargetMesh = nullptr;
	SnapPreviewSocketName = NAME_None;
	SnapPreviewHeldSocketName = NAME_None;
	SnapPreviewTransform = FTransform::Identity;
	SnapPreviewDistance = 0.f;

	if (SnapSpline)
	{
		SnapSpline->SetVisibility(false);
		SnapSpline->ClearSplinePoints();
	}

	if (SnapSplineMesh)
	{
		SnapSplineMesh->SetVisibility(false);
	}
}

bool UBuildComponent::GetSnapPreviewSocketLocation(
	FVector& OutLocation) const
{
	if (bSnapPreviewActive &&
		SnapPreviewTargetMesh &&
		SnapPreviewSocketName != NAME_None)
	{
		OutLocation =
			SnapPreviewTargetMesh->GetSocketLocation(
				SnapPreviewSocketName);
		return true;
	}

	OutLocation = FVector::ZeroVector;
	return false;
}

bool UBuildComponent::GetSnapPreviewTransform(
	FTransform& OutTransform) const
{
	if (bSnapPreviewActive)
	{
		OutTransform = SnapPreviewTransform;
		return true;
	}

	OutTransform = FTransform::Identity;
	return false;
}

bool UBuildComponent::IsHoldingObject() const
{
	return GrabbedActor != nullptr &&
		IsValid(GrabbedActor) &&
		PhysicsHandle != nullptr &&
		PhysicsHandle->GetGrabbedComponent() != nullptr;
}

bool UBuildComponent::IsHoldingInventoryObject() const
{
	return IsHoldingObject() && bSpawnedFromInventory;
}

EBuildState UBuildComponent::GetBuildState() const
{
	return BuildState;
}

void UBuildComponent::UpdateGizmoTransform()
{
	if (SpawnedGizmo && GrabbedActor)
	{
		SpawnedGizmo->SetActorLocation(
			GrabbedActor->GetActorLocation());

		SpawnedGizmo->SetActorRotation(
			CurrentSnappedRotation);
	}
}

void UBuildComponent::CleanupBuildState(
	bool bDestroyInventoryPreview)
{
	if (PhysicsHandle)
	{
		PhysicsHandle->ReleaseComponent();
	}

	if (SpawnedGizmo)
	{
		SpawnedGizmo->Destroy();
		SpawnedGizmo = nullptr;
	}

	ClearGrabSpline();
	ClearSnapPreview();

	AActor* ActorToRelease = GrabbedActor;

	if (bDestroyInventoryPreview &&
		bSpawnedFromInventory &&
		ActorToRelease &&
		IsValid(ActorToRelease))
	{
		ActorToRelease->Destroy();
	}

	GrabbedActor = nullptr;
	bSpawnedFromInventory = false;
	SourceItemInstanceID.Invalidate();
	GrabbedLocalAnchor = FVector::ZeroVector;
	BuildState = EBuildState::Idle;
	bRotationModeActive = false;
}

void UBuildComponent::RetryBindBuildInput()
{
	if (BindBuildInput() && GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(
			InputBindRetryTimer);
	}
}

bool UBuildComponent::BindBuildInput()
{
	if (bBuildInputBound)
	{
		return true;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return false;
	}

	APlayerController* PC =
		Cast<APlayerController>(Pawn->GetController());

	if (!PC)
	{
		return false;
	}

	UEnhancedInputComponent* EnhancedInput =
		Cast<UEnhancedInputComponent>(Pawn->InputComponent);

	if (!EnhancedInput)
	{
		return false;
	}

	if (IA_BuildGrab)
	{
		EnhancedInput->BindAction(
			IA_BuildGrab,
			ETriggerEvent::Started,
			this,
			&UBuildComponent::HandleGrabInput);
	}

	if (IA_BuildRelease)
	{
		EnhancedInput->BindAction(
			IA_BuildRelease,
			ETriggerEvent::Started,
			this,
			&UBuildComponent::HandleReleaseInput);
	}

	if (IA_BuildDetach)
	{
		EnhancedInput->BindAction(
			IA_BuildDetach,
			ETriggerEvent::Started,
			this,
			&UBuildComponent::HandleDetachInput);
	}

	if (IA_BuildRotationMode)
	{
		EnhancedInput->BindAction(
			IA_BuildRotationMode,
			ETriggerEvent::Started,
			this,
			&UBuildComponent::HandleRotationModeStarted);

		EnhancedInput->BindAction(
			IA_BuildRotationMode,
			ETriggerEvent::Completed,
			this,
			&UBuildComponent::HandleRotationModeCompleted);
	}

	if (IA_BuildRotate)
	{
		EnhancedInput->BindAction(
			IA_BuildRotate,
			ETriggerEvent::Triggered,
			this,
			&UBuildComponent::HandleRotateInput);
	}

	if (IA_BuildHoldDistance)
	{
		EnhancedInput->BindAction(
			IA_BuildHoldDistance,
			ETriggerEvent::Triggered,
			this,
			&UBuildComponent::HandleHoldDistanceInput);
	}

	BoundInputComponent = EnhancedInput;
	bBuildInputBound = true;
	return true;
}

void UBuildComponent::UnbindBuildInput()
{
	if (BoundInputComponent)
	{
		BoundInputComponent->ClearBindingsForObject(this);
		BoundInputComponent = nullptr;
	}

	bBuildInputBound = false;
}

void UBuildComponent::HandleGrabInput(
	const FInputActionValue& Value)
{
	TryGrabOrConfirm();
}

void UBuildComponent::HandleReleaseInput(
	const FInputActionValue& Value)
{
	ReleaseHeldObject();
}

void UBuildComponent::HandleDetachInput(
	const FInputActionValue& Value)
{
	DetachHeldPiece();
}

void UBuildComponent::HandleRotationModeStarted(
	const FInputActionValue& Value)
{
	SetRotationModeActive(true);
}

void UBuildComponent::HandleRotationModeCompleted(
	const FInputActionValue& Value)
{
	SetRotationModeActive(false);
}

void UBuildComponent::HandleRotateInput(
	const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();

	// Convert continuous controller/mouse-axis values into discrete 45-degree steps.
	// A small dead-zone prevents tiny analog noise from causing unintended rotation.
	constexpr float DeadZone = 0.25f;

	const float YawInput =
		FMath::Abs(Axis.X) >= DeadZone ? Axis.X : 0.f;

	const float PitchInput =
		FMath::Abs(Axis.Y) >= DeadZone ? Axis.Y : 0.f;

	RotateStep(YawInput, PitchInput);
}

void UBuildComponent::HandleHoldDistanceInput(
	const FInputActionValue& Value)
{
	const float Axis = Value.Get<float>();

	if (FMath::IsNearlyZero(Axis))
	{
		return;
	}

	SetHoldDistanceDelta(
		Axis * HoldDistanceScrollSpeed);
}
