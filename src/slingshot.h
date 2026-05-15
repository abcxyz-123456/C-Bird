#ifndef SLINGSHOT_H
#define SLINGSHOT_H

#include "raylib.h"
#include "entities.h"
#include "physics.h"

typedef struct {
    Vector2 basePos;
    Vector2 dragPos;
    bool isDragging;
    float maxDrag;
    BirdType activeBird;
} Slingshot;

void Slingshot_Init(Slingshot *s, Vector2 pos);
Entity* Slingshot_Update(Slingshot *s, EntityManager *em, PhysicsWorld *world, Vector2 mousePos);
void Slingshot_Draw(Slingshot *s);

#endif
