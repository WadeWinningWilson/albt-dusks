#pragma once

enum AlbwOutfitKind {
    ALBW_OUTFIT_ORDON = 0,
    ALBW_OUTFIT_HEROS,
    ALBW_OUTFIT_ZORA,
    ALBW_OUTFIT_MAGIC,
    ALBW_OUTFIT_COUNT,
};

AlbwOutfitKind albw_outfit_get_active();
AlbwOutfitKind albw_outfit_get_next_owned(AlbwOutfitKind current);
bool albw_outfit_equip(AlbwOutfitKind kind);
bool albw_outfit_is_swap_blocked();
