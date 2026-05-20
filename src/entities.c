#include "entities.h"
#include "raymath.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define GRAVITY 720.0f
#define ANGULAR_DAMPING 0.98f

static float Cross2D(Vector2 a, Vector2 b) {
    return a.x * b.y - a.y * b.x;
}

static Vector2 CrossScalarVector(float s, Vector2 v) {
    return (Vector2){ -s * v.y, s * v.x };
}

static float GetInverseInertia(const Entity *e) {
    if (e->invMass <= 0.0f) return 0.0f;

    if (e->type == ENTITY_BLOCK) {
        float w = e->size.x;
        float h = e->size.y;
        float inertia = e->mass * (w * w + h * h) / 12.0f;
        return (inertia > 0.0f) ? 1.0f / inertia : 0.0f;
    }

    float inertia = 0.5f * e->mass * e->radius * e->radius;
    return (inertia > 0.0f) ? 1.0f / inertia : 0.0f;
}

static Vector2 GetVelocityAtPoint(const Entity *e, Vector2 point) {
    Vector2 r = Vector2Subtract(point, e->pos);
    return Vector2Add(e->vel, CrossScalarVector(e->angVel, r));
}

static void ApplyImpulseAtPoint(Entity *e, Vector2 impulse, Vector2 point, float angularScale) {
    if (e->invMass <= 0.0f) return;

    Vector2 r = Vector2Subtract(point, e->pos);
    e->vel = Vector2Add(e->vel, Vector2Scale(impulse, e->invMass));
    e->angVel += Cross2D(r, impulse) * GetInverseInertia(e) * angularScale;
}

static Vector2 GetHalfExtents(const Entity *e) {
    if (e->type == ENTITY_PLATFORM) {
        return Vector2Scale(e->size, 0.5f);
    }
    if (e->type == ENTITY_BLOCK) {
        Vector2 half = Vector2Scale(e->size, 0.5f);
        float ca = fabsf(cosf(e->angle));
        float sa = fabsf(sinf(e->angle));
        return (Vector2){
            ca * half.x + sa * half.y,
            sa * half.x + ca * half.y
        };
    }

    return (Vector2){ e->radius, e->radius };
}

static Rectangle GetBounds(const Entity *e) {
    Vector2 half = GetHalfExtents(e);
    return (Rectangle){ e->pos.x - half.x, e->pos.y - half.y, half.x * 2.0f, half.y * 2.0f };
}

static bool IsRectEntity(const Entity *e) {
    return e->type == ENTITY_BLOCK || e->type == ENTITY_PLATFORM;
}

static float GetDamageMultiplier(const Entity *a, const Entity *b) {
    if (a->type == ENTITY_BIRD && a->subType.birdType == BIRD_BLUE &&
        b->type == ENTITY_BLOCK && b->subType.blockType == BLOCK_GLASS) {
        return 6.0f;
    }
    if (b->type == ENTITY_BIRD && b->subType.birdType == BIRD_BLUE &&
        a->type == ENTITY_BLOCK && a->subType.blockType == BLOCK_GLASS) {
        return 6.0f;
    }

    return 1.0f;
}

static void ApplyDamageFromImpulse(Entity *a, Entity *b, float impulseMagnitude) {
    if (impulseMagnitude <= 40.0f) return;

    float damage = (impulseMagnitude / 15.0f) * GetDamageMultiplier(a, b);
    a->health -= damage;
    b->health -= damage;
}

static void ApplyDamageFromCollision(Entity *a, Entity *b, float normalImpulse, float tangentImpulse) {
    float impulseMagnitude = fabsf(normalImpulse) + fabsf(tangentImpulse) * 0.35f;
    ApplyDamageFromImpulse(a, b, impulseMagnitude);
}

