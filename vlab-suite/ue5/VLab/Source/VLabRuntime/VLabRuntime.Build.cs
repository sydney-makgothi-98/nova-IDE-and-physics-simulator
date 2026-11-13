// VLabRuntime.Build.cs -- the Unreal build rules for the Vibration Lab module.
// The physics engine (vlab::World, Session, ...) is plain standard C++ living in ThirdParty/; it is compiled as part of this module.
using System.IO;
using UnrealBuildTool;

public class VLabRuntime : ModuleRules
{
	public VLabRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp17;

		// The engine core parses numbers with std::stod inside try/catch, so exceptions must be on for this module.
		bEnableExceptions = true;

		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty", "include"));

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
	}
}
