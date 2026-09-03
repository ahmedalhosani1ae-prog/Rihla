#include "PlayerStatsComponent.h"

UPlayerStatsComponent::UPlayerStatsComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerStatsComponent::BeginPlay()
{
    Super::BeginPlay();

    if (!bHasAppliedSavedStats)
    {
        InitializeNewGameStats();
    }
}

void UPlayerStatsComponent::InitializeNewGameStats()
{
    bHasAppliedSavedStats = false;

    MaxHealth = FMath::Max(0, MaxHealth);
    CurrentHealth = MaxHealth;
    TemporaryHealth = 0;

    MaxStamina = FMath::Max(0.f, MaxStamina);
    CurrentStamina = MaxStamina;
    TemporaryStamina = 0.f;

    MaxBattery = FMath::Max(0.f, MaxBattery);
    CurrentBattery = MaxBattery;

    bIsDead = false;

    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
    OnTemporaryHealthChanged.Broadcast(TemporaryHealth);
    OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
    OnTemporaryStaminaChanged.Broadcast(TemporaryStamina);
    OnBatteryChanged.Broadcast(CurrentBattery, MaxBattery);
}

void UPlayerStatsComponent::ApplySavedStats(
    int32 SavedMaxHealth,
    int32 SavedCurrentHealth,
    int32 SavedTemporaryHealth,
    float SavedMaxStamina,
    float SavedCurrentStamina,
    float SavedTemporaryStamina,
    float SavedMaxBattery,
    float SavedCurrentBattery,
    bool bSavedIsDead)
{
    bHasAppliedSavedStats = true;

    MaxHealth = FMath::Max(0, SavedMaxHealth);
    CurrentHealth = FMath::Clamp(SavedCurrentHealth, 0, MaxHealth);
    TemporaryHealth = FMath::Max(0, SavedTemporaryHealth);

    MaxStamina = FMath::Max(0.f, SavedMaxStamina);
    CurrentStamina = FMath::Clamp(SavedCurrentStamina, 0.f, MaxStamina);
    TemporaryStamina = FMath::Max(0.f, SavedTemporaryStamina);

    MaxBattery = FMath::Max(0.f, SavedMaxBattery);
    CurrentBattery = FMath::Clamp(SavedCurrentBattery, 0.f, MaxBattery);

    bIsDead = bSavedIsDead || (CurrentHealth <= 0 && TemporaryHealth <= 0);

    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
    OnTemporaryHealthChanged.Broadcast(TemporaryHealth);
    OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
    OnTemporaryStaminaChanged.Broadcast(TemporaryStamina);
    OnBatteryChanged.Broadcast(CurrentBattery, MaxBattery);
}

void UPlayerStatsComponent::RestoreHealth(int32 Amount)
{
    if (Amount <= 0 || bIsDead || MaxHealth <= 0)
    {
        return;
    }

    const int32 OldHealth = CurrentHealth;
    CurrentHealth = FMath::Clamp(CurrentHealth + Amount, 0, MaxHealth);

    if (CurrentHealth != OldHealth)
    {
        OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
    }
}

void UPlayerStatsComponent::AddTemporaryHealth(int32 Amount)
{
    if (Amount <= 0 || bIsDead)
    {
        return;
    }

    TemporaryHealth = FMath::Max(0, TemporaryHealth + Amount);
    OnTemporaryHealthChanged.Broadcast(TemporaryHealth);
}

void UPlayerStatsComponent::ClearTemporaryHealth()
{
    if (TemporaryHealth == 0)
    {
        return;
    }

    TemporaryHealth = 0;
    OnTemporaryHealthChanged.Broadcast(TemporaryHealth);
}

void UPlayerStatsComponent::TakeDamage(int32 Amount)
{
    if (Amount <= 0 || bIsDead)
    {
        return;
    }

    int32 RemainingDamage = Amount;

    if (TemporaryHealth > 0)
    {
        const int32 Absorbed = FMath::Min(TemporaryHealth, RemainingDamage);
        TemporaryHealth -= Absorbed;
        RemainingDamage -= Absorbed;
        OnTemporaryHealthChanged.Broadcast(TemporaryHealth);
    }

    if (RemainingDamage > 0)
    {
        const int32 OldHealth = CurrentHealth;
        CurrentHealth = FMath::Clamp(CurrentHealth - RemainingDamage, 0, MaxHealth);

        if (CurrentHealth != OldHealth)
        {
            OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
        }
    }

    if (CurrentHealth <= 0 && TemporaryHealth <= 0 && !bIsDead)
    {
        bIsDead = true;
        OnPlayerDeath.Broadcast();
    }
}

void UPlayerStatsComponent::ReviveAndResetStats()
{
    CurrentHealth = MaxHealth;
    CurrentStamina = MaxStamina;
    TemporaryHealth = 0;
    TemporaryStamina = 0;
    bIsDead = false;

    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
    OnTemporaryHealthChanged.Broadcast(TemporaryHealth);
    OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
    OnTemporaryStaminaChanged.Broadcast(TemporaryStamina);
}

float UPlayerStatsComponent::GetHealthPercent() const
{
    if (MaxHealth <= 0)
    {
        return 0.f;
    }

    return FMath::Clamp(
        static_cast<float>(CurrentHealth) / static_cast<float>(MaxHealth),
        0.f,
        1.f);
}

float UPlayerStatsComponent::GetHeartsDisplay() const
{
    return static_cast<float>(CurrentHealth) / 4.f;
}

int32 UPlayerStatsComponent::GetTotalHealthUnits() const
{
    return FMath::Max(0, CurrentHealth + TemporaryHealth);
}