static void ApplyCollisionImpulse(Entity *a, Entity *b, Vector2 normal, float penetration, Vector2 contactPoint) {
    float totalInvMass = a->invMass + b->invMass;
    if (totalInvMass <= 0.0f) return;

    a->pos = Vector2Subtract(a->pos, Vector2Scale(normal, penetration * (a->invMass / totalInvMass) * 0.8f));
    b->pos = Vector2Add(b->pos, Vector2Scale(normal, penetration * (b->invMass / totalInvMass) * 0.8f));

    float invInertiaA = GetInverseInertia(a);
    float invInertiaB = GetInverseInertia(b);
    Vector2 ra = Vector2Subtract(contactPoint, a->pos);
    Vector2 rb = Vector2Subtract(contactPoint, b->pos);
    Vector2 velA = GetVelocityAtPoint(a, contactPoint);
    Vector2 velB = GetVelocityAtPoint(b, contactPoint);
    Vector2 rv = Vector2Subtract(velB, velA);
    float velAlongNormal = Vector2DotProduct(rv, normal);
    if (velAlongNormal > 0.0f) return;

    bool restingSupport = fabsf(normal.y) > 0.75f &&
                          fabsf(velAlongNormal) < 38.0f &&
                          penetration < 6.0f &&
                          a->type != ENTITY_BIRD &&
                          b->type != ENTITY_BIRD;
    float angularScale = restingSupport ? 0.0f : 1.0f;

    float restitution = restingSupport ? 0.0f : fminf(a->restitution, b->restitution);
    
    // Apply stabilization bias to correct ALL penetrations via velocity impulse.
    // Decoupling this from restingSupport is CRITICAL so it catches synchronous free-falls 
    // before velocities exceed the resting state threshold.
    float bias = 0.0f;
    if (penetration > 0.02f) {
        bias = 120.0f * (penetration - 0.02f); // Robust corrective force
    }

    float raCrossN = Cross2D(ra, normal);
    float rbCrossN = Cross2D(rb, normal);
    float normalDenom = totalInvMass + raCrossN * raCrossN * invInertiaA + rbCrossN * rbCrossN * invInertiaB;
    if (normalDenom <= 0.0f) return;

    float normalImpulseMag = -( (1.0f + restitution) * velAlongNormal - bias ) / normalDenom;
    // Use genuine velocity impulse WITHOUT stabilizing bias to calculate structural damage.
    // This prevents the stabilization system from destroying buildings automatically.
    float realNormalImpulseMag = -( (1.0f + restitution) * velAlongNormal ) / normalDenom; 
    
    Vector2 normalImpulse = Vector2Scale(normal, normalImpulseMag);

    ApplyImpulseAtPoint(a, Vector2Negate(normalImpulse), contactPoint, angularScale);
    ApplyImpulseAtPoint(b, normalImpulse, contactPoint, angularScale);

    velA = GetVelocityAtPoint(a, contactPoint);
    velB = GetVelocityAtPoint(b, contactPoint);
    rv = Vector2Subtract(velB, velA);

    Vector2 tangent = Vector2Subtract(rv, Vector2Scale(normal, Vector2DotProduct(rv, normal)));
    float tangentLen = Vector2Length(tangent);
    float tangentImpulseMag = 0.0f;

    if (tangentLen > 0.001f) {
        tangent = Vector2Scale(tangent, 1.0f / tangentLen);
        float raCrossT = Cross2D(ra, tangent);
        float rbCrossT = Cross2D(rb, tangent);
        float tangentDenom = totalInvMass + raCrossT * raCrossT * invInertiaA + rbCrossT * rbCrossT * invInertiaB;

        if (tangentDenom > 0.0f) {
            float jt = -Vector2DotProduct(rv, tangent) / tangentDenom;
            float friction = sqrtf(fmaxf(a->friction, 0.0f) * fmaxf(b->friction, 0.0f));
            float maxFriction = fabsf(normalImpulseMag) * friction;
            tangentImpulseMag = Clamp(jt, -maxFriction, maxFriction);
            Vector2 tangentImpulse = Vector2Scale(tangent, tangentImpulseMag);

            ApplyImpulseAtPoint(a, Vector2Negate(tangentImpulse), contactPoint, angularScale);
            ApplyImpulseAtPoint(b, tangentImpulse, contactPoint, angularScale);
        }
    }

    ApplyDamageFromCollision(a, b, realNormalImpulseMag, tangentImpulseMag);
}

