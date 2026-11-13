// VLabSystemActor.h -- drop this actor in a level, give it a VLab System asset, press Play.
// The Vibration Lab engine computes the motion (validated against closed-form solutions); Unreal only draws it.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VLabSystemActor.generated.h"

class UStaticMeshComponent;
class UVLabSystemAsset;
namespace vlab { class Session; }

UCLASS(BlueprintType, Blueprintable)
class VLABRUNTIME_API AVLabSystemActor : public AActor
{
	GENERATED_BODY()

public:
	AVLabSystemActor();
	virtual ~AVLabSystemActor() override;

	// ---- setup (details panel)
	/** The design to simulate. Create one with Add > Miscellaneous > Data Asset > VLab System Asset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vibration Lab")
	TObjectPtr<UVLabSystemAsset> System;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vibration Lab")
	bool bAutoStart = true;

	/** Simulation speed relative to game time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vibration Lab", meta = (ClampMin = "0.01", ClampMax = "20"))
	float TimeScale = 1.0f;

	/** Unreal units per metre of the 2-D simulation (100 = real scale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vibration Lab|Plane")
	float UnitsPerMetre = 100.0f;

	/** The 2-D world lives on a plane: its x axis maps to this direction ... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vibration Lab|Plane")
	FVector PlaneRight = FVector(0, 1, 0);

	/** ... and "up" (the opposite of the simulation's y, which points down) maps to this one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vibration Lab|Plane")
	FVector PlaneUp = FVector(0, 0, 1);

	/** Thickness given to the visual meshes in the direction of the plane normal (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vibration Lab|Plane")
	float VisualDepth = 40.0f;

	/** Flip this if bodies spin the wrong way for your plane orientation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vibration Lab|Plane")
	bool bFlipRotation = false;

	// ---- control (Blueprint)
	UFUNCTION(BlueprintCallable, Category = "Vibration Lab")
	bool StartSimulation();

	UFUNCTION(BlueprintCallable, Category = "Vibration Lab")
	void StopSimulation();

	/** Back to the design's starting state and run again. */
	UFUNCTION(BlueprintCallable, Category = "Vibration Lab")
	bool ResetSimulation();

	UFUNCTION(BlueprintCallable, Category = "Vibration Lab")
	void SetIdealPhysics(bool bIdeal);

	UFUNCTION(BlueprintCallable, Category = "Vibration Lab")
	void SetGravity(float MetresPerSecondSquared);

	// ---- telemetry (Blueprint)
	UFUNCTION(BlueprintPure, Category = "Vibration Lab")
	float GetSimulationTime() const;

	/** Total mechanical energy in joules. */
	UFUNCTION(BlueprintPure, Category = "Vibration Lab")
	float GetTotalEnergy() const;

	/** Joules turned into heat so far (dampers, friction, impacts, drag). */
	UFUNCTION(BlueprintPure, Category = "Vibration Lab")
	float GetEnergyLost() const;

	/** Position of a named body in Unreal world space. Returns false if the name is unknown. */
	UFUNCTION(BlueprintCallable, Category = "Vibration Lab")
	bool GetBodyLocation(const FString& BodyName, FVector& OutLocation) const;

	/** Speed of a named body in metres per second. */
	UFUNCTION(BlueprintCallable, Category = "Vibration Lab")
	bool GetBodySpeed(const FString& BodyName, float& OutSpeed) const;

	/** Writes all recorded telemetry to a CSV file (open it in Excel). */
	UFUNCTION(BlueprintCallable, Category = "Vibration Lab")
	bool SaveTelemetryCsv(const FString& FilePath);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void BuildMeshes();
	void ClearMeshes();
	void SyncTransforms();
	FVector ToWorld(double X, double Y) const;

	TUniquePtr<vlab::Session> Sim;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BodyMeshes;
};
