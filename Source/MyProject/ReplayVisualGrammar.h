// Copyright enaDyne. M2 phase 3 visual grammar.
//
// The single C++ source of truth for the element palette and the two-level
// element law (visual-grammar.md, Law 3). These 8 pairs mirror MPC_Elements
// (/Game/M2/Palettes/MPC_Elements) exactly; the MPC is the material-side copy,
// this is the renderer-side copy used to push per-spawn Niagara colors.
//
// Header-only and engine-light (FLinearColor + FString) so it compiles trivially
// and can be unit-reasoned. It computes NO combat logic - it maps tokens to color.

#pragma once

#include "CoreMinimal.h"

/** One element's primary->secondary palette pair (linear color, matches MPC). */
struct FElementPalette
{
	FLinearColor Primary;
	FLinearColor Secondary;
};

namespace ReplayGrammar
{
	/** Lowercase an element token so payload "Fire" and grammar "fire" match (deviation D1). */
	inline FString NormalizeElement(const FString& In)
	{
		return In.ToLower();
	}

	/** The eight ratified palettes, linear-space, identical to MPC_Elements.
	 *  Unknown/none -> arcane-neutral (Law 3 bare-spell fallback). */
	inline FElementPalette PaletteFor(const FString& ElementRaw)
	{
		const FString E = NormalizeElement(ElementRaw);
		// sRGB hex sources are recorded in docs/m2-plan.md; values here are linear.
		if (E == TEXT("fire"))   return { FLinearColor(1.0f, 0.194618f, 0.010330f), FLinearColor(0.262251f, 0.010330f, 0.004025f) };
		if (E == TEXT("water"))  return { FLinearColor(0.006049f, 0.042311f, 0.262251f), FLinearColor(0.028426f, 0.571125f, 0.745404f) };
		if (E == TEXT("earth"))  return { FLinearColor(0.479320f, 0.238398f, 0.043735f), FLinearColor(0.102242f, 0.127438f, 0.162029f) };
		if (E == TEXT("air"))    return { FLinearColor(0.520996f, 0.863157f, 1.0f), FLinearColor(1.0f, 1.0f, 1.0f) };
		if (E == TEXT("light"))  return { FLinearColor(1.0f, 0.896269f, 0.672443f), FLinearColor(1.0f, 0.564712f, 0.068478f) };
		if (E == TEXT("shadow")) return { FLinearColor(0.144128f, 0.042311f, 0.351533f), FLinearColor(0.006049f, 0.003035f, 0.013702f) };
		if (E == TEXT("nature")) return { FLinearColor(0.078187f, 0.341914f, 0.042311f), FLinearColor(0.147027f, 0.198069f, 0.042311f) };
		// arcane and the neutral fallback share the arcane palette.
		return { FLinearColor(0.745404f, 0.051269f, 0.964686f), FLinearColor(0.198069f, 0.028426f, 0.520996f) };
	}

	/** Two-level element law (Law 3), computed per clause-effect.
	 *  @param ConceptElement  the spell header element ("" if bare).
	 *  @param ClauseElement   this event payload's element ("" if the event carries none).
	 *  @param HighestShareClauseElement  for a bare spell, the element of the highest-share
	 *         clause ("" if none anywhere -> arcane-neutral).
	 *  Rule: clause element tints when present; else inherit concept; a bare spell uses the
	 *  highest-share clause element; nothing anywhere -> arcane. */
	inline FElementPalette Resolve(const FString& ConceptElement,
	                               const FString& ClauseElement,
	                               const FString& HighestShareClauseElement)
	{
		if (!ClauseElement.IsEmpty())
		{
			return PaletteFor(ClauseElement);           // clause tint wins where present
		}
		if (!ConceptElement.IsEmpty())
		{
			return PaletteFor(ConceptElement);          // inherit the concept palette
		}
		if (!HighestShareClauseElement.IsEmpty())
		{
			return PaletteFor(HighestShareClauseElement); // bare spell: highest-share clause
		}
		return PaletteFor(TEXT("arcane"));              // none anywhere -> arcane-neutral
	}
}
