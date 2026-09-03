// Rihla — Player Stats Component
// UE 5.7.4
//
// Authoritative runtime state for health, temporary/gold health, stamina,
// temporary/enduring stamina, and battery. Gameplay systems should mutate
// resources through this API rather than writing the public state directly.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerStatsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, int32, NewHealth, int32, NewMaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTemporaryHealthChanged, int32, NewTemporaryHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaminaChanged, float, NewStamina, float, NewMaxStamina);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTemporaryStaminaChanged, float, NewTemporaryStamina);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBatteryChanged, float, NewBattery, float, NewMaxBattery);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDeath);

UCLASS(ClassGroup = (Rihla), meta = (BlueprintSpawnableComponent))
class RIHLA_API UPlayerStatsComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPlayerStatsComponent();

protected:
    virtual void BeginPlay() override;

public:
    // One health unit = one quarter-heart. 4 units = 1 full heart.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Health", meta = (ClampMin = "0"))
    int32 MaxHealth = 12;

    UPROPERTY(BlueprintReadOnly, Category = "Stats|Health", meta = (ClampMin = "0"))
    int32 CurrentHealth = 12;

    // Additional health above MaxHealth. It is consumed before normal health.
    UPROPERTY(BlueprintReadOnly, Category = "Stats|Health", meta = (ClampMin = "0"))
    int32 TemporaryHealth = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    bool bIsDead = false;

    // One unit = one stamina-wheel segment.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Stamina", meta = (ClampMin = "0.0"))
    float MaxStamina = 3.f;

    UPROPERTY(BlueprintReadOnly, Category = "Stats|Stamina", meta = (ClampMin = "0.0"))
    float CurrentStamina = 3.f;

    // Additional stamina above MaxStamina.
    UPROPERTY(BlueprintReadOnly, Category = "Stats|Stamina", meta = (ClampMin = "0.0"))
    float TemporaryStamina = 0.f;

    // Battery starts at zero capacity until progression grants capacity.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Battery", meta = (ClampMin = "0.0"))
    float MaxBattery = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Stats|Battery", meta = (ClampMin = "0.0"))
    float CurrentBattery = 0.f;

    UPROPERTY(BlueprintAssignable, Category = "Stats|Events")
    FOnHealthChanged OnHealthChanged;

    UPROPERTY(BlueprintAssignable, Category = "Stats|Events")
    FOnTemporaryHealthChanged OnTemporaryHealthChanged;

    UPROPERTY(BlueprintAssignable, Category = "Stats|Events")
    FOnStaminaChanged OnStaminaChanged;

    UPROPERTY(BlueprintAssignable, Category = "Stats|Events")
    FOnTemporaryStaminaChanged OnTemporaryStaminaChanged;

    UPROPERTY(BlueprintAssignable, Category = "Stats|Events")
    FOnBatteryChanged OnBatteryChanged;

    UPROPERTY(BlueprintAssignable, Category = "Stats|Events")
    FOnPlayerDeath OnPlayerDeath;

    UFUNCTION(BlueprintCallable, Category = "Stats|Health")
    void RestoreHealth(int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Stats|Health")
    void AddTemporaryHealth(int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Stats|Health")
    void ClearTemporaryHealth();

    UFUNCTION(BlueprintCallable, Category = "Stats|Health")
    void TakeDamage(int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Stats|Health")
    void ReviveAndResetStats();

    UFUNCTION(BlueprintPure, Category = "Stats|Health")
    float GetHealthPercent() const;

    UFUNCTION(BlueprintPure, Category = "Stats|Health")
    float GetHeartsDisplay() const;

    UFUNCTION(BlueprintPure, Category = "Stats|Health")
    int32 GetTotalHealthUnits() const;

    UFUNCTION(BlueprintPure, Category = "Stats|Health")
    float GetTotalHeartsDisplay() const;

    UFUNCTION(BlueprintCallable, Category = "Stats|Stamina")
    void RestoreStamina(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Stats|Stamina")
    void AddTemporaryStamina(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Stats|Stamina")
    void ClearTemporaryStamina();

    // Returns false and changes nothing when available stamina is insufficient.
    UFUNCTION(BlueprintCallable, Category = "Stats|Stamina")
    bool DrainStamina(float Amount);

    UFUNCTION(BlueprintPure, Category = "Stats|Stamina")
    float GetStaminaPercent() const;

    UFUNCTION(BlueprintPure, Category = "Stats|Stamina")
    float GetTotalStamina() const;

    UFUNCTION(BlueprintCallable, Category = "Stats|Battery")
    void RestoreBattery(float Amount);

    // Returns false and changes nothing when battery is insufficient.
    UFUNCTION(BlueprintCallable, Category = "Stats|Battery")
    bool DrainBattery(float Amount);

    UFUNCTION(BlueprintPure, Category = "Stats|Battery")
    float GetBatteryPercent() const;

    UFUNCTION(BlueprintCallable, Category = "Stats|Save")
    void InitializeNewGameStats();

    // Applies a complete saved snapshot while enforcing invariants. It can be
    // called before or after BeginPlay; BeginPlay will not overwrite a snapshot
    // that has already been applied.
    UFUNCTION(BlueprintCallable, Category = "Stats|Save")
    void ApplySavedStats(
        int32 SavedMaxHealth,
        int32 SavedCurrentHealth,
        int32 SavedTemporaryHealth,
        float SavedMaxStamina,
        float SavedCurrentStamina,
        float SavedTemporaryStamina,
        float SavedMaxBattery,
        float SavedCurrentBattery,
        bool bSavedIsDead);

    UFUNCTION(BlueprintCallable, Category = "Stats|Progression")
    void SetMaxHealth(int32 NewMaxHealth, bool bFillCurrentHealth = false);

    UFUNCTION(BlueprintCallable, Category = "Stats|Progression")
    void SetMaxStamina(float NewMaxStamina, bool bFillCurrentStamina = false);

    UFUNCTION(BlueprintCallable, Category = "Stats|Progression")
    void SetMaxBattery(float NewMaxBattery, bool bFillCurrentBattery = false);

private:
    // Prevents BeginPlay from resetting a snapshot already supplied by save/load.
    bool bHasAppliedSavedStats = false;
};
