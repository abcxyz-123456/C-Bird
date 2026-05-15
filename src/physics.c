#include "physics.h"
#include "entities.h"
#include "raymath.h"
#include <stdlib.h>

PhysicsWorld* Physics_Init(void) {
    PhysicsWorld *w = (PhysicsWorld*)malloc(sizeof(PhysicsWorld));
    w->gravity = 1000.0f;
    w->timeStep = 1.0f/60.0f;
    w->iterations = 8;
    return w;
}

void Physics_Update(void *em, float dt) {
    if (em && dt > 0) {
        Entities_Update((EntityManager*)em, dt);
    }
}

void Physics_ApplyExplosion(void *em_void, Vector2 epicenter, float radius, float force) {
    EntityManager *em = (EntityManager*)em_void;
    for (int i = 0; i < em->count; i++) {
        Entity *e = &em->entities[i];
        if (!e->active || e->type == ENTITY_PLATFORM) continue;
        
        float dist = Vector2Distance(epicenter, e->pos);
        if (dist < radius) {
            float strength = (1.0f - dist/radius) * force;
            Vector2 dir = (dist > 0.1f) ? Vector2Normalize(Vector2Subtract(e->pos, epicenter)) : (Vector2){0, -1};
            e->vel = Vector2Add(e->vel, Vector2Scale(dir, strength * e->invMass * 5.0f));
            e->angVel += (e->pos.x - epicenter.x) * 0.1f;
        }
    }
}

void Physics_Cleanup(PhysicsWorld *world) {
    if (world) free(world);
}

Vector2 PhysicsToScreen(Vector2 pos) { return pos; }
Vector2 ScreenToPhysics(Vector2 pos) { return pos; }
