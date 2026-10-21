#include "BulletEditor/WuwaBulletShapeDraw.h"

#include "DynamicMeshBuilder.h"
#include "Game/NewWorld/Bullet/WuwaBulletTypes.h"
#include "PrimitiveDrawingUtils.h"
#include "SceneManagement.h"

namespace WuwaBulletDraw
{
	namespace
	{
		/** 判定体在自身空间里的线段和三角形（之后统一按 ShapeTransform 转到世界）。 */
		struct FShapeGeometry
		{
			TArray<TPair<FVector, FVector>> Lines;
			TArray<FVector> Vertices;
			TArray<uint32> Indices;

			void Line(const FVector& A, const FVector& B) { Lines.Emplace(A, B); }
			uint32 Vertex(const FVector& P) { return static_cast<uint32>(Vertices.Add(P)); }
			void Triangle(uint32 A, uint32 B, uint32 C) { Indices.Append({A, B, C}); }
			void Quad(uint32 A, uint32 B, uint32 C, uint32 D) { Triangle(A, B, C); Triangle(A, C, D); }
		};

		FVector Polar(double Radius, double Radians, double Z)
		{
			return FVector(Radius * FMath::Cos(Radians), Radius * FMath::Sin(Radians), Z);
		}

		void BuildBox(const FVector& Half, FShapeGeometry& Out)
		{
			FVector Corners[8];
			for (int32 I = 0; I < 8; ++I)
			{
				Corners[I] = FVector((I & 1) ? Half.X : -Half.X, (I & 2) ? Half.Y : -Half.Y, (I & 4) ? Half.Z : -Half.Z);
			}
			for (int32 I = 0; I < 8; ++I)
			{
				for (const int32 Bit : {1, 2, 4})
				{
					if (!(I & Bit)) Out.Line(Corners[I], Corners[I | Bit]);
				}
			}
			uint32 V[8];
			for (int32 I = 0; I < 8; ++I) V[I] = Out.Vertex(Corners[I]);
			Out.Quad(V[0], V[1], V[3], V[2]);
			Out.Quad(V[4], V[6], V[7], V[5]);
			Out.Quad(V[0], V[4], V[5], V[1]);
			Out.Quad(V[2], V[3], V[7], V[6]);
			Out.Quad(V[0], V[2], V[6], V[4]);
			Out.Quad(V[1], V[5], V[7], V[3]);
		}

		void BuildSphere(double Radius, FShapeGeometry& Out)
		{
			constexpr int32 Sides = 32;
			constexpr int32 Rings = 16;
			for (int32 S = 0; S < Sides; ++S)
			{
				const double A0 = 2.0 * PI * S / Sides, A1 = 2.0 * PI * (S + 1) / Sides;
				Out.Line(Polar(Radius, A0, 0.0), Polar(Radius, A1, 0.0));
				Out.Line(FVector(Radius * FMath::Cos(A0), 0.0, Radius * FMath::Sin(A0)), FVector(Radius * FMath::Cos(A1), 0.0, Radius * FMath::Sin(A1)));
				Out.Line(FVector(0.0, Radius * FMath::Cos(A0), Radius * FMath::Sin(A0)), FVector(0.0, Radius * FMath::Cos(A1), Radius * FMath::Sin(A1)));
			}
			TArray<uint32> Grid;
			for (int32 R = 0; R <= Rings; ++R)
			{
				const double Lat = PI * R / Rings - PI * 0.5;
				for (int32 S = 0; S <= Sides; ++S)
				{
					Grid.Add(Out.Vertex(Polar(Radius * FMath::Cos(Lat), 2.0 * PI * S / Sides, Radius * FMath::Sin(Lat))));
				}
			}
			for (int32 R = 0; R < Rings; ++R)
			{
				for (int32 S = 0; S < Sides; ++S)
				{
					const int32 A = R * (Sides + 1) + S;
					Out.Quad(Grid[A], Grid[A + 1], Grid[A + Sides + 2], Grid[A + Sides + 1]);
				}
			}
		}

