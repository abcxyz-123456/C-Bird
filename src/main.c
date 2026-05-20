#include "raylib.h"
#include "raymath.h"
#include "physics.h"
#include "entities.h"
#include "slingshot.h"
#include "level.h"
#include "ui.h"
#include "cJSON.h"
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef enum { SCREEN_HOME, SCREEN_LEVEL_SELECT, SCREEN_GAMEPLAY } GameScreen;

#define TOTAL_LEVELS 25
#define LEVELS_PER_PAGE 6
#define PROGRESS_FILE "assets/progress.json"

typedef struct {
    char title[32];
    char subtitle[64];
    const char *path;
    Color accent;
    bool available;
} LevelEntry;

typedef struct {
    int unlockedLevels;
    int stars[TOTAL_LEVELS];
} LevelProgress;

static BirdType birdQueue[MAX_ENTITIES];
static int birdCount = 0;
static int currentBirdIdx = 0;
static Entity *flyingBird = NULL;
static bool levelWon = false;
static int selectedLevelIndex = 0;
static int currentLevelPage = 0;
static const char *currentLevelPath = NULL;
static int currentRunStars = 0;
static LevelEntry gLevels[TOTAL_LEVELS];
static LevelConfig levelPreviewConfigs[TOTAL_LEVELS];
static LevelProgress levelProgress;

static Color BirdTypeColor(BirdType type) {
    switch (type) {
        case BIRD_BLUE:
            return SKYBLUE;
        case BIRD_YELLOW:
            return YELLOW;
        case BIRD_BLACK:
            return BLACK;
        case BIRD_RED:
        default:
            return RED;
    }
}

static Color LevelAccentForIndex(int index) {
    static const Color palette[] = {
        { 103, 80, 164, 255 },
        { 54, 94, 157, 255 },
        { 46, 125, 98, 255 },
        { 181, 91, 68, 255 },
        { 214, 141, 53, 255 }
    };

    return palette[index % (int)(sizeof(palette) / sizeof(palette[0]))];
}

static int GetPageCount(void) {
    return (TOTAL_LEVELS + LEVELS_PER_PAGE - 1) / LEVELS_PER_PAGE;
}

static int GetTotalStarsEarned(void) {
    int total = 0;
    for (int i = 0; i < TOTAL_LEVELS; i++) {
        total += levelProgress.stars[i];
    }
    return total;
}

static void SyncLevelPageToSelection(void) {
    currentLevelPage = selectedLevelIndex / LEVELS_PER_PAGE;
    if (currentLevelPage < 0) currentLevelPage = 0;
    if (currentLevelPage >= GetPageCount()) currentLevelPage = GetPageCount() - 1;
}

static bool DrawActionButton(Rectangle bounds, const char *text, Color baseColor, Color textColor) {
    Gui_DrawButton(bounds, text, baseColor, textColor);
    return Gui_IsClicked(bounds);
}

static void DrawStarsRow(Vector2 center, int stars, int total, float spacing, float radius, Color filled, Color empty) {
    float totalWidth = (float)(total - 1) * spacing;
    float startX = center.x - totalWidth * 0.5f;

    for (int i = 0; i < total; i++) {
        Color c = (i < stars) ? filled : empty;
        DrawPoly((Vector2){ startX + i * spacing, center.y }, 5, radius, 0.0f, c);
    }
}

static int FitFontSize(const char *text, int preferred, int min, float maxWidth) {
    int fontSize = preferred;

    while (fontSize > min && MeasureText(text, fontSize) > maxWidth) {
        fontSize--;
    }

    return fontSize;
}

static void DrawTextLeftFitted(const char *text, Rectangle bounds, int preferred, int min, Color color) {
    int fontSize = FitFontSize(text, preferred, min, bounds.width);
    DrawText(text, (int)bounds.x, (int)(bounds.y + (bounds.height - fontSize) * 0.5f), fontSize, color);
}

static void DrawTextRightFitted(const char *text, Rectangle bounds, int preferred, int min, Color color) {
    int fontSize = FitFontSize(text, preferred, min, bounds.width);
    int textWidth = MeasureText(text, fontSize);
    DrawText(text, (int)(bounds.x + bounds.width - textWidth), (int)(bounds.y + (bounds.height - fontSize) * 0.5f), fontSize, color);
}

static void DrawTextCenteredFitted(const char *text, Rectangle bounds, int preferred, int min, Color color) {
    int fontSize = FitFontSize(text, preferred, min, bounds.width);
    int textWidth = MeasureText(text, fontSize);
    DrawText(text, (int)(bounds.x + (bounds.width - textWidth) * 0.5f), (int)(bounds.y + (bounds.height - fontSize) * 0.5f), fontSize, color);
}

