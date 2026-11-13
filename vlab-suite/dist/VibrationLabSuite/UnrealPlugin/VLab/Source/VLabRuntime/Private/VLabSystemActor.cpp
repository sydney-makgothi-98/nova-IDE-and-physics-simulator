#include "VLabSystemActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "UObject/ConstructorHelpers.h"
#include "VLabSystemAsset.h"

#include "vlab/project_io.hpp"
#include "vlab/session.hpp"

static UStaticMesh* LoadBasicMesh(const TCHAR* Path)
{
	return Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, Path));
}

AVLabSystemActor::AVLabSystemActor()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

AVLabSystemActor::~AVLabSystemActor() = default;

void AVLabSystemActor::BeginPlay()
{
	Super::BeginPlay();
	if (bAutoStart)
	{
		StartSimulation();
	}
}

void AVLabSystemActor::EndPlay(const EEndPlayReason::Type Reason)
{
	StopSimulation();
	Super::EndPlay(Reason);
}

bool AVLabSystemActor::StartSimulation()
{
	if (!System || System->VlabText.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("VLab: assign a VLab System Asset with some design text first."));
		return false;
	}
	vlab::Model Model;
	std::string Error;
	if (!vlab::modelFromText(TCHAR_TO_UTF8(*System->VlabText), Model, &Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("VLab: could not read the design: %s"), UTF8_TO_TCHAR(Error.c_str()));
		return false;
	}
	Sim = MakeUnique<vlab::Session>();
	Sim->setDesign(Model);
	Sim->design().run.duration = 1.0e9;      // in a level the simulation runs for as long as the game does
	Sim->start();
	BuildMeshes();
	SyncTransforms();
	return true;
}

void AVLabSystemActor::StopSimulation()
{
	Sim.Reset();
	ClearMeshes();
}

bool AVLabSystemActor::ResetSimulation()
{
	StopSimulation();
	return StartSimulation();
}

void AVLabSystemActor::SetIdealPhysics(bool bIdeal)
{
	if (!Sim || !Sim->world()) return;
	vlab::Environment& Env = Sim->design().env;
	if (bIdeal) Env.setIdeal(); else Env.setRealistic();
	Sim->syncLiveEdits();
}

void AVLabSystemActor::SetGravity(float G)
{
	if (!Sim || !Sim->world()) return;
	Sim->design().env.gravity = G;
	Sim->syncLiveEdits();
}

float AVLabSystemActor::GetSimulationTime() const { return Sim ? (float)Sim->simTime() : 0.f; }
float AVLabSystemActor::GetTotalEnergy() const { return (Sim && Sim->world()) ? (float)Sim->world()->ledger().E : 0.f; }
float AVLabSystemActor::GetEnergyLost() const { return (Sim && Sim->world()) ? (float)Sim->world()->ledger().dissipated() : 0.f; }

bool AVLabSystemActor::GetBodyLocation(const FString& BodyName, FVector& Out) const
{
	if (!Sim || !Sim->world()) return false;
	const vlab::Model& M = Sim->world()->model();
	for (size_t i = 0; i < M.bodies.size(); ++i)
	{
		if (FString(UTF8_TO_TCHAR(M.bodies[i].name.c_str())) == BodyName)
		{
			const vlab::BodyState& S = Sim->world()->state()[i];
			Out = ToWorld(S.pos.x, S.pos.y);
			return true;
		}
	}
	return false;
}

bool AVLabSystemActor::GetBodySpeed(const FString& BodyName, float& Out) const
{
	if (!Sim || !Sim->world()) return false;
	const vlab::Model& M = Sim->world()->model();
	for (size_t i = 0; i < M.bodies.size(); ++i)
	{
		if (FString(UTF8_TO_TCHAR(M.bodies[i].name.c_str())) == BodyName)
		{
			Out = (float)vlab::length(Sim->world()->state()[i].vel);
			return true;
		}
	}
	return false;
}

