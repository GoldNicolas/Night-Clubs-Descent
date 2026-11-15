#include "GJEditorTools.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_RotationOffsetBlendSpace.h"
#include "AnimGraphNode_Slot.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

FString UGJEditorTools::InsertUpperBodySlot(UAnimBlueprint* AnimBlueprint, FName SlotName)
{
#if WITH_EDITOR
	if (!AnimBlueprint || SlotName.IsNone())
	{
		return TEXT("nothing done: no Anim Blueprint or no slot name");
	}
	TArray<UEdGraph*> Graphs;
	AnimBlueprint->GetAllGraphs(Graphs);
	UEdGraph* AnimGraph = nullptr;
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetFName() == TEXT("AnimGraph"))
		{
			AnimGraph = Graph;
			break;
		}
	}
	if (!AnimGraph)
	{
		return TEXT("nothing done: no AnimGraph");
	}

	UAnimGraphNode_LayeredBoneBlend* Blend = nullptr;
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		if (UAnimGraphNode_LayeredBoneBlend* Layered = Cast<UAnimGraphNode_LayeredBoneBlend>(Node))
		{
			Blend = Layered;
			break;
		}
	}
	if (!Blend)
	{
		return TEXT("nothing done: no Layered blend per bone in the AnimGraph");
	}
	UEdGraphPin* LayerIn = Blend->FindPin(TEXT("BlendPoses_0"), EGPD_Input);
	if (!LayerIn || LayerIn->LinkedTo.Num() != 1)
	{
		return TEXT("nothing done: the layered blend's BlendPoses_0 is not fed by exactly one pose");
	}
	// Where the slot goes: before the aim offset when the upper body is "pose -> aim offset", so montages keep the aim (the
	// aim offset is additive on top of them); otherwise straight before the layer.
	UEdGraphPin* Target = LayerIn;
	if (UEdGraphNode* Upper = LayerIn->LinkedTo[0]->GetOwningNode())
	{
		if (Upper->IsA<UAnimGraphNode_RotationOffsetBlendSpace>())
		{
			UEdGraphPin* AimBase = Upper->FindPin(TEXT("BasePose"), EGPD_Input);
			if (AimBase && AimBase->LinkedTo.Num() == 1)
			{
				Target = AimBase;
			}
		}
	}
	UEdGraphPin* Source = Target->LinkedTo[0];
	UEdGraphNode* SourceNode = Source->GetOwningNode();
	for (const UEdGraphPin* Feed : { Source, LayerIn->LinkedTo[0] })
	{
		if (const UAnimGraphNode_Slot* Existing = Cast<UAnimGraphNode_Slot>(Feed->GetOwningNode()))
		{
			return FString::Printf(TEXT("already there: Slot '%s' on the upper-body branch"), *Existing->Node.SlotName.ToString());
		}
	}

	// The slot must exist on the skeleton (DefaultSlot always does).
	if (USkeleton* Skeleton = AnimBlueprint->TargetSkeleton)
	{
		if (!Skeleton->ContainsSlotName(SlotName))
		{
			Skeleton->Modify();
			Skeleton->RegisterSlotNode(SlotName);
		}
	}

	AnimGraph->Modify();
	FGraphNodeCreator<UAnimGraphNode_Slot> Creator(*AnimGraph);
	UAnimGraphNode_Slot* SlotNode = Creator.CreateNode(/*bSelectNewNode=*/false);
	SlotNode->Node.SlotName = SlotName;
	SlotNode->NodePosX = (SourceNode->NodePosX + Target->GetOwningNode()->NodePosX) / 2;
	SlotNode->NodePosY = Target->GetOwningNode()->NodePosY + 180;
	Creator.Finalize();

	UEdGraphPin* SlotIn = SlotNode->FindPin(TEXT("Source"), EGPD_Input);
	UEdGraphPin* SlotOut = SlotNode->FindPin(TEXT("Pose"), EGPD_Output);
	const UEdGraphSchema* Schema = AnimGraph->GetSchema();
	if (!SlotIn || !SlotOut || !Schema)
	{
		AnimGraph->RemoveNode(SlotNode);
		return TEXT("nothing done: the new Slot node has no Source / Pose pins");
	}
	Schema->BreakSinglePinLink(Source, Target);
	const bool bIn = Schema->TryCreateConnection(Source, SlotIn);
	const bool bOut = Schema->TryCreateConnection(SlotOut, Target);
	if (!bIn || !bOut)
	{
		// Put the graph back as it was.
		AnimGraph->RemoveNode(SlotNode);
		Schema->TryCreateConnection(Source, Target);
		return TEXT("nothing done: could not wire the Slot node (graph restored)");
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	FKismetEditorUtilities::CompileBlueprint(AnimBlueprint);
	return FString::Printf(TEXT("inserted Slot '%s' between '%s' and '%s'; compiled (save the asset)"), *SlotName.ToString(),
		*SourceNode->GetNodeTitle(ENodeTitleType::ListView).ToString(), *Target->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString());
#else
	return TEXT("editor only");
#endif
}