static void DrawTextBlockFitted(Rectangle bounds, const char *const *lines, int lineCount, int preferred, int min, int spacing, Color color) {
    int fontSize = preferred;

    for (int i = 0; i < lineCount; i++) {
        int fitted = FitFontSize(lines[i], preferred, min, bounds.width);
        if (fitted < fontSize) {
            fontSize = fitted;
        }
    }

    int totalHeight = lineCount * fontSize + (lineCount - 1) * spacing;
    float y = bounds.y + (bounds.height - totalHeight) * 0.5f;

    for (int i = 0; i < lineCount; i++) {
        DrawText(lines[i], (int)bounds.x, (int)y, fontSize, color);
        y += fontSize + spacing;
    }
}

static void InitLevelEntries(void) {
    for (int i = 0; i < TOTAL_LEVELS; i++) {
        snprintf(gLevels[i].title, sizeof(gLevels[i].title), "Level %d", i + 1);
        snprintf(gLevels[i].subtitle, sizeof(gLevels[i].subtitle), "Coming soon");
        gLevels[i].path = NULL;
        gLevels[i].accent = LevelAccentForIndex(i);
        gLevels[i].available = false;
        memset(&levelPreviewConfigs[i], 0, sizeof(LevelConfig));
    }

    snprintf(gLevels[0].title, sizeof(gLevels[0].title), "Level 1");
    snprintf(gLevels[0].subtitle, sizeof(gLevels[0].subtitle), "Classic starter layout");
    gLevels[0].path = "assets/level1.json";
    gLevels[0].available = true;

    snprintf(gLevels[1].title, sizeof(gLevels[1].title), "Level 2");
    snprintf(gLevels[1].subtitle, sizeof(gLevels[1].subtitle), "Compact custom challenge");
    gLevels[1].path = "assets/custom_level.json";
    gLevels[1].available = true;
}

static void ResetProgressDefaults(void) {
    memset(&levelProgress, 0, sizeof(levelProgress));
    levelProgress.unlockedLevels = 1;
}

static void SaveProgress(void) {
    FILE *fp = fopen(PROGRESS_FILE, "w");
    if (!fp) return;

    fprintf(fp, "{\n");
    fprintf(fp, "  \"unlockedLevels\": %d,\n", levelProgress.unlockedLevels);
    fprintf(fp, "  \"stars\": [");
    for (int i = 0; i < TOTAL_LEVELS; i++) {
        fprintf(fp, "%s%d", (i == 0) ? "" : ", ", levelProgress.stars[i]);
    }
    fprintf(fp, "]\n");
    fprintf(fp, "}\n");

    fclose(fp);
}

static void LoadProgress(void) {
    ResetProgressDefaults();

    char *data = LoadFileText(PROGRESS_FILE);
    if (!data) return;

    cJSON *root = cJSON_Parse(data);
    if (root) {
        cJSON *unlocked = cJSON_GetObjectItem(root, "unlockedLevels");
        if (cJSON_IsNumber(unlocked)) {
            levelProgress.unlockedLevels = unlocked->valueint;
        }

        cJSON *stars = cJSON_GetObjectItem(root, "stars");
        if (cJSON_IsArray(stars)) {
            int count = cJSON_GetArraySize(stars);
            for (int i = 0; i < TOTAL_LEVELS && i < count; i++) {
                cJSON *item = cJSON_GetArrayItem(stars, i);
                if (cJSON_IsNumber(item)) {
                    levelProgress.stars[i] = item->valueint;
                }
            }
        }

        cJSON_Delete(root);
    }

    UnloadFileText(data);

    if (levelProgress.unlockedLevels < 1) levelProgress.unlockedLevels = 1;
    if (levelProgress.unlockedLevels > TOTAL_LEVELS) levelProgress.unlockedLevels = TOTAL_LEVELS;
    for (int i = 0; i < TOTAL_LEVELS; i++) {
        if (levelProgress.stars[i] < 0) levelProgress.stars[i] = 0;
        if (levelProgress.stars[i] > 3) levelProgress.stars[i] = 3;
    }
}

static bool IsLevelLocked(int levelIndex) {
    if (levelIndex < 0 || levelIndex >= TOTAL_LEVELS) return true;
    return !gLevels[levelIndex].available || levelIndex >= levelProgress.unlockedLevels;
}

static void EnsureSelectedLevelIsUnlocked(void) {
    if (!IsLevelLocked(selectedLevelIndex)) {
        SyncLevelPageToSelection();
        return;
    }

    for (int i = levelProgress.unlockedLevels - 1; i >= 0; i--) {
        if (!IsLevelLocked(i)) {
            selectedLevelIndex = i;
            SyncLevelPageToSelection();
            return;
        }
    }

    selectedLevelIndex = 0;
    SyncLevelPageToSelection();
}

static void LoadLevelPreviews(void) {
    for (int i = 0; i < TOTAL_LEVELS; i++) {
        if (gLevels[i].available && gLevels[i].path) {
            Level_Load(gLevels[i].path, &levelPreviewConfigs[i]);
        } else {
            levelPreviewConfigs[i].castleCount = 0;
        }
    }
}