EntityManager* Entities_Init(void) {
    return (EntityManager*)calloc(1, sizeof(EntityManager));
}

static Entity* Alloc(EntityManager *em) {
    if (em->count >= MAX_ENTITIES) return NULL;

    Entity *e = &em->entities[em->count++];
    memset(e, 0, sizeof(Entity));
    e->active = true;
    return e;
}

Entity* Entities_AddBird(EntityManager *em, Vector2 pos, BirdType type) {
    Entity *e = Alloc(em);
    if (!e) return NULL;

    e->type = ENTITY_BIRD;
    e->pos = pos;
    e->subType.birdType = type;
    e->mass = 2.5f;
    e->invMass = 1.0f/e->mass;
    e->radius = 16.0f;
    e->restitution = 0.5f;
    e->friction = 0.4f;
    e->health = 2000.0f;

    switch (type) {
        case BIRD_RED:
            e->color = RED;
            break;
        case BIRD_BLUE:
            e->color = SKYBLUE;
            e->radius = 10.0f;
            e->mass = 1.0f;
            e->invMass = 1.0f;
            break;
        case BIRD_YELLOW:
            e->color = YELLOW;
            break;
        case BIRD_BLACK:
            e->color = BLACK;
            e->radius = 20.0f;
            e->mass = 5.0f;
            e->invMass = 0.2f;
            break;
    }

    return e;
}

Entity* Entities_AddPig(EntityManager *em, Vector2 pos) {
    Entity *e = Alloc(em);
    if (!e) return NULL;

    e->type = ENTITY_PIG;
    e->pos = pos;
    e->health = 40.0f;
    e->mass = 1.5f;
    e->invMass = 1.0f/e->mass;
    e->radius = 18.0f;
    e->restitution = 0.3f;
    e->friction = 0.5f;
    e->color = LIME;
    return e;
}

Entity* Entities_AddBlock(EntityManager *em, Vector2 pos, Vector2 size, BlockType type, float angle) {
    Entity *e = Alloc(em);
    if (!e) return NULL;

    e->type = ENTITY_BLOCK;
    e->pos = pos;
    e->size = size;
    e->angle = angle;
    e->subType.blockType = type;

    switch (type) {
        case BLOCK_STONE:
            e->mass = 15.0f;
            e->color = GRAY;
            e->health = 250.0f;
            e->friction = 0.65f;
            break;
        case BLOCK_WOOD:
            e->mass = 5.0f;
            e->color = DARKBROWN;
            e->health = 80.0f;
            e->friction = 0.68f;
            break;
        case BLOCK_GLASS:
            e->mass = 3.2f;
            e->color = (Color){ 150, 230, 255, 180 };
            e->health = 25.0f;
            e->friction = 0.48f;
            break;
    }

    e->invMass = 1.0f/e->mass;
    e->radius = Vector2Length(Vector2Scale(size, 0.5f));
    e->restitution = 0.1f;

    return e;
}

Entity* Entities_AddPlatform(EntityManager *em, Vector2 pos, Vector2 size) {
    Entity *e = Alloc(em);
    if (!e) return NULL;

    e->type = ENTITY_PLATFORM;
    e->pos = pos;
    e->size = size;
    e->mass = 0.0f;
    e->invMass = 0.0f;
    e->friction = 0.92f;
    e->color = DARKGRAY;
    return e;
}

