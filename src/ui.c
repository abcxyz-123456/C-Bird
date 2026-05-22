#include "ui.h"
#include "raymath.h"
#include <string.h>
#include <stdio.h>

static Vector2 gGuiMouse = { 0 };

void Gui_SetMousePosition(Vector2 mouse) {
    gGuiMouse = mouse;
}

Vector2 Gui_GetMousePosition(void) {
    return gGuiMouse;
}

bool Gui_IsClicked(Rectangle bounds) {
    return CheckCollisionPointRec(gGuiMouse, bounds) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

void Gui_DrawShadow(Rectangle bounds, float elevation) {
    if (elevation <= 0.0f) return;

    Color shadow = (Color){ 0, 0, 0, (unsigned char)Clamp(20.0f + elevation * 10.0f, 20.0f, 80.0f) };
    Rectangle s = { bounds.x + elevation, bounds.y + elevation * 1.4f, bounds.width, bounds.height };
    DrawRectangleRounded(s, 0.5f, 16, shadow);
}

void Gui_DrawButton(Rectangle bounds, const char *text, Color baseColor, Color textColor) {
    bool hovering = CheckCollisionPointRec(gGuiMouse, bounds);
    bool pressed = hovering && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    
    float elevation = hovering ? 4.0f : 1.0f;
    if (pressed) elevation = 0.0f;

    // Shadow
    Gui_DrawShadow(bounds, elevation);
    
    // Main Body
    Color drawColor = baseColor;
    if (hovering) drawColor = ColorAlphaBlend(baseColor, (Color){255, 255, 255, 40}, WHITE);
    
    DrawRectangleRounded(bounds, 0.5f, 16, drawColor);
    DrawRectangleRoundedLines(bounds, 0.5f, 16, ColorAlpha(MD_OUTLINE, hovering ? 120 : 70));
    
    // Text
    int fontSize = 20;
    while (fontSize > 12 && MeasureTextEx(gFont, text, (float)fontSize, 1.0f).x > bounds.width - 24) {
        fontSize--;
    }
    int textWidth = (int)MeasureTextEx(gFont, text, (float)fontSize, 1.0f).x;
    DrawTextEx(gFont, text, (Vector2){ bounds.x + (bounds.width - textWidth)/2, bounds.y + (bounds.height - fontSize)/2 }, (float)fontSize, 1.0f, textColor);
}

void Gui_DrawIconButton(Rectangle bounds, int icon, Color baseColor) {
    char iconText[16];
    snprintf(iconText, sizeof(iconText), "%d", icon);
    Gui_DrawButton(bounds, iconText, baseColor, MD_ON_PRIMARY);
}

void Gui_DrawCard(Rectangle bounds, float elevation) {
    Gui_DrawShadow(bounds, elevation);
    DrawRectangleRounded(bounds, 0.1f, 12, MD_SURFACE);
    DrawRectangleRoundedLines(bounds, 0.1f, 12, MD_OUTLINE);
}

void Gui_DrawLevelButton(Rectangle bounds, int levelNum, int stars, bool locked) {
    bool hovering = CheckCollisionPointRec(gGuiMouse, bounds);
    
    float elevation = hovering ? 6.0f : 2.0f;
    if (locked) elevation = 0.0f;

    // Shadow
    Gui_DrawShadow(bounds, elevation);

    // Background
    Color bgColor = locked ? MD_SURFACE_VARIANT : MD_PRIMARY_CONTAINER;
    if (hovering && !locked) bgColor = ColorAlphaBlend(bgColor, (Color){255, 255, 255, 60}, WHITE);
    
    DrawRectangleRounded(bounds, 0.2f, 16, bgColor);
    DrawRectangleRoundedLines(bounds, 0.2f, 16, ColorAlpha(MD_OUTLINE, hovering ? 120 : 70));
    
    if (locked) {
        // Draw Lock Icon (Simplified)
        DrawCircle(bounds.x + bounds.width/2, bounds.y + bounds.height/2, 10, MD_ON_SURFACE_VARIANT);
        DrawRectangle(bounds.x + bounds.width/2 - 10, bounds.y + bounds.height/2, 20, 15, MD_ON_SURFACE_VARIANT);
    } else {
        // Level Number
        char txt[10];
        sprintf(txt, "%d", levelNum);
        int fontSize = 32;
        int tw = (int)MeasureTextEx(gFont, txt, (float)fontSize, 1.0f).x;
        DrawTextEx(gFont, txt, (Vector2){ bounds.x + (bounds.width - tw)/2, bounds.y + 20 }, (float)fontSize, 1.0f, MD_ON_PRIMARY_CONTAINER);
        
        // Stars
        float starSize = 15;
        float spacing = 20;
        float startX = bounds.x + (bounds.width - (spacing * 2))/2;
        for (int i = 0; i < 3; i++) {
            Color starCol = (i < stars) ? GOLD : MD_OUTLINE;
            DrawPoly((Vector2){ startX + i*spacing, bounds.y + bounds.height - 25 }, 5, starSize, 0, starCol);
        }
    }
}
