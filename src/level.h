#ifndef LEVEL_H
#define LEVEL_H

#include "entities.h"
#include "physics.h"

typedef struct {
    int redBirds;
    int blueBirds;
    int yellowBirds;
    int blackBirds;
    int pigCount;
    int castleCount;
    int floors[10];
    int woodBlocks;
    int stoneBlocks;
    int glassBlocks;
} LevelConfig;

void Level_Load(const char *filename, LevelConfig *config);
void Level_GenerateScene(PhysicsWorld *world, EntityManager *em, LevelConfig *config);

#endif
