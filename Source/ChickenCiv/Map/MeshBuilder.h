#pragma once

#include "CoreMinimal.h"

/**
 * Prosty budowniczy siatki z wypalonym cieniowaniem w kolorach wierzcholkow (material bez swiatel).
 * Wszystkie bryly sa wypukle, wiec orientacja scian jest wyznaczana wzgledem srodka bryly.
 * Uklad UE: X naprzod, Y w prawo, Z w gore; sciany "przednie" maja wierzcholki zgodnie z ruchem wskazowek zegara.
 */
struct FMeshBuilder
{
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;

	FVector LightDir = FVector(-0.35, -0.3, 0.88).GetSafeNormal();

	void Reset() { Verts.Reset(); Tris.Reset(); Normals.Reset(); UVs.Reset(); Colors.Reset(); }
	bool IsEmpty() const { return Verts.Num() == 0; }

	/** Trojkat; Center = punkt wewnatrz bryly (do ustalenia, ktora strona jest zewnetrzna). */
	void AddTri(FVector A, FVector B, FVector C, const FLinearColor& Color, const FVector& Center)
	{
		FVector N = FVector::CrossProduct(B - A, C - B);
		if (N.SizeSquared() < 1e-6) return;
		N.Normalize();
		if (FVector::DotProduct(N, (A + B + C) / 3.0 - Center) < 0.0)
		{
			Swap(B, C);
			N = -N;
		}
		const float Shade = 0.45f + 0.55f * FMath::Clamp((float)FVector::DotProduct(N, LightDir), 0.f, 1.f);
		const FLinearColor Col(Color.R * Shade, Color.G * Shade, Color.B * Shade, 1.f);
		const int32 Base = Verts.Num();
		Verts.Add(A); Verts.Add(B); Verts.Add(C);
		for (int32 i = 0; i < 3; ++i) { Normals.Add(N); UVs.Add(FVector2D::ZeroVector); Colors.Add(Col); }
		Tris.Add(Base); Tris.Add(Base + 1); Tris.Add(Base + 2);
	}