		/** 环形柱 / 扇形柱的公共部分：角度区间 [A0, A1] 内，内外半径之间、上下半高之间的立体。 */
		void BuildSweep(double Outer, double Inner, double HalfHeight, double A0, double A1, bool bClosedSides, FShapeGeometry& Out)
		{
			const int32 Segments = FMath::Clamp(FMath::CeilToInt((A1 - A0) / FMath::DegreesToRadians(6.0)), 4, 64);
			auto Ring = [&](double Radius, double Z)
			{
				TArray<uint32> Ids;
				for (int32 I = 0; I <= Segments; ++I)
				{
					const double A = FMath::Lerp(A0, A1, double(I) / Segments);
					Ids.Add(Out.Vertex(Polar(Radius, A, Z)));
					if (I > 0) Out.Line(Polar(Radius, FMath::Lerp(A0, A1, double(I - 1) / Segments), Z), Polar(Radius, A, Z));
				}
				return Ids;
			};
			const TArray<uint32> OuterTop = Ring(Outer, HalfHeight), OuterBottom = Ring(Outer, -HalfHeight);
			TArray<uint32> InnerTop, InnerBottom;
			if (Inner > 0.0)
			{
				InnerTop = Ring(Inner, HalfHeight);
				InnerBottom = Ring(Inner, -HalfHeight);
			}
			else
			{
				// 实心：内圈退化成圆心
				const uint32 Top = Out.Vertex(FVector(0, 0, HalfHeight)), Bottom = Out.Vertex(FVector(0, 0, -HalfHeight));
				InnerTop.Init(Top, Segments + 1);
				InnerBottom.Init(Bottom, Segments + 1);
			}
			for (int32 I = 0; I < Segments; ++I)
			{
				Out.Quad(OuterBottom[I], OuterBottom[I + 1], OuterTop[I + 1], OuterTop[I]);
				Out.Quad(InnerTop[I], InnerTop[I + 1], OuterTop[I + 1], OuterTop[I]);
				Out.Quad(InnerBottom[I], OuterBottom[I], OuterBottom[I + 1], InnerBottom[I + 1]);
				if (Inner > 0.0) Out.Quad(InnerBottom[I], InnerTop[I], InnerTop[I + 1], InnerBottom[I + 1]);
			}
			// 竖线：外圈四个方向（或扇形两端），内圈同理
			const int32 Verticals = bClosedSides ? 1 : 4;
			for (int32 K = 0; K <= (bClosedSides ? 1 : Verticals - 1); ++K)
			{
				const double A = bClosedSides ? (K == 0 ? A0 : A1) : A0 + (A1 - A0) * K / Verticals;
				Out.Line(Polar(Outer, A, -HalfHeight), Polar(Outer, A, HalfHeight));
				if (Inner > 0.0) Out.Line(Polar(Inner, A, -HalfHeight), Polar(Inner, A, HalfHeight));
			}
			if (bClosedSides)
			{
				for (const double A : {A0, A1})
				{
					const FVector InTop = Inner > 0.0 ? Polar(Inner, A, HalfHeight) : FVector(0, 0, HalfHeight);
					const FVector InBottom = Inner > 0.0 ? Polar(Inner, A, -HalfHeight) : FVector(0, 0, -HalfHeight);
					Out.Line(InTop, Polar(Outer, A, HalfHeight));
					Out.Line(InBottom, Polar(Outer, A, -HalfHeight));
					const int32 Index = A == A0 ? 0 : Segments;
					Out.Quad(InnerBottom[Index], OuterBottom[Index], OuterTop[Index], InnerTop[Index]);
				}
				if (Inner <= 0.0) Out.Line(FVector(0, 0, -HalfHeight), FVector(0, 0, HalfHeight));
			}
		}

