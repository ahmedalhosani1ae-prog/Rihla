// Rihla — Build Component
// UE 5.7.4
//
// Live Ultrahand-style build interaction:
//   - Grab existing BuildableActor objects.
//   - Spawn/grab an inventory-provided BuildableActor.
//   - Hold, rotate, change distance, snap, confirm, cancel, detach.
//   - Inventory-originated actors carry a stable ItemInstanceID.
//   - Inventory consumption happens OUTSIDE this component, from OnBuildConfirmed.
//
// Important responsibility split:
//   BuildComponent owns live build state.
//   Inventory owns item identity/consumption.
//   Static item data resolves ItemID -> ActorClass before SpawnAndGrabBuildable() is called.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "BuildGizmoActor.h"
#include "TimerManager.h"
#include "BuildComponent.generated.h"

class UPhysicsHandleComponent;
class USplineComponent;
class USplineMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UInputAction;
class UEnhancedInputComponent;

UENUM(BlueprintType)
enum class EBuildState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	HoldingWorldObject UMETA(DisplayName = "Holding World Object"),
	HoldingInventoryObject UMETA(DisplayName = "Holding Inventory Object")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuildPiecesAttached, FVector, Location);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuildDetached, FVector, Location);

// Broadcast ONLY after a held object has been successfully confirmed as a placed world object.
// For an inventory-originated actor, listeners should consume SourceItemInstanceID exactly once.
// The actor is still valid when this event fires.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnBuildConfirmed,
	AActor*, PlacedActor,
	bool, bWasFromInventory,
	FGuid, SourceItemInstanceID);

UCLASS(ClassGroup = (Rihla), meta = (BlueprintSpawnableComponent))
class RIHLA_API UBuildComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBuildComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// -------------------------------------------------------------------------
	// Public build operations
	// -------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Build")
	void TryGrabOrConfirm();

	UFUNCTION(BlueprintCallable, Category = "Build")
	bool TryGrab();

	UFUNCTION(BlueprintCallable, Category = "Build")
	bool ConfirmPlacement();

	// Spawns a BuildableActor subclass and immediately grabs it.
	// SourceItemInstanceID must be valid for inventory-originated placement.
	UFUNCTION(BlueprintCallable, Category = "Build")
	bool SpawnAndGrabBuildable(TSubclassOf<AActor> ActorClass, FGuid ItemInstanceID);

	// Releases a world object, or cancels/destroys an inventory-originated preview.
	UFUNCTION(BlueprintCallable, Category = "Build")
	void ReleaseHeldObject();

	UFUNCTION(BlueprintCallable, Category = "Build")
	void DetachHeldPiece();

	UFUNCTION(BlueprintCallable, Category = "Build")
	void SetRotationModeActive(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "Build")
	void RotateStep(float YawSteps, float PitchSteps);

	UFUNCTION(BlueprintCallable, Category = "Build")
	void SetHoldDistanceDelta(float Delta);

	UFUNCTION(BlueprintPure, Category = "Build")
	bool GetSnapPreviewSocketLocation(FVector& OutLocation) const;

	UFUNCTION(BlueprintPure, Category = "Build")
	bool GetSnapPreviewTransform(FTransform& OutTransform) const;

	UFUNCTION(BlueprintPure, Category = "Build")
	bool IsHoldingObject() const;

	UFUNCTION(BlueprintPure, Category = "Build")
	bool IsHoldingInventoryObject() const;

	UFUNCTION(BlueprintPure, Category = "Build")
	EBuildState GetBuildState() const;

	// -------------------------------------------------------------------------
	// Events
	// -------------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Build|Events")
	FOnBuildPiecesAttached OnPiecesAttached;

	UPROPERTY(BlueprintAssignable, Category = "Build|Events")
	FOnBuildDetached OnDetached;

	UPROPERTY(BlueprintAssignable, Category = "Build|Events")
	FOnBuildConfirmed OnBuildConfirmed;

	// -------------------------------------------------------------------------
	// Build settings
	// -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build|Movement", meta = (ClampMin = "1.0"))
	float GrabDistance = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build|Movement", meta = (ClampMin = "1.0"))
	float MinHoldDistance = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build|Movement", meta = (ClampMin = "1.0"))
	float MaxHoldDistance = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build|Snapping", meta = (ClampMin = "1.0"))
	float SnapPointRadius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build|Rotation", meta = (ClampMin = "0.01"))
	float RotationSnapIncrement = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build|Movement", meta = (ClampMin = "0.0"))
	float DefaultHoldDistance = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build|Input", meta = (ClampMin = "0.0"))
	float HoldDistanceScrollSpeed = 25.f;

	// Dedicated configurable channels keep the prototype flexible when collision setup changes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build|Collision")
	TEnumAsByte<ECollisionChannel> GrabTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build|Collision")
	TEnumAsByte<ECollisionChannel> SnapCollisionChannel = ECC_WorldDynamic;

	// Optional line mesh used by the grab/snap guide. If unset, spline objects still function as
	// logical/preview data but no visible spline mesh is rendered.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build|Visuals")
	TObjectPtr<UStaticMesh> SplineVisualMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build|Visuals")
	TObjectPtr<UMaterialInterface> SplineVisualMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	TSubclassOf<ABuildGizmoActor> GizmoActorClass;

	// -------------------------------------------------------------------------
	// Enhanced Input
	// -------------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Build|Input")
	TObjectPtr<UInputAction> IA_BuildGrab = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Build|Input")
	TObjectPtr<UInputAction> IA_BuildRelease = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Build|Input")
	TObjectPtr<UInputAction> IA_BuildDetach = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Build|Input")
	TObjectPtr<UInputAction> IA_BuildRotationMode = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Build|Input")
	TObjectPtr<UInputAction> IA_BuildRotate = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Build|Input")
	TObjectPtr<UInputAction> IA_BuildHoldDistance = nullptr;

	// -------------------------------------------------------------------------
	// Runtime state
	// -------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "Build|State")
	TObjectPtr<AActor> GrabbedActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Build|State")
	bool bSpawnedFromInventory = false;

	UPROPERTY(BlueprintReadOnly, Category = "Build|State")
	FGuid SourceItemInstanceID;

	UPROPERTY(BlueprintReadOnly, Category = "Build|State")
	bool bSnapPreviewActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Build|State")
	EBuildState BuildState = EBuildState::Idle;

