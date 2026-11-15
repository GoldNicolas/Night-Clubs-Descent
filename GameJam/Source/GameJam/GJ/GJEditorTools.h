#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GJEditorTools.generated.h"

class UAnimBlueprint;

/** One-off editor helpers for the animation setup (editor builds only: in a game build they do nothing). Call them from
 *  Python, e.g. unreal.GJEditorTools.insert_upper_body_slot(unreal.load_asset("/Game/Variant_Shooter/Anims/ABP_TP_Rifle"), "DefaultSlot"). */
UCLASS()
class GAMEJAM_API UGJEditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Put a Slot node on the upper-body branch of an Anim Blueprint whose AnimGraph layers an upper-body pose over its
	 * locomotion with "Layered blend per bone" (the template's ABP_TP_Rifle: idle aim -> aim offset -> layer, over the state
	 * machine). The slot goes before the aim offset when there is one, so montages on SlotName play on the upper body, keep
	 * the aim on top, and leave the legs to the locomotion (the enemies' reload, recoil and hit reactions). Idempotent.
	 * Compiles the Blueprint; save the asset afterwards. Returns what it did.
	 */
	UFUNCTION(BlueprintCallable, Category="GameJam|Editor")
	static FString InsertUpperBodySlot(UAnimBlueprint* AnimBlueprint, FName SlotName);
};