static void ResolvePlatformCollision(Entity *platform, Entity *obj) {
    Rectangle platBounds = GetBounds(platform);
    Rectangle objBounds = GetBounds(obj);

    if (!CheckCollisionRecs(platBounds, objBounds)) return;

    float overlapLeft = (objBounds.x + objBounds.width) - platBounds.x;
    float overlapRight = (platBounds.x + platBounds.width) - objBounds.x;
    float overlapTop = (objBounds.y + objBounds.height) - platBounds.y;
    float overlapBottom = (platBounds.y + platBounds.height) - objBounds.y;

    float minOverlap = overlapTop;
    Vector2 normal = { 0.0f, -1.0f };

    if (overlapBottom < minOverlap) {
        minOverlap = overlapBottom;
        normal = (Vector2){ 0.0f, 1.0f };
    }
    if (overlapLeft < minOverlap) {
        minOverlap = overlapLeft;
        normal = (Vector2){ -1.0f, 0.0f };
    }
    if (overlapRight < minOverlap) {
        minOverlap = overlapRight;
        normal = (Vector2){ 1.0f, 0.0f };
    }

    Vector2 contactPoint;
    if (fabsf(normal.x) > 0.5f) {
        float top = fmaxf(platBounds.y, objBounds.y);
        float bottom = fminf(platBounds.y + platBounds.height, objBounds.y + objBounds.height);
        contactPoint = (Vector2){
            (normal.x > 0.0f) ? platBounds.x + platBounds.width : platBounds.x,
            (top + bottom) * 0.5f
        };
    } else {
        float left = fmaxf(platBounds.x, objBounds.x);
        float right = fminf(platBounds.x + platBounds.width, objBounds.x + objBounds.width);
        contactPoint = (Vector2){
            (left + right) * 0.5f,
            (normal.y < 0.0f) ? platBounds.y : platBounds.y + platBounds.height
        };
    }

    ApplyCollisionImpulse(platform, obj, normal, minOverlap, contactPoint);
}

static void ResolveRectRectCollision(Entity *a, Entity *b) {
    Rectangle boundsA = GetBounds(a);
    Rectangle boundsB = GetBounds(b);
    if (!CheckCollisionRecs(boundsA, boundsB)) return;

    float overlapLeft = (boundsA.x + boundsA.width) - boundsB.x;
    float overlapRight = (boundsB.x + boundsB.width) - boundsA.x;
    float overlapTop = (boundsA.y + boundsA.height) - boundsB.y;
    float overlapBottom = (boundsB.y + boundsB.height) - boundsA.y;

    float penetration = overlapLeft;
    Vector2 normal = { 1.0f, 0.0f };

    if (overlapRight < penetration) {
        penetration = overlapRight;
        normal = (Vector2){ -1.0f, 0.0f };
    }
    if (overlapTop < penetration) {
        penetration = overlapTop;
        normal = (Vector2){ 0.0f, 1.0f };
    }
    if (overlapBottom < penetration) {
        penetration = overlapBottom;
        normal = (Vector2){ 0.0f, -1.0f };
    }

    Vector2 contactPoint;
    if (fabsf(normal.x) > 0.5f) {
        float top = fmaxf(boundsA.y, boundsB.y);
        float bottom = fminf(boundsA.y + boundsA.height, boundsB.y + boundsB.height);
        float faceX = (normal.x > 0.0f)
            ? ((boundsA.x + boundsA.width) + boundsB.x) * 0.5f
            : (boundsA.x + (boundsB.x + boundsB.width)) * 0.5f;
        contactPoint = (Vector2){ faceX, (top + bottom) * 0.5f };
    } else {
        float left = fmaxf(boundsA.x, boundsB.x);
        float right = fminf(boundsA.x + boundsA.width, boundsB.x + boundsB.width);
        float faceY = (normal.y > 0.0f)
            ? ((boundsA.y + boundsA.height) + boundsB.y) * 0.5f
            : (boundsA.y + (boundsB.y + boundsB.height)) * 0.5f;
        contactPoint = (Vector2){ (left + right) * 0.5f, faceY };
    }

    ApplyCollisionImpulse(a, b, normal, penetration, contactPoint);
}

