#include "GJAssetManager.h"
#include "GameJam.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace
{
	/** The project's own script packages: their native classes hold the settings and the constructor asset references. */
	bool IsProjectClass(const UClass* Class)
	{
		static const FName GameJam(TEXT("/Script/GameJam"));
		static const FName AgentRuntime(TEXT("/Script/AgentRuntime"));
		const FName Package = Class->GetOutermost()->GetFName();
		return Package == GameJam || Package == AgentRuntime;
	}

	void ScanValues(const UStruct* Type, const void* Container, TSet<FSoftObjectPath>& Found)
	{
		auto Consider = [&Found](const FSoftObjectPath& Path)
		{
			if (Path.IsValid() && Path.GetLongPackageName().StartsWith(TEXT("/Game/")))
			{
				Found.Add(Path);
			}
		};
		// Every property value, recursing into arrays, sets, maps and structs (TArray<FSoftObjectPath> dance lists and so on).
		for (TPropertyValueIterator<FProperty> It(Type, Container); It; ++It)
		{
			const FProperty* Property = It->Key;
			const void* Value = It->Value;
			if (const FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property))
			{
				Consider(static_cast<const FSoftObjectPtr*>(Value)->ToSoftObjectPath());
			}
			else if (const FObjectPropertyBase* Hard = CastField<FObjectPropertyBase>(Property))
			{
				if (const UObject* Object = Hard->GetObjectPropertyValue(Value))
				{
					Consider(FSoftObjectPath(Object));
				}
			}
			else if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
			{
				if (Struct->Struct == TBaseStructure<FSoftObjectPath>::Get() || Struct->Struct == TBaseStructure<FSoftClassPath>::Get())
				{
					Consider(*static_cast<const FSoftObjectPath*>(Value));
				}
			}
		}
	}

	FAutoConsoleCommandWithArgsAndOutputDevice CmdVerifyContent(TEXT("gj.VerifyContent"),
		TEXT("gj.VerifyContent [quit]: load every asset the game reaches by path (Project Settings, class defaults) and report "
			"missing ones. 'quit' exits afterwards, with code 1 if anything is missing."),
		FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, FOutputDevice&)
		{
			TArray<FSoftObjectPath> Assets;
			UGJAssetManager::CollectCodeReferencedAssets(Assets);
			int32 Missing = 0;
			for (const FSoftObjectPath& Asset : Assets)
			{
				if (!Asset.TryLoad())
				{
					++Missing;
					UE_LOG(LogGameJam, Error, TEXT("VerifyContent: MISSING %s"), *Asset.ToString());
				}
			}
			if (Missing == 0)
			{
				UE_LOG(LogGameJam, Display, TEXT("VerifyContent: PASS %d assets"), Assets.Num());
			}
			else
			{
				UE_LOG(LogGameJam, Error, TEXT("VerifyContent: FAIL %d of %d assets missing"), Missing, Assets.Num());
			}
			if (Args.Contains(TEXT("quit")))
			{
				FPlatformMisc::RequestExitWithStatus(false, Missing == 0 ? 0 : 1);
			}
		}));
}

void UGJAssetManager::CollectCodeReferencedAssets(TArray<FSoftObjectPath>& OutAssets)
{
	TSet<FSoftObjectPath> Found;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		const UClass* Class = *It;
		if (!Class->HasAnyClassFlags(CLASS_Native) || Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists)
			|| !IsProjectClass(Class))
		{
			continue;
		}
		UObject* Defaults = Class->GetDefaultObject(/*bCreateIfNeeded=*/false);
		if (!Defaults)
		{
			continue;
		}
		ScanValues(Class, Defaults, Found);
		// Components and other default subobjects carry the constructor's meshes and anim classes.
		TArray<UObject*> Subobjects;
		Defaults->GetDefaultSubobjects(Subobjects);
		for (const UObject* Subobject : Subobjects)
		{
			ScanValues(Subobject->GetClass(), Subobject, Found);
		}
	}
	OutAssets = Found.Array();
	OutAssets.Sort([](const FSoftObjectPath& A, const FSoftObjectPath& B) { return A.ToString() < B.ToString(); });
}

#if WITH_EDITOR
void UGJAssetManager::ModifyCook(TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& PackagesToCook,
	TArray<FName>& PackagesToNeverCook)
{
	Super::ModifyCook(TargetPlatforms, PackagesToCook, PackagesToNeverCook);

	TArray<FSoftObjectPath> Assets;
	CollectCodeReferencedAssets(Assets);
	TArray<FString> Packages;
	for (const FSoftObjectPath& Asset : Assets)
	{
		const FString Package = Asset.GetLongPackageName();
		if (!Packages.Contains(Package))
		{
			Packages.Add(Package);
			PackagesToCook.AddUnique(FName(*Package));
		}
	}
	FFileHelper::SaveStringArrayToFile(Packages, *(FPaths::ProjectSavedDir() / TEXT("GJCookManifest.txt")));
	UE_LOG(LogGameJam, Display, TEXT("GJAssetManager: added %d packages the game loads by path (see Saved/GJCookManifest.txt)"),
		Packages.Num());
}
#endif
