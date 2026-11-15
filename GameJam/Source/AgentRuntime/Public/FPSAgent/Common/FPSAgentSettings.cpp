#include "Common/FPSAgentSettings.h"

UFPSAgentSettings::UFPSAgentSettings()
{
	CategoryName = TEXT("Game");

	auto Lane = [](float Distance, float Lateral, ERangeBotMode Mode)
	{
		FRangeLane L;
		L.DistanceCm = Distance;
		L.LateralCm = Lateral;
		L.Mode = Mode;
		return L;
	};
	Range.Lanes = {
		Lane(1000.f,    0.f, ERangeBotMode::Static),
		Lane(1500.f, -500.f, ERangeBotMode::Strafe),
		Lane(2000.f,  500.f, ERangeBotMode::Static),
		Lane(3000.f,    0.f, ERangeBotMode::Strafe),
		Lane(4500.f, -500.f, ERangeBotMode::Static),
	};
}