static void ResolveCircleCircleCollision(Entity *a, Entity *b) {
    Vector2 delta = Vector2Subtract(b->pos, a->pos);
    float dist = Vector2Length(delta);
    float minD = a->radius + b->radius;

    if (dist >= minD || dist <= 0.001f) return;

    Vector2 normal = Vector2Normalize(delta);
    Vector2 contactPoint = Vector2Add(a->pos, Vector2Scale(normal, a->radius));
    ApplyCollisionImpulse(a, b, normal, minD - dist, contactPoint);
}

static void ResolveCircleRectCollision(Entity *circle, Entity *rect) {
    Rectangle rectBounds = GetBounds(rect);
    Vector2 closest = {
        Clamp(circle->pos.x, rectBounds.x, rectBounds.x + rectBounds.width),
        Clamp(circle->pos.y, rectBounds.y, rectBounds.y + rectBounds.height)
    };

    Vector2 delta = Vector2Subtract(circle->pos, closest);
    float dist = Vector2Length(delta);
    if (dist >= circle->radius) return;

    Vector2 normal;
    float penetration;

    if (dist > 0.001f) {
        normal = Vector2Normalize(delta);
        penetration = circle->radius - dist;
    } else {
        float left = fabsf(circle->pos.x - rectBounds.x);
        float right = fabsf((rectBounds.x + rectBounds.width) - circle->pos.x);
        float top = fabsf(circle->pos.y - rectBounds.y);
        float bottom = fabsf((rectBounds.y + rectBounds.height) - circle->pos.y);

        penetration = left;
        normal = (Vector2){ -1.0f, 0.0f };

        if (right < penetration) {
            penetration = right;
            normal = (Vector2){ 1.0f, 0.0f };
        }
        if (top < penetration) {
            penetration = top;
            normal = (Vector2){ 0.0f, -1.0f };
        }
        if (bottom < penetration) {
            penetration = bottom;
            normal = (Vector2){ 0.0f, 1.0f };
        }
    }

    ApplyCollisionImpulse(rect, circle, normal, penetration, closest);
}

static void ResolveDynamicCollision(Entity *a, Entity *b) {
    if (IsRectEntity(a) && IsRectEntity(b)) {
        ResolveRectRectCollision(a, b);
        return;
    }

    if (IsRectEntity(a) && !IsRectEntity(b)) {
        ResolveCircleRectCollision(b, a);
        return;
    }

    if (!IsRectEntity(a) && IsRectEntity(b)) {
        ResolveCircleRectCollision(a, b);
        return;
    }

    ResolveCircleCircleCollision(a, b);
}

static void ResolveCollision(Entity *a, Entity *b) {
    if (a->type == ENTITY_BIRD && b->type == ENTITY_BIRD) return;

    if (a->type == ENTITY_PLATFORM || b->type == ENTITY_PLATFORM) {
        Entity *platform = (a->type == ENTITY_PLATFORM) ? a : b;
        Entity *obj = (a->type == ENTITY_PLATFORM) ? b : a;

        if (obj->type != ENTITY_PLATFORM) {
            ResolvePlatformCollision(platform, obj);
        }
        return;
    }

    ResolveDynamicCollision(a, b);
}

