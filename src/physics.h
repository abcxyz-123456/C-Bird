#ifndef PHYSICS_H
#define PHYSICS_H

#include "raylib.h"

/* 
 * 极简物理引擎定义：使用顺序冲量法模拟 
 */
typedef struct {
    float gravity;
    float timeStep;
    int   iterations;
} PhysicsWorld;

PhysicsWorld* Physics_Init(void);
void          Physics_Update(void *entityManager, float dt);
void          Physics_ApplyExplosion(void *entityManager, Vector2 epicenter, float radius, float force);
void          Physics_Cleanup(PhysicsWorld *world);

/* 坐标转换保持兼容 */
Vector2 PhysicsToScreen(Vector2 pos);
Vector2 ScreenToPhysics(Vector2 pos);

#endif
