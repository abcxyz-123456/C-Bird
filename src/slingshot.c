#include "slingshot.h"
#include "raymath.h"
#include <stddef.h>

void Slingshot_Init(Slingshot *s, Vector2 pos) {
    s->basePos = pos;
    s->dragPos = pos;
    s->isDragging = false;
    s->maxDrag = 120.0f;
    s->activeBird = BIRD_RED;
}

Entity* Slingshot_Update(Slingshot *s, EntityManager *em, PhysicsWorld *world, Vector2 mousePos) {
    (void)world;

    Vector2 m = mousePos;
    Entity *launchedBird = NULL;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (CheckCollisionPointCircle(m, s->basePos, 50.0f)) {
            s->isDragging = true;
        }
    }

    if (s->isDragging) {
        s->dragPos = m;
        float d = Vector2Distance(s->basePos, s->dragPos);
        if (d > s->maxDrag) {
            s->dragPos = Vector2Add(
                s->basePos,
                Vector2Scale(Vector2Normalize(Vector2Subtract(s->dragPos, s->basePos)), s->maxDrag)
            );
        }

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            Vector2 launch = Vector2Subtract(s->basePos, s->dragPos);
            if (Vector2Length(launch) > 10.0f) {
                launchedBird = Entities_AddBird(em, s->basePos, s->activeBird);
                if (launchedBird) {
                    const float power = 7.0f;
                    launchedBird->vel.x = launch.x * power;
                    launchedBird->vel.y = launch.y * power;
                }
            }

            s->isDragging = false;
            s->dragPos = s->basePos;
        }
    }

    return launchedBird;
}

void Slingshot_Draw(Slingshot *s) {
    DrawRectangleV((Vector2){ s->basePos.x - 5, s->basePos.y }, (Vector2){ 10, 100 }, DARKBROWN);
    if (s->isDragging) {
        DrawLineEx(s->basePos, s->dragPos, 4, (Color){ 100, 50, 20, 255 });
        Color c = RED;
        float radius = 16.0f;
        switch (s->activeBird) {
            case BIRD_BLUE:   c = SKYBLUE; radius = 10.0f; break;
            case BIRD_YELLOW: c = YELLOW;  radius = 16.0f; break;
            case BIRD_BLACK:  c = BLACK;   radius = 20.0f; break;
            case BIRD_RED:
            default:          c = RED;     radius = 16.0f; break;
        }
        DrawCircleV(s->dragPos, radius, c);
        if (s->activeBird == BIRD_BLACK) {
            DrawCircleV(Vector2Add(s->dragPos, (Vector2){ 0, -10 }), 5, YELLOW);
        }
    }
}