static int CalculateStarsForWin(void) {
    if (birdCount <= 0) return 1;

    int birdsRemaining = birdCount - currentBirdIdx;
    float ratio = (float)birdsRemaining / (float)birdCount;

    if (ratio >= 0.50f) return 3;
    if (ratio >= 0.20f) return 2;
    return 1;
}

static void CommitLevelWin(int levelIndex) {
    currentRunStars = CalculateStarsForWin();
    if (currentRunStars > levelProgress.stars[levelIndex]) {
        levelProgress.stars[levelIndex] = currentRunStars;
    }

    int unlockCount = levelIndex + 2;
    if (unlockCount > levelProgress.unlockedLevels) {
        levelProgress.unlockedLevels = unlockCount;
    }
    if (levelProgress.unlockedLevels > TOTAL_LEVELS) {
        levelProgress.unlockedLevels = TOTAL_LEVELS;
    }

    SaveProgress();
}

static bool QueueBird(BirdType type) {
    if (birdCount >= MAX_ENTITIES) {
        return false;
    }

    birdQueue[birdCount++] = type;
    return true;
}

static void ShuffleBirds(LevelConfig *cfg) {
    birdCount = 0;
    currentBirdIdx = 0;

    for (int i = 0; i < cfg->redBirds; i++) {
        if (!QueueBird(BIRD_RED)) break;
    }
    for (int i = 0; i < cfg->blueBirds; i++) {
        if (!QueueBird(BIRD_BLUE)) break;
    }
    for (int i = 0; i < cfg->yellowBirds; i++) {
        if (!QueueBird(BIRD_YELLOW)) break;
    }
    for (int i = 0; i < cfg->blackBirds; i++) {
        if (!QueueBird(BIRD_BLACK)) break;
    }

    for (int i = 0; i < birdCount; i++) {
        int r = GetRandomValue(0, birdCount - 1);
        BirdType temp = birdQueue[i];
        birdQueue[i] = birdQueue[r];
        birdQueue[r] = temp;
    }
}

static void UnloadLevel(PhysicsWorld **pw, EntityManager **em) {
    if (*pw) {
        Physics_Cleanup(*pw);
        *pw = NULL;
    }
    if (*em) {
        Entities_Cleanup(*em);
        *em = NULL;
    }

    flyingBird = NULL;
    levelWon = false;
    currentRunStars = 0;
}

static void ResetLevel(PhysicsWorld **pw, EntityManager **em, Slingshot *sl, const char *levelPath) {
    UnloadLevel(pw, em);

    *pw = Physics_Init();
    *em = Entities_Init();

    LevelConfig cfg;
    Level_Load(levelPath, &cfg);
    ShuffleBirds(&cfg);

    Slingshot_Init(sl, (Vector2){ 200, 660 });
    if (birdCount > 0) {
        sl->activeBird = birdQueue[0];
    }

    Level_GenerateScene(*pw, *em, &cfg);
}

static void StartSelectedLevel(GameScreen *screen, bool *levelLoaded) {
    EnsureSelectedLevelIsUnlocked();
    if (IsLevelLocked(selectedLevelIndex) || gLevels[selectedLevelIndex].path == NULL) {
        return;
    }

    currentLevelPath = gLevels[selectedLevelIndex].path;
    *screen = SCREEN_GAMEPLAY;
    *levelLoaded = false;
}

static void DrawBackdrop(int width, int height, bool gameplay) {
    Color top = gameplay ? (Color){ 122, 190, 246, 255 } : (Color){ 245, 248, 255, 255 };
    Color bottom = gameplay ? (Color){ 91, 171, 235, 255 } : (Color){ 205, 228, 255, 255 };

    DrawRectangleGradientV(0, 0, width, height, top, bottom);

    DrawCircle(1020, 120, 170, ColorAlpha(MD_PRIMARY, gameplay ? 0.08f : 0.18f));
    DrawCircle(160, 90, 120, ColorAlpha((Color){ 255, 193, 92, 255 }, gameplay ? 0.05f : 0.14f));
    DrawCircle(110, 720, 210, ColorAlpha(MD_SECONDARY, gameplay ? 0.06f : 0.12f));

    DrawEllipse(180, 160, 80, 24, ColorAlpha(WHITE, 0.60f));
    DrawEllipse(240, 150, 68, 20, ColorAlpha(WHITE, 0.55f));
    DrawEllipse(910, 190, 92, 26, ColorAlpha(WHITE, 0.50f));
    DrawEllipse(980, 180, 74, 21, ColorAlpha(WHITE, 0.45f));
}

