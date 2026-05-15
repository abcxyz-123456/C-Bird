#ifndef EDITOR_H
#define EDITOR_H

#include "entities.h"
#include "physics.h"
#include "level.h"

typedef struct {
    bool active;
    bool restartRequested;
    EntityType selectedType;
    BlockType selectedBlock;
} Editor;

void Editor_Init(Editor *ed);
void Editor_Update(Editor *ed, EntityManager *em, PhysicsWorld *world);
void Editor_Draw(Editor *ed);
void Editor_Save(EntityManager *em, const char *filename);

#endif
