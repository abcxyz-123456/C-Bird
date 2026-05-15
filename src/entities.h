#ifndef ENTITIES_H
#define ENTITIES_H

#include "raylib.h"

typedef enum { BIRD_RED, BIRD_BLUE, BIRD_YELLOW, BIRD_BLACK } BirdType;
typedef enum { BLOCK_WOOD, BLOCK_STONE, BLOCK_GLASS } BlockType;
typedef enum { ENTITY_BIRD, ENTITY_PIG, ENTITY_BLOCK, ENTITY_PLATFORM } EntityType;

typedef struct {
    EntityType type;
    Vector2    pos;         /* 位置 (中心) */
    Vector2    vel;         /* 速度 */
    float      angle;       /* 角度 (弧度) */
    float      angVel;      /* 角速度 */
    float      mass;        /* 质量 */
    float      invMass;     /* 质量倒数 (0 为静态) */
    float      restitution; /* 弹性 (0-1) */
    float      friction;    /* 摩擦 (0-1) */
    Vector2    size;        /* 碰撞尺寸 */
    float      radius;      /* 碰撞半径 */
    float      health;
    bool       active;
    Color      color;
    union { BirdType birdType; BlockType blockType; } subType;
    
    /* 扩展属性用于特殊能力 */
    bool       abilityUsed;
    float      timer;
} Entity;

#define MAX_ENTITIES 256

typedef struct {
    Entity entities[MAX_ENTITIES];
    int    count;
} EntityManager;

EntityManager* Entities_Init(void);
Entity* Entities_AddBird (EntityManager *em, Vector2 pos, BirdType type);
Entity* Entities_AddPig  (EntityManager *em, Vector2 pos);
Entity* Entities_AddBlock(EntityManager *em, Vector2 pos, Vector2 size, BlockType type, float angle);
Entity* Entities_AddPlatform(EntityManager *em, Vector2 pos, Vector2 size);

void Entities_Update(EntityManager *em, float dt);
void Entities_Draw(EntityManager *em);
int  Entities_CountActive(const EntityManager *em, EntityType type);
void Entities_Cleanup(EntityManager *em);

#endif
