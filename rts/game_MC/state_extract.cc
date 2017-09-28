#include "state_feature.h"

void MCExtract(const RTSState &s, PlayerId player_id, bool respect_fow, std::vector<float> *state) {
    const GameEnv &env = s.env();

    const int n_type = env.GetGameDef().GetNumUnitType();
    const int n_additional = 2;
    const int resource_grid = 50;
    const int res_pt = NUM_RES_SLOT;
    const int total_channel = n_type + n_additional + res_pt;

    const auto &m = env.GetMap();

    // [Channel, width, height]
    const int sz = total_channel * m.GetXSize() * m.GetYSize();
    state->resize(sz);
    std::fill(state->begin(), state->end(), 0.0);

    std::map<int, std::pair<int, float> > idx2record;

    PlayerId visibility_check = respect_fow ? player_id : INVALID;

    auto unit_iter = env.GetUnitIterator(visibility_check);
    float total_hp_ratio = 0.0;

    int myworker = 0;
    int mytroop = 0;
    int mybarrack = 0;

    std::vector<int> quantized_r(env.GetNumOfPlayers(), 0);

    while (! unit_iter.end()) {
        const Unit &u = *unit_iter;
        int x = int(u.GetPointF().x);
        int y = int(u.GetPointF().y);
        float hp_level = u.GetProperty()._hp / (u.GetProperty()._max_hp + 1e-6);
        UnitType t = u.GetUnitType();

        bool self_unit = (u.GetPlayerId() == player_id);

        accu_value(_OFFSET(t, x, y, m), 1.0, idx2record);

        // Self unit or enemy unit.
        // For historical reason, the flag of enemy unit = 2
        accu_value(_OFFSET(n_type, x, y, m), (self_unit ? 1 : 2), idx2record);
        accu_value(_OFFSET(n_type + 1, x, y, m), hp_level, idx2record);

        total_hp_ratio += hp_level;

        if (self_unit) {
            if (t == WORKER) myworker += 1;
            else if (t == MELEE_ATTACKER || t == RANGE_ATTACKER) mytroop += 1;
            else if (t == BARRACKS) mybarrack += 1;
       }

        ++ unit_iter;
    }

    for (const auto &p : idx2record) {
        state->at(p.first) = p.second.second / p.second.first;
    }

    myworker = min(myworker, 3);
    mytroop = min(mytroop, 5);
    mybarrack = min(mybarrack, 1);

    for (int i = 0; i < env.GetNumOfPlayers(); ++i) {
        // Omit player signal from other player's perspective.
        if (visibility_check != INVALID && visibility_check != i) continue;
        const auto &player = env.GetPlayer(i);
        quantized_r[i] = min(int(player.GetResource() / resource_grid), res_pt - 1);
    }

    if (player_id != INVALID) {
        // Add resource layer for the current player.
        const int c = _OFFSET(n_type + n_additional + quantized_r[player_id], 0, 0, m);
        std::fill(state->begin() + c, state->begin() + c + m.GetXSize() * m.GetYSize(), 1.0);
    }
}