private:
	// -------------------------------------------------------------------------
	// Runtime components/state
	// -------------------------------------------------------------------------

	UPROPERTY(Transient)
	TObjectPtr<UPhysicsHandleComponent> PhysicsHandle = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USplineComponent> GrabSpline = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USplineMeshComponent> GrabSplineMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USplineComponent> SnapSpline = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USplineMeshComponent> SnapSplineMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AActor> SpawnedGizmo = nullptr;

	TObjectPtr<UEnhancedInputComponent> BoundInputComponent = nullptr;

	bool bRotationModeActive = false;

	FRotator CurrentSnappedRotation = FRotator::ZeroRotator;
	float CurrentHoldDistance = 200.f;

	// Actor-local point at which the physics handle grabbed the object. Using actor-local space
	// keeps the handle target correct even if the root primitive has a relative transform.
	FVector GrabbedLocalAnchor = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<AActor> SnapPreviewTargetActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SnapPreviewTargetMesh = nullptr;

	FName SnapPreviewSocketName = NAME_None;
	FName SnapPreviewHeldSocketName = NAME_None;
	FTransform SnapPreviewTransform = FTransform::Identity;
	float SnapPreviewDistance = 0.f;

	FTimerHandle InputBindRetryTimer;
	bool bBuildInputBound = false;

	// -------------------------------------------------------------------------
	// Internal build functions
	// -------------------------------------------------------------------------

	FVector GetEyesLocation() const;
	FRotator GetEyesRotation() const;

	bool TraceForward(FHitResult& OutHit) const;
	AActor* GetAttachRoot(AActor* Start) const;

	bool IsBuildableActor(AActor* Actor) const;

	bool FindNearestSnapSocket(
		AActor* Held,
		AActor*& OutActor,
		UStaticMeshComponent*& OutMesh,
		FName& OutTargetSocket,
		FName& OutHeldSocket,
		float& OutDist) const;

	bool CalculateSnapTransform(
		AActor* Held,
		UStaticMeshComponent* HeldMesh,
		FName HeldSocket,
		UStaticMeshComponent* TargetMesh,
		FName TargetSocket,
		FTransform& OutTransform) const;

	bool AttachBuiltPiece(AActor* Child, AActor* Parent);
	void SetBuildActorPhysicsEnabled(AActor* Actor, bool bEnabled) const;

	FRotator SnapRotatorToIncrement(const FRotator& In) const;

	void UpdateSplineSegment(
		USplineComponent* Spline,
		USplineMeshComponent* SplineMesh,
		const FVector& Start,
		const FVector& End) const;

	void ClearGrabSpline();
	void ClearSnapPreview();
	void UpdateGizmoTransform();
	void CleanupBuildState(bool bDestroyInventoryPreview);

	bool BindBuildInput();
	void RetryBindBuildInput();
	void UnbindBuildInput();

	void HandleGrabInput(const FInputActionValue& Value);
	void HandleReleaseInput(const FInputActionValue& Value);
	void HandleDetachInput(const FInputActionValue& Value);
	void HandleRotationModeStarted(const FInputActionValue& Value);
	void HandleRotationModeCompleted(const FInputActionValue& Value);
	void HandleRotateInput(const FInputActionValue& Value);
	void HandleHoldDistanceInput(const FInputActionValue& Value);

	void CreateGuideSplineComponents();
};
