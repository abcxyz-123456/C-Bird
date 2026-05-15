#include "editor.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>

void Editor_Init(Editor *ed) {
    ed->active = false;
    ed->selectedType = ENTITY_BLOCK;
    ed->selectedBlock = BLOCK_WOOD;
}

void Editor_Update(Editor *ed, EntityManager *em, PhysicsWorld *world) {
    if (IsKeyPressed(KEY_TAB)) ed->active = !ed->active;
    if (!ed->active) return;
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 m = GetMousePosition();
        if (ed->selectedType == ENTITY_BLOCK) {
            Entities_AddBlock(em, m, (Vector2){40, 40}, ed->selectedBlock, 0.0f);
        } else {
            Entities_AddPig(em, m);
        }
    }
}

void Editor_Draw(Editor *ed) {
    if (!ed->active) {
        DrawText("TAB for Editor", 10, 10, 18, DARKGRAY);
    } else {
        DrawText("EDITOR ON | LEFT CLICK TO PLACE | TAB TO CLOSE", 10, 10, 18, RED);
    }
}

void Editor_Save(EntityManager *em, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) return;

    fprintf(fp, "{\n");
    fprintf(fp, "  \"birds\": { \"red\": 0, \"blue\": 0, \"yellow\": 0, \"black\": 0 },\n");
    fprintf(fp, "  \"pigs\": %d,\n", Entities_CountActive(em, ENTITY_PIG));
    fprintf(fp, "  \"castle\": 0,\n");
    fprintf(fp, "  \"floor\": [],\n");
    fprintf(fp, "  \"blocks\": {\n");
    fprintf(fp, "    \"wood\": 0,\n");
    fprintf(fp, "    \"stone\": 0,\n");
    fprintf(fp, "    \"glass\": 0\n");
    fprintf(fp, "  }\n");
    fprintf(fp, "}\n");

    fclose(fp);
}