static void DrawHomeScreen(int width, int height, GameScreen *screen, bool *exitRequested) {
    Rectangle hero = { 80, 90, width - 160, 560 };
    Rectangle titlePanel = { hero.x + 42, hero.y + 54, 500, 180 };
    Rectangle insight = { hero.x + hero.width - 300, hero.y + 52, 240, 238 };
    Rectangle copyPanel = { hero.x + 58, hero.y + 266, 560, 124 };
    Rectangle primaryButton = { hero.x + 56, hero.y + 432, 240, 58 };
    Rectangle secondaryButton = { hero.x + 322, hero.y + 432, 170, 58 };
    const char *copyLines[] = {
        "A clean mouse-first menu and gentler physics",
        "support a 25-stage campaign without the old",
        "keyboard-only flow or crowded level browser."
    };

    Gui_DrawCard(hero, 7.0f);
    DrawRectangleRounded(titlePanel, 0.14f, 16, ColorAlpha(MD_PRIMARY_CONTAINER, 0.95f));
    DrawRectangleRounded(copyPanel, 0.08f, 12, ColorAlpha(WHITE, 0.28f));

    DrawTextLeftFitted("ANGRY BIRDS", (Rectangle){ hero.x + 72, hero.y + 86, 430, 60 }, 58, 42, MD_ON_SURFACE);
    DrawTextLeftFitted("ORIGINAL EXPERIENCE", (Rectangle){ hero.x + 72, hero.y + 150, 430, 40 }, 34, 22, MD_PRIMARY);
    DrawTextBlockFitted((Rectangle){ copyPanel.x + 18, copyPanel.y + 10, copyPanel.width - 36, copyPanel.height - 20 },
                        copyLines, 3, 22, 16, 8, MD_ON_SURFACE_VARIANT);

    Gui_DrawCard(insight, 4.0f);
    DrawTextLeftFitted("PROGRESS", (Rectangle){ insight.x + 26, insight.y + 22, insight.width - 52, 26 }, 20, 16, MD_PRIMARY);
    DrawTextLeftFitted(TextFormat("%d / %d levels unlocked", levelProgress.unlockedLevels, TOTAL_LEVELS),
                       (Rectangle){ insight.x + 26, insight.y + 68, insight.width - 52, 28 }, 22, 14, MD_ON_SURFACE);
    DrawTextLeftFitted(TextFormat("%d stars collected", GetTotalStarsEarned()),
                       (Rectangle){ insight.x + 26, insight.y + 118, insight.width - 52, 28 }, 22, 14, MD_ON_SURFACE);
    DrawTextLeftFitted(TextFormat("%d pages of levels", GetPageCount()),
                       (Rectangle){ insight.x + 26, insight.y + 168, insight.width - 52, 24 }, 18, 13, MD_ON_SURFACE_VARIANT);

    if (DrawActionButton(primaryButton, "Select Level", MD_PRIMARY, MD_ON_PRIMARY)) {
        *screen = SCREEN_LEVEL_SELECT;
    }
    if (DrawActionButton(secondaryButton, "Quit", MD_SECONDARY_CONTAINER, MD_ON_SECONDARY_CONTAINER)) {
        *exitRequested = true;
    }
}

static void DrawLevelPreview(Rectangle bounds, const LevelConfig *cfg, Color accent, bool locked, bool available) {
    Rectangle preview = { bounds.x, bounds.y, bounds.width, bounds.height };
    Color skyTop = ColorAlpha(accent, 0.20f);
    Color skyBottom = ColorAlpha((Color){ 255, 255, 255, 255 }, 0.35f);

    DrawRectangleRounded(preview, 0.20f, 12, skyBottom);
    DrawRectangleGradientV((int)preview.x, (int)preview.y, (int)preview.width, (int)preview.height, skyTop, skyBottom);

    if (available && cfg->castleCount > 0) {
        for (int i = 0; i < cfg->castleCount; i++) {
            float islandWidth = (cfg->castleCount == 1) ? 88.0f : 52.0f;
            float gap = preview.width / (cfg->castleCount + 1);
            float islandX = preview.x + gap * (i + 1) - islandWidth * 0.5f;
            float islandY = preview.y + preview.height - 14 - (i % 2) * 4;

            DrawRectangleRounded((Rectangle){ islandX, islandY, islandWidth, 18 }, 0.35f, 10, (Color){ 136, 93, 53, 255 });
            DrawRectangleRounded((Rectangle){ islandX, islandY - 6, islandWidth, 8 }, 0.35f, 10, (Color){ 104, 166, 66, 255 });

            int floors = cfg->floors[i];
            if (floors <= 0) floors = 1;
            if (floors > 3) floors = 3;

            for (int f = 0; f < floors; f++) {
                float supportY = islandY - f * 17;
                DrawRectangleRounded((Rectangle){ islandX + islandWidth * 0.24f, supportY - 13, 7, 13 }, 0.2f, 6, DARKBROWN);
                DrawRectangleRounded((Rectangle){ islandX + islandWidth * 0.68f, supportY - 13, 7, 13 }, 0.2f, 6, DARKBROWN);
                DrawRectangleRounded((Rectangle){ islandX + islandWidth * 0.18f, supportY - 17, islandWidth * 0.64f, 5 }, 0.2f, 6, (Color){ 151, 111, 71, 255 });
                DrawCircle((int)(islandX + islandWidth * 0.50f), (int)(supportY - 8), 5, LIME);
            }
        }
    } else {
        DrawCircle((int)preview.x + 42, (int)preview.y + 34, 10, ColorAlpha(accent, 0.28f));
        DrawCircle((int)preview.x + 76, (int)preview.y + 26, 7, ColorAlpha(accent, 0.20f));
        DrawCircle((int)preview.x + 104, (int)preview.y + 40, 12, ColorAlpha(accent, 0.14f));
        DrawText("Soon", (int)preview.x + 138, (int)preview.y + 24, 18, ColorAlpha(MD_ON_SURFACE_VARIANT, 0.9f));
    }

    if (locked) {
        DrawRectangleRounded(preview, 0.20f, 12, ColorAlpha(MD_SURFACE, 0.78f));
        DrawCircle((int)(preview.x + preview.width * 0.5f), (int)(preview.y + preview.height * 0.5f - 6), 12, MD_ON_SURFACE_VARIANT);
        DrawRectangle((int)(preview.x + preview.width * 0.5f - 12), (int)(preview.y + preview.height * 0.5f - 4), 24, 18, MD_ON_SURFACE_VARIANT);
    }
}