float UPlayerStatsComponent::GetTotalHeartsDisplay() const
{
    return static_cast<float>(GetTotalHealthUnits()) / 4.f;
}

void UPlayerStatsComponent::RestoreStamina(float Amount)
{
    if (Amount <= 0.f || bIsDead || MaxStamina <= 0.f)
    {
        return;
    }

    const float OldStamina = CurrentStamina;
    CurrentStamina = FMath::Clamp(CurrentStamina + Amount, 0.f, MaxStamina);

    if (!FMath::IsNearlyEqual(CurrentStamina, OldStamina))
    {
        OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
    }
}

void UPlayerStatsComponent::AddTemporaryStamina(float Amount)
{
    if (Amount <= 0.f || bIsDead)
    {
        return;
    }

    TemporaryStamina = FMath::Max(0.f, TemporaryStamina + Amount);
    OnTemporaryStaminaChanged.Broadcast(TemporaryStamina);
}

void UPlayerStatsComponent::ClearTemporaryStamina()
{
    if (TemporaryStamina <= 0.f)
    {
        return;
    }

    TemporaryStamina = 0.f;
    OnTemporaryStaminaChanged.Broadcast(TemporaryStamina);
}

bool UPlayerStatsComponent::DrainStamina(float Amount)
{
    if (Amount <= 0.f)
    {
        return true;
    }

    if (bIsDead)
    {
        return false;
    }

    const float TotalAvailable = TemporaryStamina + CurrentStamina;
    if (TotalAvailable + KINDA_SMALL_NUMBER < Amount)
    {
        return false;
    }

    float Remaining = Amount;

    if (TemporaryStamina > 0.f)
    {
        const float Absorbed = FMath::Min(TemporaryStamina, Remaining);
        TemporaryStamina -= Absorbed;
        Remaining -= Absorbed;
        OnTemporaryStaminaChanged.Broadcast(TemporaryStamina);
    }

    if (Remaining > 0.f)
    {
        CurrentStamina = FMath::Clamp(CurrentStamina - Remaining, 0.f, MaxStamina);
        OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
    }

    return true;
}

float UPlayerStatsComponent::GetStaminaPercent() const
{
    if (MaxStamina <= 0.f)
    {
        return 0.f;
    }

    return FMath::Clamp(CurrentStamina / MaxStamina, 0.f, 1.f);
}

float UPlayerStatsComponent::GetTotalStamina() const
{
    return FMath::Max(0.f, CurrentStamina + TemporaryStamina);
}

void UPlayerStatsComponent::RestoreBattery(float Amount)
{
    if (Amount <= 0.f || bIsDead || MaxBattery <= 0.f)
    {
        return;
    }

    const float OldBattery = CurrentBattery;
    CurrentBattery = FMath::Clamp(CurrentBattery + Amount, 0.f, MaxBattery);

    if (!FMath::IsNearlyEqual(CurrentBattery, OldBattery))
    {
        OnBatteryChanged.Broadcast(CurrentBattery, MaxBattery);
    }
}

bool UPlayerStatsComponent::DrainBattery(float Amount)
{
    if (Amount <= 0.f)
    {
        return true;
    }

    if (bIsDead || CurrentBattery + KINDA_SMALL_NUMBER < Amount)
    {
        return false;
    }

    CurrentBattery = FMath::Clamp(CurrentBattery - Amount, 0.f, MaxBattery);
    OnBatteryChanged.Broadcast(CurrentBattery, MaxBattery);
    return true;
}

float UPlayerStatsComponent::GetBatteryPercent() const
{
    if (MaxBattery <= 0.f)
    {
        return 0.f;
    }

    return FMath::Clamp(CurrentBattery / MaxBattery, 0.f, 1.f);
}

void UPlayerStatsComponent::SetMaxHealth(int32 NewMaxHealth, bool bFillCurrentHealth)
{
    const int32 OldMax = MaxHealth;
    const int32 OldCurrent = CurrentHealth;

    MaxHealth = FMath::Max(0, NewMaxHealth);
    CurrentHealth = bFillCurrentHealth
        ? MaxHealth
        : FMath::Clamp(CurrentHealth, 0, MaxHealth);

    if (CurrentHealth != OldCurrent || MaxHealth != OldMax)
    {
        OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
    }
}

void UPlayerStatsComponent::SetMaxStamina(float NewMaxStamina, bool bFillCurrentStamina)
{
    const float OldMax = MaxStamina;
    const float OldCurrent = CurrentStamina;

    MaxStamina = FMath::Max(0.f, NewMaxStamina);
    CurrentStamina = bFillCurrentStamina
        ? MaxStamina
        : FMath::Clamp(CurrentStamina, 0.f, MaxStamina);

    if (!FMath::IsNearlyEqual(CurrentStamina, OldCurrent) ||
        !FMath::IsNearlyEqual(MaxStamina, OldMax))
    {
        OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
    }
}

void UPlayerStatsComponent::SetMaxBattery(float NewMaxBattery, bool bFillCurrentBattery)
{
    const float OldMax = MaxBattery;
    const float OldCurrent = CurrentBattery;

    MaxBattery = FMath::Max(0.f, NewMaxBattery);
    CurrentBattery = bFillCurrentBattery
        ? MaxBattery
        : FMath::Clamp(CurrentBattery, 0.f, MaxBattery);

    if (!FMath::IsNearlyEqual(CurrentBattery, OldCurrent) ||
        !FMath::IsNearlyEqual(MaxBattery, OldMax))
    {
        OnBatteryChanged.Broadcast(CurrentBattery, MaxBattery);
    }
}
