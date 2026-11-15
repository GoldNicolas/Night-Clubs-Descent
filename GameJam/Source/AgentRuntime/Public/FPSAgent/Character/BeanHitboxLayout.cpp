#include "Character/BeanHitboxLayout.h"

FBeanHitboxLayout ComputeBeanHitboxLayout(const FBeanBody& B, float H)
{
	FBeanHitboxLayout L;
	L.HeadRadius = B.HeadRadius;
	L.HeadCenterZ = H - B.HeadTopClearance - B.HeadRadius;
	const float LegsHeight = B.LegFraction * 2.f * H;
	const float LegsTopZ = -H + LegsHeight;
	const float BodyTopZ = L.HeadCenterZ - B.HeadRadius;
	L.LegsCenterZ = -H + 0.5f * LegsHeight;
	L.LegsExtent = FVector(B.LegsHalfDepth, B.LegsHalfWidth, 0.5f * LegsHeight);
	L.BodyCenterZ = 0.5f * (LegsTopZ + BodyTopZ);
	L.BodyExtent = FVector(B.BodyHalfDepth, B.BodyHalfWidth, 0.5f * (BodyTopZ - LegsTopZ));
	L.EyeZ = L.HeadCenterZ;
	return L;
}