static void DrawLevelCard(Rectangle bounds, const LevelEntry *entry, const LevelConfig *cfg, int levelIndex, bool selected, bool locked) {
    bool hovering = CheckCollisionPointRec(Gui_GetMousePosition(), bounds);
    float elevation = selected ? 8.0f : (hovering ? 5.0f : 2.0f);
    const float padding = 18.0f;
    Rectangle preview = { bounds.x + padding, bounds.y + 24, bounds.width - padding * 2.0f, 82.0f };
    int statusFontSize = 14;
    int footerY = (int)(bounds.y + bounds.height - 36.0f);

    Gui_DrawCard(bounds, elevation);
    DrawRectangleRounded((Rectangle){ bounds.x, bounds.y, bounds.width, 16 }, 0.12f, 16, locked ? MD_SURFACE_VARIANT : entry->accent);
    if (selected) {
        DrawRectangleRoundedLines(bounds, 0.12f, 16, entry->accent);
    }

    DrawLevelPreview(preview, cfg, entry->accent, locked, entry->available);

    DrawTextLeftFitted(TextFormat("Level %d", levelIndex + 1),
                       (Rectangle){ bounds.x + 18, bounds.y + 114, bounds.width - 36, 18 }, 15, 12,
                       locked ? MD_ON_SURFACE_VARIANT : entry->accent);
    DrawTextLeftFitted(entry->title, (Rectangle){ bounds.x + 18, bounds.y + 134, bounds.width - 36, 28 }, 24, 16, MD_ON_SURFACE);
    DrawTextLeftFitted(entry->subtitle, (Rectangle){ bounds.x + 18, bounds.y + 164, bounds.width - 36, 20 }, 16, 12, MD_ON_SURFACE_VARIANT);

    if (!entry->available) {
        DrawTextLeftFitted("Placeholder slot for future content",
                           (Rectangle){ bounds.x + 18, (float)footerY, bounds.width - 36, 16 }, statusFontSize, 11, MD_ON_SURFACE_VARIANT);
    } else if (locked) {
        DrawTextLeftFitted("Beat the previous level to unlock",
                           (Rectangle){ bounds.x + 18, (float)footerY, bounds.width - 36, 16 }, statusFontSize, 11, MD_ON_SURFACE_VARIANT);
    } else {
        const char *selectionLabel = selected ? "Selected" : "Tap to select";

        DrawTextLeftFitted("Best rating", (Rectangle){ bounds.x + 18, (float)footerY, 96, 16 }, statusFontSize, 11, MD_ON_SURFACE_VARIANT);
        DrawStarsRow((Vector2){ bounds.x + 64, bounds.y + bounds.height - 14.0f }, levelProgress.stars[levelIndex], 3, 18.0f, 7.0f,
                     GOLD, ColorAlpha(MD_OUTLINE, 0.45f));
        DrawTextRightFitted(selectionLabel, (Rectangle){ bounds.x + 126, (float)footerY, bounds.width - 144, 16 }, statusFontSize, 11,
                            selected ? entry->accent : MD_ON_SURFACE_VARIANT);
    }
}