bool AVLabSystemActor::SaveTelemetryCsv(const FString& FilePath)
{
	if (!Sim) return false;
	const vlab::Telemetry& T = Sim->telemetry();
	FString Out = TEXT("Time (s)");
	for (const vlab::Channel& C : T.channels)
	{
		Out += TEXT(",") + FString(UTF8_TO_TCHAR(C.name.c_str())) + TEXT(" (") + FString(UTF8_TO_TCHAR(C.unit.c_str())) + TEXT(")");
	}
	Out += TEXT("\r\n");
	for (size_t r = 0; r < T.rows(); ++r)
	{
		Out += FString::Printf(TEXT("%.9g"), T.t[r]);
		for (size_t c = 0; c < T.col.size(); ++c)
		{
			Out += FString::Printf(TEXT(",%.9g"), T.col[c][r]);
		}
		Out += TEXT("\r\n");
	}
	return FFileHelper::SaveStringToFile(Out, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

FVector AVLabSystemActor::ToWorld(double X, double Y) const
{
	// the simulation's y points DOWN, so "up" in the plane is -y
	const FVector Local = PlaneRight.GetSafeNormal() * (X * UnitsPerMetre) + PlaneUp.GetSafeNormal() * (-Y * UnitsPerMetre);
	return GetActorLocation() + Local;
}

void AVLabSystemActor::ClearMeshes()
{
	for (UStaticMeshComponent* C : BodyMeshes)
	{
		if (C) C->DestroyComponent();
	}
	BodyMeshes.Reset();
}

void AVLabSystemActor::BuildMeshes()
{
	ClearMeshes();
	if (!Sim || !Sim->world()) return;
	UStaticMesh* Sphere = LoadBasicMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cube = LoadBasicMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	const vlab::Model& M = Sim->world()->model();
	for (const vlab::Body& B : M.bodies)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetupAttachment(GetRootComponent());
		C->RegisterComponent();
		const bool bRound = B.shape.isCircle();
		C->SetStaticMesh(bRound ? Sphere : Cube);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);        // the Vibration Lab engine does the collisions, not Chaos
		const float U = UnitsPerMetre / 100.0f;                        // basic shapes are 100 units across
		if (bRound)
		{
			const float D = (float)(B.shape.radius * 2.0) * U;
			C->SetWorldScale3D(FVector(D, D, D));
		}
		else
		{
			const float R = (float)B.shape.boundingRadius();
			float W = B.shape.preset == vlab::ShapePreset::Box || B.shape.preset == vlab::ShapePreset::Wedge ? (float)B.shape.w : R * 2.0f;
			float H = B.shape.preset == vlab::ShapePreset::Box || B.shape.preset == vlab::ShapePreset::Wedge ? (float)B.shape.h : R * 2.0f;
			// X = depth (plane normal), Y = plane right, Z = plane up
			C->SetWorldScale3D(FVector(VisualDepth / 100.0f, W * U, H * U));
		}
		BodyMeshes.Add(C);
	}
}

void AVLabSystemActor::SyncTransforms()
{
	if (!Sim || !Sim->world()) return;
	const vlab::World& W = *Sim->world();
	const FVector Normal = FVector::CrossProduct(PlaneRight.GetSafeNormal(), PlaneUp.GetSafeNormal());
	for (size_t i = 0; i < BodyMeshes.Num() && i < W.state().size(); ++i)
	{
		const vlab::BodyState& S = W.state()[i];
		const float Deg = (float)(S.angle * 180.0 / vlab::kPi) * (bFlipRotation ? -1.f : 1.f);
		const FQuat Q = FQuat(Normal, FMath::DegreesToRadians(Deg));
		BodyMeshes[i]->SetWorldLocationAndRotation(ToWorld(S.pos.x, S.pos.y), Q);
	}
}

void AVLabSystemActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Sim || !Sim->world()) return;
	Sim->advance(FMath::Min(DeltaSeconds, 0.1f) * TimeScale);
	SyncTransforms();
}
