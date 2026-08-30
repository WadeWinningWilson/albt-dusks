#include "albw_stage.h"

#include "albw_game.h"
#include "boss_refinement_hooks.h"
#include "focused_arts.h"

#include <cstring>

namespace {

int s_lastStayRoom = -1;
char s_lastStage[8] = {};

}  // namespace

void albw_stage_tick() {
    const int room = albw_game::stay_room();
    const char* stage = albw_game::stage_name();
    if (stage == nullptr) {
        stage = "";
    }

    if (room == s_lastStayRoom && std::strncmp(stage, s_lastStage, sizeof(s_lastStage)) == 0) {
        return;
    }

    s_lastStayRoom = room;
    std::strncpy(s_lastStage, stage, sizeof(s_lastStage) - 1);
    s_lastStage[sizeof(s_lastStage) - 1] = '\0';
    dFocusedArts_onStageLoad();
    albw_boss_refinement_on_stage_load();
}