	void AddQuad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FLinearColor& Color, const FVector& Center)
	{
		AddTri(A, B, C, Color, Center);
		AddTri(A, C, D, Color, Center);
	}

	/** Prostopadloscian osiowy, obrocony o Yaw (stopnie) wokol osi Z srodka. */
	void AddBox(const FVector& Center, const FVector& Half, const FLinearColor& Color, float YawDeg = 0.f)
	{
		const FRotator Rot(0.f, YawDeg, 0.f);
		FVector P[8];
		for (int32 i = 0; i < 8; ++i)
		{
			const FVector Local((i & 1) ? Half.X : -Half.X, (i & 2) ? Half.Y : -Half.Y, (i & 4) ? Half.Z : -Half.Z);
			P[i] = Center + Rot.RotateVector(Local);
		}
		static const int32 Faces[6][4] = { { 0, 1, 3, 2 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 }, { 2, 3, 7, 6 }, { 0, 2, 6, 4 }, { 1, 3, 7, 5 } };
		for (const auto& F : Faces) AddQuad(P[F[0]], P[F[1]], P[F[2]], P[F[3]], Color, Center);
	}

	/** Elipsoida (kula, gdy Radii rowne); Rings x Segs podzialow. */
	void AddEllipsoid(const FVector& Center, const FVector& Radii, const FLinearColor& Color, int32 Rings = 5, int32 Segs = 9)
	{
		auto Pt = [&](int32 Ring, int32 Seg)
		{
			const float Phi = PI * Ring / Rings;          // 0..PI od gory do dolu
			const float Theta = 2.f * PI * Seg / Segs;
			return Center + FVector(Radii.X * FMath::Sin(Phi) * FMath::Cos(Theta), Radii.Y * FMath::Sin(Phi) * FMath::Sin(Theta), Radii.Z * FMath::Cos(Phi));
		};
		for (int32 R = 0; R < Rings; ++R)
			for (int32 S = 0; S < Segs; ++S)
			{
				const FVector A = Pt(R, S), B = Pt(R, S + 1), C = Pt(R + 1, S + 1), D = Pt(R + 1, S);
				if (R == 0) AddTri(A, C, D, Color, Center);
				else if (R == Rings - 1) AddTri(A, B, C, Color, Center);
				else AddQuad(A, B, C, D, Color, Center);
			}
	}

	void AddSphere(const FVector& Center, float Radius, const FLinearColor& Color, int32 Rings = 5, int32 Segs = 9)
	{
		AddEllipsoid(Center, FVector(Radius), Color, Rings, Segs);
	}

	/** Walec o osi Z; Base = srodek podstawy dolnej. */
	void AddCylinder(const FVector& Base, float Radius, float Height, const FLinearColor& Color, int32 Segs = 10)
	{
		const FVector Center = Base + FVector(0, 0, Height * 0.5f);
		const FVector Top = Base + FVector(0, 0, Height);
		for (int32 S = 0; S < Segs; ++S)
		{
			const float A0 = 2.f * PI * S / Segs, A1 = 2.f * PI * (S + 1) / Segs;
			const FVector O0(Radius * FMath::Cos(A0), Radius * FMath::Sin(A0), 0), O1(Radius * FMath::Cos(A1), Radius * FMath::Sin(A1), 0);
			AddQuad(Base + O0, Base + O1, Top + O1, Top + O0, Color, Center);
			AddTri(Base, Base + O0, Base + O1, Color, Center);
			AddTri(Top, Top + O0, Top + O1, Color, Center);
		}
	}

	/** Stozek o dowolnej osi: od srodka podstawy Base w kierunku Dir (dlugosc Length). */
	void AddCone(const FVector& Base, FVector Dir, float Radius, float Length, const FLinearColor& Color, int32 Segs = 8)
	{
		Dir = Dir.GetSafeNormal();
		const FVector Up = FMath::Abs(Dir.Z) > 0.95 ? FVector(1, 0, 0) : FVector(0, 0, 1);
		const FVector U = FVector::CrossProduct(Dir, Up).GetSafeNormal();
		const FVector V = FVector::CrossProduct(Dir, U).GetSafeNormal();
		const FVector Tip = Base + Dir * Length;
		const FVector Center = Base + Dir * (Length * 0.33f);
		for (int32 S = 0; S < Segs; ++S)
		{
			const float A0 = 2.f * PI * S / Segs, A1 = 2.f * PI * (S + 1) / Segs;
			const FVector P0 = Base + (U * FMath::Cos(A0) + V * FMath::Sin(A0)) * Radius;
			const FVector P1 = Base + (U * FMath::Cos(A1) + V * FMath::Sin(A1)) * Radius;
			AddTri(P0, P1, Tip, Color, Center);
			AddTri(Base, P0, P1, Color, Center);
		}
	}

	/** Cienki walec/pret miedzy dwoma punktami (kwadratowy przekroj dla prostoty). */
	void AddBar(const FVector& A, const FVector& B, float Thickness, const FLinearColor& Color)
	{
		const FVector Dir = (B - A);
		if (Dir.SizeSquared() < 1e-4) return;
		const FVector D = Dir.GetSafeNormal();
		const FVector Up = FMath::Abs(D.Z) > 0.95 ? FVector(1, 0, 0) : FVector(0, 0, 1);
		const FVector U = FVector::CrossProduct(D, Up).GetSafeNormal() * Thickness;
		const FVector V = FVector::CrossProduct(D, U).GetSafeNormal() * Thickness;
		const FVector Center = (A + B) * 0.5;
		const FVector C0[4] = { A + U + V, A - U + V, A - U - V, A + U - V };
		const FVector C1[4] = { B + U + V, B - U + V, B - U - V, B + U - V };
		for (int32 i = 0; i < 4; ++i)
		{
			const int32 j = (i + 1) % 4;
			AddQuad(C0[i], C0[j], C1[j], C1[i], Color, Center);
		}
		AddQuad(C0[0], C0[1], C0[2], C0[3], Color, Center);
		AddQuad(C1[0], C1[1], C1[2], C1[3], Color, Center);
	}
};
