#include "level.h"
#include "cJSON.h"
#include <string.h>

static BlockType GetRandomMaterial(LevelConfig *cfg) {
    int total = cfg->woodBlocks + cfg->stoneBlocks + cfg->glassBlocks;
    if (total <= 0) return BLOCK_WOOD;

    int r = GetRandomValue(1, total);
    if (r <= cfg->woodBlocks) {
        cfg->woodBlocks--;
        return BLOCK_WOOD;
    }
    if (r <= cfg->woodBlocks + cfg->stoneBlocks) {
        cfg->stoneBlocks--;
        return BLOCK_STONE;
    }

    cfg->glassBlocks--;
    return BLOCK_GLASS;
}

void Level_Load(const char *filename, LevelConfig *config) {
    memset(config, 0, sizeof(LevelConfig));

    char *data = LoadFileText(filename);
    if (!data) return;

    cJSON *root = cJSON_Parse(data);
    if (root) {
        cJSON *birds = cJSON_GetObjectItem(root, "birds");
        if (birds) {
            cJSON *red = cJSON_GetObjectItem(birds, "red");
            cJSON *blue = cJSON_GetObjectItem(birds, "blue");
            cJSON *yellow = cJSON_GetObjectItem(birds, "yellow");
            cJSON *black = cJSON_GetObjectItem(birds, "black");

            if (red) config->redBirds = red->valueint;
            if (blue) config->blueBirds = blue->valueint;
            if (yellow) config->yellowBirds = yellow->valueint;
            if (black) config->blackBirds = black->valueint;
        }

        cJSON *pigs = cJSON_GetObjectItem(root, "pigs");
        if (pigs) config->pigCount = pigs->valueint;

        cJSON *castle = cJSON_GetObjectItem(root, "castle");
        if (castle) config->castleCount = castle->valueint;

        cJSON *floors = cJSON_GetObjectItem(root, "floor");
        if (floors && cJSON_IsArray(floors)) {
            for (int i = 0; i < cJSON_GetArraySize(floors) && i < 10; i++) {
                config->floors[i] = cJSON_GetArrayItem(floors, i)->valueint;
            }
        }

        cJSON *blocks = cJSON_GetObjectItem(root, "blocks");
        if (blocks) {
            cJSON *wood = cJSON_GetObjectItem(blocks, "wood");
            cJSON *stone = cJSON_GetObjectItem(blocks, "stone");
            cJSON *glass = cJSON_GetObjectItem(blocks, "glass");

            if (wood) config->woodBlocks = wood->valueint;
            if (stone) config->stoneBlocks = stone->valueint;
            if (glass) config->glassBlocks = glass->valueint;
        }

        cJSON_Delete(root);
    }

    UnloadFileText(data);
}

static void BuildCastle(EntityManager *em, float x, float groundY, int floors, LevelConfig *cfg) {
    const float columnWidth = 15.0f;
    const float columnHeight = 80.0f;
    const float beamWidth = 100.0f;
    const float beamHeight = 16.0f;
    const float pigRadius = 18.0f;
    const float pigPadding = 2.0f;
    const float columnOffsetX = 35.0f;
    float supportTopY = groundY;

    for (int f = 0; f < floors; f++) {
        BlockType m1 = GetRandomMaterial(cfg);
        BlockType m2 = GetRandomMaterial(cfg);
        BlockType m3 = GetRandomMaterial(cfg);
        float columnCenterY = supportTopY - columnHeight * 0.5f;
        float beamCenterY = supportTopY - columnHeight - beamHeight * 0.5f;
        float pigCenterY = supportTopY - pigRadius - pigPadding;

        Entities_AddBlock(em, (Vector2){ x - columnOffsetX, columnCenterY }, (Vector2){ columnWidth, columnHeight }, m1, 0.0f);
        Entities_AddBlock(em, (Vector2){ x + columnOffsetX, columnCenterY }, (Vector2){ columnWidth, columnHeight }, m2, 0.0f);
        Entities_AddBlock(em, (Vector2){ x, beamCenterY }, (Vector2){ beamWidth, beamHeight }, m3, 0.0f);
        Entities_AddPig(em, (Vector2){ x, pigCenterY });

        supportTopY = beamCenterY - beamHeight * 0.5f;
    }
}

void Level_GenerateScene(PhysicsWorld *pw, EntityManager *em, LevelConfig *config) {
    (void)pw;
    const float platformWidth = 200.0f;
    const float platformHeight = 100.0f;
    const float platformBottomY = 800.0f;
    const float platformCenterY = platformBottomY - platformHeight * 0.5f;
    const float platformTopY = platformBottomY - platformHeight;

    Entities_AddPlatform(em, (Vector2){ 200, 750 }, (Vector2){ 250, 100 });

    float firstCastleX = 600.0f;
    for (int i = 0; i < config->castleCount; i++) {
        float cx = firstCastleX + i * 250.0f;
        Entities_AddPlatform(em, (Vector2){ cx, platformCenterY }, (Vector2){ platformWidth, platformHeight });
        BuildCastle(em, cx, platformTopY, config->floors[i], config);
    }
}
