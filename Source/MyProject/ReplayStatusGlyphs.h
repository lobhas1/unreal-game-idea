// Copyright enaDyne. M2 phase 3 - status sigil glyph table.
//
// The full status vocabulary (visual-grammar.md, corpus or not) mapped to a text
// glyph + tint. Grammar allows "simple geometric or text glyphs ... legibility over
// beauty" for M2, so each status docks as a short tinted text sigil over the head.
// This maps tokens to a legible marker; it computes no combat logic.

#pragma once

#include "CoreMinimal.h"

struct FStatusGlyph
{
	FString Text;         // short docked label (the sigil)
	FLinearColor Tint;    // per-status colour so statuses read apart at a glance
	bool bRisingMotes = false; // regen: green motes rise on each tick (grammar v1.1)
};

namespace ReplayStatus
{
	/** Full grammar status table + the v1.1 rows (vulnerable, regen). Unknown statuses
	 *  fall back to their own name in neutral white, so nothing renders blank (G2). */
	inline FStatusGlyph GlyphFor(const FString& StatusRaw)
	{
		const FString S = StatusRaw.ToLower();
		// grammar motifs rendered as tinted text glyphs (icon shapes are the beauty round)
		if (S == TEXT("burn"))         return { TEXT("Burn"),    FLinearColor(1.0f, 0.35f, 0.05f) };
		if (S == TEXT("chill"))        return { TEXT("Chill"),   FLinearColor(0.4f, 0.8f, 1.0f) };
		if (S == TEXT("freeze"))       return { TEXT("Freeze"),  FLinearColor(0.6f, 0.85f, 1.0f) };
		if (S == TEXT("slow"))         return { TEXT("Slow"),    FLinearColor(0.55f, 0.55f, 0.6f) };
		if (S == TEXT("root"))         return { TEXT("Root"),    FLinearColor(0.5f, 0.35f, 0.2f) };
		if (S == TEXT("stun"))         return { TEXT("Stun"),    FLinearColor(1.0f, 0.9f, 0.2f) };
		if (S == TEXT("blind"))        return { TEXT("Blind"),   FLinearColor(0.25f, 0.25f, 0.3f) };
		if (S == TEXT("stealth"))      return { TEXT("Stealth"), FLinearColor(0.5f, 0.3f, 0.7f) };
		if (S == TEXT("haste"))        return { TEXT("Haste"),   FLinearColor(0.9f, 0.95f, 1.0f) };
		if (S == TEXT("silence"))      return { TEXT("Silence"), FLinearColor(0.6f, 0.3f, 0.6f) };
		if (S == TEXT("fear"))         return { TEXT("Fear"),    FLinearColor(0.45f, 0.15f, 0.6f) };
		if (S == TEXT("poison"))       return { TEXT("Poison"),  FLinearColor(0.3f, 0.8f, 0.2f) };
		if (S == TEXT("bleed"))        return { TEXT("Bleed"),   FLinearColor(0.8f, 0.05f, 0.05f) };
		if (S == TEXT("shock"))        return { TEXT("Shock"),   FLinearColor(0.4f, 0.7f, 1.0f) };
		if (S == TEXT("weaken"))       return { TEXT("Weaken"),  FLinearColor(0.5f, 0.5f, 0.45f) };
		if (S == TEXT("invulnerable")) return { TEXT("Invuln"),  FLinearColor(1.0f, 0.85f, 0.3f) };
		if (S == TEXT("unstoppable"))  return { TEXT("Unstop"),  FLinearColor(0.9f, 0.2f, 0.15f) };
		if (S == TEXT("disarm"))       return { TEXT("Disarm"),  FLinearColor(0.6f, 0.6f, 0.65f) };
		// v1.1 rows:
		if (S == TEXT("vulnerable"))   return { TEXT("[!]Vuln"), FLinearColor(0.95f, 0.15f, 0.15f) }; // cracked shield, red
		if (S == TEXT("regen"))        return { TEXT("[+]Regen"),FLinearColor(0.2f, 0.9f, 0.3f), true }; // green plus + motes
		// off-table corpus status or anything new: its own name, neutral, never blank.
		return { StatusRaw, FLinearColor(0.85f, 0.85f, 0.85f) };
	}
}
