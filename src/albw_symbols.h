// ============================================
// NEW CODE - ALBT multiplatform
// Per-platform mangled names for DEFINE_HOOK_SYMBOL targets.
//
// The host resolves hook targets through a symgen symbol manifest generated from
// the game binary (src/dusk/mods/svc/hook.cpp, resolve_symbol_checked), so the
// string must match that binary's symbol table EXACTLY. C++ mangling is
// ABI-specific, so a single literal cannot work everywhere:
//
//   Windows        MSVC mangling            "?name@@YA..."
//   Linux/Android  Itanium                  "_Z<len>name..."
//   macOS/iOS      Itanium + Mach-O's '_'   "__Z<len>name..."
//
// Hardcoding the MSVC name made every one of these fail to resolve off Windows.
// Because a failed hook install aborts mod_initialize, that turned into a total
// load failure on Linux:
//
//   [loader] hook target '?dComIfGp_setItemMagicCount@@YAXF@Z' did not resolve
//   [loader] failed: SetItemMagicCount (1)
//
// Every Itanium string below was read back from clang for the exact signature,
// not hand-derived. Do not edit one without re-deriving it the same way.
//
// Prefer DEFINE_HOOK(&Class::method, Tag) over DEFINE_HOOK_SYMBOL wherever the
// header declares the function - the compiler then mangles per target and none
// of this is needed. These five have no usable declaration at the call site.
// ============================================

#pragma once

#if defined(_WIN32)
#define ALBT_SYM(msvc, itanium) msvc
#elif defined(__APPLE__)
#define ALBT_SYM(msvc, itanium) "_" itanium
#else
#define ALBT_SYM(msvc, itanium) itanium
#endif

// fopAcM_fastCreate(s16, u32, const cXyz*, int, const csXyz*, const cXyz*, s8,
//                   createFunc, void*, u32, u8)  - the TARGET_PC IF_DUSK_ARG overload.
#define ALBT_SYM_FASTCREATE                                                                        \
    ALBT_SYM("?fopAcM_fastCreate@@YAPEAVfopAc_ac_c@@FIPEBUcXyz@@HPEBVcsXyz@@0CP6AHPEAX@Z2IE@Z",     \
             "_Z17fopAcM_fastCreatesjPK4cXyziPK5csXyzS1_aPFiPvES5_jh")

// dComIfGp_setSelectItem(int)
#define ALBT_SYM_SET_SELECT_ITEM                                                                   \
    ALBT_SYM("?dComIfGp_setSelectItem@@YAXH@Z", "_Z22dComIfGp_setSelectItemi")

// dComIfGp_setItemMagicCount(s16)
#define ALBT_SYM_SET_ITEM_MAGIC_COUNT                                                              \
    ALBT_SYM("?dComIfGp_setItemMagicCount@@YAXF@Z", "_Z26dComIfGp_setItemMagicCounts")

// dComIfGs_setSelectItemIndex(int, u8)
#define ALBT_SYM_SET_SELECT_ITEM_INDEX                                                             \
    ALBT_SYM("?dComIfGs_setSelectItemIndex@@YAXHE@Z", "_Z27dComIfGs_setSelectItemIndexih")

// cc_at_check(fopAc_ac_c*, dCcU_AtInfo*) - the undecorated name is ambiguous on
// some hosts ("maps to more than one address; use the mangled name").
#define ALBT_SYM_CC_AT_CHECK                                                                       \
    ALBT_SYM("?cc_at_check@@YAPEAVfopAc_ac_c@@PEAV1@PEAUdCcU_AtInfo@@@Z",                          \
             "_Z11cc_at_checkP10fopAc_ac_cP11dCcU_AtInfo")
// fopAcM_create(s16, u32, const cXyz*, int, const csXyz*, const cXyz*, s8, u32, u8)
#define ALBT_SYM_FOPACM_CREATE                                                                         ALBT_SYM("?fopAcM_create@@YAIFIPEBUcXyz@@HPEBVcsXyz@@0CIE@Z",                                                "_Z13fopAcM_createsjPK4cXyziPK5csXyzS1_ajh")

// dComIfG_resLoad(request_of_phase_process_class*, const char*)
#define ALBT_SYM_RES_LOAD                                                                              ALBT_SYM("?dComIfG_resLoad@@YAHPEAUrequest_of_phase_process_class@@PEBD@Z",                                  "_Z15dComIfG_resLoadP30request_of_phase_process_classPKc")

// ============================================
// NEW CODE ENDS HERE
// ============================================