static void DrawLevelSelectScreen(int width, int height, GameScreen *screen, bool *levelLoaded) {
    Rectangle header = { 72, 34, width - 144, 92 };
    Rectangle footer = { 72, height - 108, width - 144, 76 };
    Rectangle backButton = { footer.x + 16, footer.y + 16, 112, 44 };
    Rectangle prevButton = { footer.x + 572, footer.y + 16, 118, 44 };
    Rectangle nextButton = { footer.x + 702, footer.y + 16, 118, 44 };
    Rectangle startButton = { footer.x + footer.width - 188, footer.y + 11, 172, 54 };
    Rectangle selectionInfo = { footer.x + 146, footer.y + 10, 406, 56 };
    int pageCount = GetPageCount();
    int startIndex = currentLevelPage * LEVELS_PER_PAGE;
    int endIndex = startIndex + LEVELS_PER_PAGE;
    bool selectedLocked = IsLevelLocked(selectedLevelIndex);
    float cardGapX = 22.0f;
    float cardGapY = 22.0f;
    float cardWidth = (width - 144.0f - cardGapX * 2.0f) / 3.0f;
    float cardHeight = 246.0f;

    if (endIndex > TOTAL_LEVELS) endIndex = TOTAL_LEVELS;

    Gui_DrawCard(header, 5.0f);
    DrawTextLeftFitted("SELECT A LEVEL", (Rectangle){ header.x + 28, header.y + 18, 280, 30 }, 30, 22, MD_ON_SURFACE);
    DrawTextLeftFitted(TextFormat("Page %d / %d", currentLevelPage + 1, pageCount),
                       (Rectangle){ header.x + 28, header.y + 52, 120, 20 }, 18, 14, MD_PRIMARY);
    DrawTextRightFitted(TextFormat("%d / %d unlocked", levelProgress.unlockedLevels, TOTAL_LEVELS),
                        (Rectangle){ header.x + header.width - 220, header.y + 22, 190, 20 }, 18, 13, MD_ON_SURFACE_VARIANT);
    DrawTextLeftFitted("Preview six stages at a time, then launch directly into play.",
                       (Rectangle){ header.x + 180, header.y + 52, header.width - 420, 20 }, 18, 12, MD_ON_SURFACE_VARIANT);

    for (int local = 0; local < LEVELS_PER_PAGE; local++) {
        int levelIndex = startIndex + local;
        if (levelIndex >= TOTAL_LEVELS) break;

        int col = local % 3;
        int row = local / 3;
        Rectangle card = {
            72.0f + col * (cardWidth + cardGapX),
            148.0f + row * (cardHeight + cardGapY),
            cardWidth,
            cardHeight
        };
        bool locked = IsLevelLocked(levelIndex);

        DrawLevelCard(card, &gLevels[levelIndex], &levelPreviewConfigs[levelIndex], levelIndex, levelIndex == selectedLevelIndex, locked);
        if (!locked && Gui_IsClicked(card)) {
            selectedLevelIndex = levelIndex;
        }
    }

    Gui_DrawCard(footer, 3.0f);
    Gui_DrawCard(selectionInfo, 1.0f);
    DrawTextLeftFitted(gLevels[selectedLevelIndex].title,
                       (Rectangle){ selectionInfo.x + 16, selectionInfo.y + 8, selectionInfo.width - 114, 24 }, 22, 15, MD_ON_SURFACE);
    DrawTextLeftFitted(selectedLocked ? "Locked or unavailable" : "Ready to launch",
                       (Rectangle){ selectionInfo.x + 16, selectionInfo.y + 30, selectionInfo.width - 114, 16 }, 14, 11, MD_ON_SURFACE_VARIANT);
    if (!selectedLocked) {
        DrawStarsRow((Vector2){ selectionInfo.x + selectionInfo.width - 58.0f, selectionInfo.y + 28.0f },
                     levelProgress.stars[selectedLevelIndex], 3, 16.0f, 6.0f, GOLD, ColorAlpha(MD_OUTLINE, 0.45f));
    }

    if (DrawActionButton(backButton, "Back", MD_SECONDARY_CONTAINER, MD_ON_SECONDARY_CONTAINER)) {
        *screen = SCREEN_HOME;
    }

    if (currentLevelPage > 0 && DrawActionButton(prevButton, "Previous", MD_SURFACE, MD_ON_SURFACE)) {
        currentLevelPage--;
    } else {
        Gui_DrawButton(prevButton, "Previous", MD_SURFACE_VARIANT, MD_ON_SURFACE_VARIANT);
    }

    if (currentLevelPage + 1 < pageCount && DrawActionButton(nextButton, "Next", MD_SURFACE, MD_ON_SURFACE)) {
        currentLevelPage++;
    } else {
        Gui_DrawButton(nextButton, "Next", MD_SURFACE_VARIANT, MD_ON_SURFACE_VARIANT);
    }

    Gui_DrawButton(startButton,
                   selectedLocked ? "Locked" : "Play Selected",
                   selectedLocked ? MD_SURFACE_VARIANT : MD_PRIMARY,
                   selectedLocked ? MD_ON_SURFACE_VARIANT : MD_ON_PRIMARY);
    if (!selectedLocked && Gui_IsClicked(startButton)) {
        StartSelectedLevel(screen, levelLoaded);
    }
}