void Entities_Update(EntityManager *em, float dt) {
    const int subSteps = 8;
    const float sdt = dt / subSteps;

    for (int s = 0; s < subSteps; s++) {
        for (int i = 0; i < em->count; i++) {
            Entity *e = &em->entities[i];
            if (!e->active || e->type == ENTITY_PLATFORM) continue;

            e->vel.y += GRAVITY * sdt;
            e->pos = Vector2Add(e->pos, Vector2Scale(e->vel, sdt));
            e->angle += e->angVel * sdt;
            e->angVel *= ANGULAR_DAMPING;

            if (e->type == ENTITY_BLOCK) {
                if (fabsf(e->vel.x) < 6.0f && fabsf(e->vel.y) < 8.0f && fabsf(e->angVel) < 0.25f) {
                    e->angVel *= 0.85f;
                    if (fabsf(e->angVel) < 0.02f) {
                        e->angVel = 0.0f;
                    }
                    e->vel.x *= 0.90f;
                    if (fabsf(e->vel.x) < 0.05f) {
                        e->vel.x = 0.0f;
                    }
                }
                if (e->angle > PI) e->angle -= 2.0f * PI;
                if (e->angle < -PI) e->angle += 2.0f * PI;
            }

            if (e->type == ENTITY_BIRD) {
                float speed = Vector2Length(e->vel);
                if (speed < 12.0f) {
                    e->timer += sdt;
                } else {
                    e->timer = 0.0f;
                }

                if (e->timer > 1.25f || e->pos.x < -150.0f || e->pos.x > 1350.0f || e->pos.y > 900.0f) {
                    e->active = false;
                    continue;
                }
            }

            if (e->pos.y > 850.0f) {
                e->active = false;
                continue;
            }
        }

        for (int i = 0; i < em->count; i++) {
            for (int j = i + 1; j < em->count; j++) {
                if (em->entities[i].active && em->entities[j].active) {
                    ResolveCollision(&em->entities[i], &em->entities[j]);
                }
            }
        }
    }

    for (int i = 0; i < em->count; i++) {
        Entity *e = &em->entities[i];
        if (e->active &&
            e->type != ENTITY_BIRD &&
            e->type != ENTITY_PLATFORM &&
            e->health <= 0.0f) {
            e->active = false;
        }
    }
}

void Entities_Draw(EntityManager *em) {
    for (int i = 0; i < em->count; i++) {
        Entity *e = &em->entities[i];
        if (!e->active) continue;

        if (e->type == ENTITY_PLATFORM) {
            Rectangle dirt = {
                e->pos.x - e->size.x * 0.5f,
                e->pos.y - e->size.y * 0.5f,
                e->size.x,
                e->size.y
            };
            Rectangle grass = {
                dirt.x,
                dirt.y - e->size.y * 0.22f,
                e->size.x,
                e->size.y * 0.28f
            };

            DrawRectangleRounded(dirt, 0.24f, 10, (Color){ 124, 83, 46, 255 });
            DrawRectangleRounded(grass, 0.28f, 10, (Color){ 102, 168, 66, 255 });
        } else if (e->type == ENTITY_BLOCK) {
            Rectangle r = { e->pos.x, e->pos.y, e->size.x, e->size.y };
            DrawRectanglePro(r, (Vector2){ e->size.x/2, e->size.y/2 }, e->angle * RAD2DEG, e->color);
            if (e->subType.blockType == BLOCK_STONE && e->health < 150.0f) {
                DrawLineV(Vector2Add(e->pos, (Vector2){ -10, -10 }), Vector2Add(e->pos, (Vector2){ 10, 10 }), BLACK);
                DrawLineV(Vector2Add(e->pos, (Vector2){ 10, -10 }), Vector2Add(e->pos, (Vector2){ -10, 10 }), BLACK);
            }
        } else if (e->type == ENTITY_PIG) {
            DrawCircleV(e->pos, e->radius, e->color);
            DrawCircleV(Vector2Add(e->pos, (Vector2){ 0, 4 }), e->radius * 0.35f, (Color){ 0, 180, 0, 255 });
        } else {
            DrawCircleV(e->pos, e->radius, e->color);
            if (e->subType.birdType == BIRD_BLACK) {
                DrawCircleV(Vector2Add(e->pos, (Vector2){ 0, -10 }), 5, YELLOW);
            }
        }
    }
}

int Entities_CountActive(const EntityManager *em, EntityType type) {
    int count = 0;

    if (!em) return 0;

    for (int i = 0; i < em->count; i++) {
        if (em->entities[i].active && em->entities[i].type == type) {
            count++;
        }
    }

    return count;
}

void Entities_Cleanup(EntityManager *em) {
    if (em) free(em);
}
