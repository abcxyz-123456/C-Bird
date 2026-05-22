#ifndef UI_H
#define UI_H

#include "raylib.h"

extern Font gFont;

// Material Design 3 Palette (Tonal Palettes)
#define MD_PRIMARY (Color){ 103, 80, 164, 255 }
#define MD_ON_PRIMARY (Color){ 255, 255, 255, 255 }
#define MD_PRIMARY_CONTAINER (Color){ 234, 221, 255, 255 }
#define MD_ON_PRIMARY_CONTAINER (Color){ 33, 0, 93, 255 }

#define MD_SECONDARY (Color){ 98, 91, 113, 255 }
#define MD_ON_SECONDARY (Color){ 255, 255, 255, 255 }
#define MD_SECONDARY_CONTAINER (Color){ 232, 222, 248, 255 }
#define MD_ON_SECONDARY_CONTAINER (Color){ 30, 25, 43, 255 }

#define MD_SURFACE (Color){ 254, 247, 255, 255 }
#define MD_ON_SURFACE (Color){ 29, 27, 32, 255 }
#define MD_SURFACE_VARIANT (Color){ 231, 224, 236, 255 }
#define MD_ON_SURFACE_VARIANT (Color){ 73, 69, 79, 255 }

#define MD_OUTLINE (Color){ 121, 116, 126, 255 }
#define MD_SHADOW (Color){ 0, 0, 0, 40 }

// Legacy names for compatibility if needed, but we'll use MD_ names
#define UI_COLOR_PANEL MD_SURFACE_VARIANT
#define UI_COLOR_TEXT MD_ON_SURFACE

bool Gui_IsClicked(Rectangle bounds);
void Gui_SetMousePosition(Vector2 mouse);
Vector2 Gui_GetMousePosition(void);
void Gui_DrawButton(Rectangle bounds, const char *text, Color baseColor, Color textColor);
void Gui_DrawIconButton(Rectangle bounds, int icon, Color baseColor);
void Gui_DrawCard(Rectangle bounds, float elevation);
void Gui_DrawLevelButton(Rectangle bounds, int levelNum, int stars, bool locked);

#endif