static void DrawGameplayTopBar(int width, GameScreen *screen, bool *levelLoaded, PhysicsWorld **physics, EntityManager **entities) {
    Rectangle titleCard = { width * 0.5f - 260, 14, 520, 72 };
    Rectangle levelsButton = { 20, 18, 150, 50 };
    Rectangle restartButton = { 182, 18, 150, 50 };

    if (DrawActionButton(levelsButton, "Levels", MD_SURFACE, MD_ON_SURFACE)) {
        UnloadLevel(physics, entities);
        *levelLoaded = false;
        *screen = SCREEN_LEVEL_SELECT;
    }
    if (DrawActionButton(restartButton, "Restart", MD_PRIMARY_CONTAINER, MD_ON_PRIMARY_CONTAINER)) {
        *levelLoaded = false;
    }

    Gui_DrawCard(titleCard, 3.0f);
    DrawTextLeftFitted(gLevels[selectedLevelIndex].title,
                       (Rectangle){ titleCard.x + 22, titleCard.y + 10, 200, 24 }, 22, 15, MD_ON_SURFACE);
    DrawTextRightFitted(TextFormat("Best stars: %d", levelProgress.stars[selectedLevelIndex]),
                        (Rectangle){ titleCard.x + 250, titleCard.y + 10, 240, 24 }, 18, 13, MD_PRIMARY);
    DrawTextLeftFitted("Reduced gravity. Click flying birds to trigger abilities.",
                       (Rectangle){ titleCard.x + 22, titleCard.y + 40, titleCard.width - 44, 18 }, 14, 10, MD_ON_SURFACE_VARIANT);
}