		bool Build(const FWuwaBulletInfo& Info, FShapeGeometry& Out)
		{
			const FVector& S = Info.Size;
			switch (Info.Shape)
			{
			case EWuwaBulletShape::Box:
				BuildBox(S.ComponentMax(FVector::ZeroVector), Out);
				return true;
			case EWuwaBulletShape::Sphere:
				if (S.X <= 0.0) return false;
				BuildSphere(S.X, Out);
				return true;
			case EWuwaBulletShape::Sector:
			{
				if (S.X <= 0.0) return false;
				const double Half = FMath::DegreesToRadians(FMath::Clamp(S.Y, 0.0, 360.0) * 0.5);
				BuildSweep(S.X, 0.0, FMath::Max(S.Z, 0.0), -Half, Half, Half < PI - KINDA_SMALL_NUMBER, Out);
				return true;
			}
			case EWuwaBulletShape::Cylinder:
				if (S.X <= 0.0) return false;
				BuildSweep(S.X, FMath::Clamp(S.Y, 0.0, S.X), FMath::Max(S.Z, 0.0), 0.0, 2.0 * PI, false, Out);
				return true;
			default:
				return false;
			}
		}
	}

	void DrawShape(FPrimitiveDrawInterface* PDI, ERHIFeatureLevel::Type FeatureLevel, const FWuwaBulletInfo& Info, const FTransform& ShapeTransform,
		const FLinearColor& Color, float Thickness, const FMaterialRenderProxy* Fill, float DepthBias)
	{
		FShapeGeometry Geometry;
		if (!Build(Info, Geometry)) return;
		for (const TPair<FVector, FVector>& Line : Geometry.Lines)
		{
			PDI->DrawLine(ShapeTransform.TransformPosition(Line.Key), ShapeTransform.TransformPosition(Line.Value), Color, SDPG_World, Thickness, DepthBias);
		}
		if (Fill && Geometry.Indices.Num() > 0)
		{
			FDynamicMeshBuilder Builder(FeatureLevel);
			for (const FVector& V : Geometry.Vertices)
			{
				Builder.AddVertex(FDynamicMeshVertex(FVector3f(V)));
			}
			for (int32 I = 0; I + 2 < Geometry.Indices.Num(); I += 3)
			{
				Builder.AddTriangle(Geometry.Indices[I], Geometry.Indices[I + 1], Geometry.Indices[I + 2]);
			}
			Builder.Draw(PDI, ShapeTransform.ToMatrixWithScale(), Fill, SDPG_World, /*bDisableBackfaceCulling*/ true);
		}
	}

	double GetBoundingRadius(const FWuwaBulletInfo& Info)
	{
		const FVector S = Info.Size.ComponentMax(FVector::ZeroVector);
		switch (Info.Shape)
		{
		case EWuwaBulletShape::Box: return S.Size();
		case EWuwaBulletShape::Sphere: return S.X;
		default: return FMath::Sqrt(S.X * S.X + S.Z * S.Z);
		}
	}

	FString DescribeSize(const FWuwaBulletInfo& Info)
	{
		const FVector& S = Info.Size;
		switch (Info.Shape)
		{
		case EWuwaBulletShape::Box: return FString::Printf(TEXT("方盒  半长 %.0f × %.0f × %.0f"), S.X, S.Y, S.Z);
		case EWuwaBulletShape::Sphere: return FString::Printf(TEXT("球  半径 %.0f"), S.X);
		case EWuwaBulletShape::Sector: return FString::Printf(TEXT("扇形  半径 %.0f  张角 %.0f°  半高 %.0f"), S.X, S.Y, S.Z);
		case EWuwaBulletShape::Cylinder:
			return S.Y > 0.0 ? FString::Printf(TEXT("环形柱  外半径 %.0f  内半径 %.0f  半高 %.0f"), S.X, S.Y, S.Z)
				: FString::Printf(TEXT("圆柱  半径 %.0f  半高 %.0f"), S.X, S.Z);
		default: return TEXT("?");
		}
	}
}