static void DrawGameplayOverlay(int width, int height, bool outOfBirds, GameScreen *screen, bool *levelLoaded,
                                PhysicsWorld **physics, EntityManager **entities) {
    if (!levelWon && !outOfBirds) return;

    Rectangle overlay = { width * 0.5f - 230, height * 0.5f - 148, 460, 296 };
    Rectangle levelsButton = { overlay.x + 30, overlay.y + 196, 150, 52 };
    Rectangle replayButton = { overlay.x + 260, overlay.y + 196, 150, 52 };
    const char *winLines[] = {
        "Your best rating has been updated",
        "on the level select page."
    };
    const char *loseLines[] = {
        "No birds left. Replay or head back",
        "to level select."
    };

    DrawRectangle(0, 0, width, height, ColorAlpha(BLACK, 0.18f));
    Gui_DrawCard(overlay, 8.0f);
    DrawTextLeftFitted(levelWon ? "LEVEL CLEARED" : "TRY AGAIN",
                       (Rectangle){ overlay.x + 40, overlay.y + 30, overlay.width - 80, 36 }, 34, 24,
                       levelWon ? DARKGREEN : MAROON);
    DrawTextBlockFitted((Rectangle){ overlay.x + 40, overlay.y + 84, overlay.width - 80, 60 },
                        levelWon ? winLines : loseLines, 2, 20, 14, 6, MD_ON_SURFACE_VARIANT);

    if (levelWon) {
        DrawTextLeftFitted("Stars earned", (Rectangle){ overlay.x + 40, overlay.y + 134, 140, 22 }, 18, 14, MD_PRIMARY);
        DrawStarsRow((Vector2){ overlay.x + overlay.width * 0.5f, overlay.y + 176 }, currentRunStars, 3, 34.0f, 13.0f,
                     GOLD, ColorAlpha(MD_OUTLINE, 0.45f));
    }

    if (DrawActionButton(levelsButton, "Levels", MD_SECONDARY_CONTAINER, MD_ON_SECONDARY_CONTAINER)) {
        UnloadLevel(physics, entities);
        *levelLoaded = false;
        *screen = SCREEN_LEVEL_SELECT;
    }
    if (DrawActionButton(replayButton, "Replay", MD_PRIMARY, MD_ON_PRIMARY)) {
        *levelLoaded = false;
    }
}

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1600, 900, "Angry Birds - Original Experience");
    SetTargetFPS(60);

    InitLevelEntries();
    LoadLevelPreviews();
    LoadProgress();
    
    EnsureSelectedLevelIsUnlocked();


    const int virtualWidth = 1200;
    const int virtualHeight = 800;
    RenderTexture2D target = LoadRenderTexture(virtualWidth, virtualHeight);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    GameScreen screen = SCREEN_HOME;
    bool levelLoaded = false;
    bool exitRequested = false;
    PhysicsWorld *physics = NULL;
    EntityManager *entities = NULL;
    Slingshot slingshot = { 0 };

    while (!exitRequested && !WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.016f;

        float scale = fminf((float)GetScreenWidth()/virtualWidth, (float)GetScreenHeight()/virtualHeight);
        Vector2 offset = {
            (GetScreenWidth() - ((float)virtualWidth * scale)) * 0.5f,
            (GetScreenHeight() - ((float)virtualHeight * scale)) * 0.5f
        };
        Vector2 mouse = GetMousePosition();
        Vector2 vMouse = { (mouse.x - offset.x) / scale, (mouse.y - offset.y) / scale };
        Gui_SetMousePosition(vMouse);

        if (screen == SCREEN_HOME) {
            if (IsKeyPressed(KEY_ENTER)) {
                screen = SCREEN_LEVEL_SELECT;
            }
        } else if (screen == SCREEN_LEVEL_SELECT) {
            EnsureSelectedLevelIsUnlocked();
            if (IsKeyPressed(KEY_ESCAPE)) {
                screen = SCREEN_HOME;
            }
        } else if (screen == SCREEN_GAMEPLAY) {
            if (!levelLoaded) {
                ResetLevel(&physics, &entities, &slingshot, currentLevelPath);
                levelLoaded = true;
            }

            Physics_Update(entities, dt);

            if (entities && Entities_CountActive(entities, ENTITY_PIG) == 0 && !levelWon) {
                levelWon = true;
                CommitLevelWin(selectedLevelIndex);
            }

            if (!levelWon && currentBirdIdx < birdCount) {
                slingshot.activeBird = birdQueue[currentBirdIdx];
                Entity *newBird = Slingshot_Update(&slingshot, entities, physics, vMouse);
                if (newBird) {
                    flyingBird = newBird;
                    currentBirdIdx++;
                }
            }

            if (flyingBird && flyingBird->active && !flyingBird->abilityUsed) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    bool clickedSlingshot = CheckCollisionPointCircle(vMouse, slingshot.basePos, 50.0f);
                    if (!clickedSlingshot && !slingshot.isDragging) {
                        flyingBird->abilityUsed = true;
                        switch (flyingBird->subType.birdType) {
                            case BIRD_BLUE: {
                                Vector2 v1 = Vector2Rotate(flyingBird->vel, 15 * DEG2RAD);
                                Vector2 v2 = Vector2Rotate(flyingBird->vel, -15 * DEG2RAD);
                                Entity *b1 = Entities_AddBird(entities, flyingBird->pos, BIRD_BLUE);
                                Entity *b2 = Entities_AddBird(entities, flyingBird->pos, BIRD_BLUE);
                                if (b1) {
                                    b1->vel = v1;
                                    b1->abilityUsed = true;
                                }
                                if (b2) {
                                    b2->vel = v2;
                                    b2->abilityUsed = true;
                                }
                                break;
                            }
                            case BIRD_YELLOW:
                                flyingBird->vel = Vector2Scale(flyingBird->vel, 2.5f);
                                break;
                            case BIRD_BLACK:
                                Physics_ApplyExplosion(entities, flyingBird->pos, 150.0f, 600.0f);
                                flyingBird->active = false;
                                break;
                            default:
                                break;
                        }
                    }
                }
            }
        }

        BeginTextureMode(target);
            DrawBackdrop(virtualWidth, virtualHeight, screen == SCREEN_GAMEPLAY);

            if (screen == SCREEN_HOME) {
                DrawHomeScreen(virtualWidth, virtualHeight, &screen, &exitRequested);
            } else if (screen == SCREEN_LEVEL_SELECT) {
                DrawLevelSelectScreen(virtualWidth, virtualHeight, &screen, &levelLoaded);
            } else {
                bool outOfBirds = currentBirdIdx >= birdCount && (!flyingBird || !flyingBird->active);

                if (entities) Entities_Draw(entities);
                Slingshot_Draw(&slingshot);

                if (!levelWon &&
                    currentBirdIdx < birdCount &&
                    !slingshot.isDragging) {
                    Color c = RED;
                    float radius = 16.0f;
                    switch (birdQueue[currentBirdIdx]) {
                        case BIRD_BLUE:   c = SKYBLUE; radius = 10.0f; break;
                        case BIRD_YELLOW: c = YELLOW;  radius = 16.0f; break;
                        case BIRD_BLACK:  c = BLACK;   radius = 20.0f; break;
                        case BIRD_RED:
                        default:          c = RED;     radius = 16.0f; break;
                    }
                    DrawCircleV(slingshot.basePos, radius, c);
                    if (birdQueue[currentBirdIdx] == BIRD_BLACK) {
                        DrawCircleV(Vector2Add(slingshot.basePos, (Vector2){ 0, -10 }), 5, YELLOW);
                    }
                }

                for (int i = currentBirdIdx + 1; i < birdCount; i++) {
                    Color c = BirdTypeColor(birdQueue[i]);
                    DrawCircle(100 - (i - currentBirdIdx) * 30, 705, 12, c);
                }

                DrawGameplayTopBar(virtualWidth, &screen, &levelLoaded, &physics, &entities);
                DrawGameplayOverlay(virtualWidth, virtualHeight, outOfBirds, &screen, &levelLoaded, &physics, &entities);
            }
        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK);
            DrawTexturePro(
                target.texture,
                (Rectangle){ 0.0f, 0.0f, (float)target.texture.width, (float)-target.texture.height },
                (Rectangle){ offset.x, offset.y, (float)virtualWidth * scale, (float)virtualHeight * scale },
                (Vector2){ 0, 0 },
                0.0f,
                WHITE
            );
        EndDrawing();
    }

    UnloadRenderTexture(target);
    UnloadLevel(&physics, &entities);
    CloseWindow();
    return 0;
}
