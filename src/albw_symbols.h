// ============================================
// NEW CODE - ALBT multiplatform
// Per-platform mangled names for DEFINE_HOOK_SYMBOL targets.
//
// The host resolves hook targets through a symgen symbol manifest generated from
// the game binary (src/dusk/mods/svc/hook.cpp, resolve_symbol_checked), so the
// string must match that binary's symbol table EXACTLY. C++ mangling is
// ABI-specific, so a single literal cannot work everywhere:
//
//   Windows                 MSVC mangling   "?name@@YA..."
//   Linux/Android/Apple     Itanium         "_Z<len>name..."
//
// (macOS/iOS take the SAME string as Linux - see the ALBT_SYM note below. An
// earlier version added Mach-O's leading '_' here and broke every Apple hook.)
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

// APPLE TAKES THE PLAIN ITANIUM NAME - NO LEADING UNDERSCORE.
//
// The Mach-O symbol TABLE stores these with a leading '_', which is why the
// first version of this header prefixed one. That was wrong: the host does not
// look the name up in the symbol table, it looks it up in symgen's manifest,
// and that manifest's contract is explicit (dusklight sdk,
// src/dusk/mods/manifest.hpp:38-40):
//
//   "Names can be either the platform's mangled name (i.e. the name passed to
//    dlopen; NO MACH-O LEADING UNDERSCORE) or the function name without
//    parameters (e.g. "daAlink_c::execute")."
//
// manifest::resolve() is an exact strcmp with no normalisation
// (src/dusk/mods/manifest.cpp:362-394), so the extra '_' made EVERY
// DEFINE_HOOK_SYMBOL target below miss on macOS and iOS - since day one, on
// every release. Combined with the old fatal install() that turned the first
// miss into a total mod unload, that is the
// "Failed - Reason: SetItemMagicCount" players reported.
//
// So Apple and Linux/Android share one spelling; only Windows differs.
#if defined(_WIN32)
#define ALBT_SYM(msvc, itanium) msvc
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
// fopAcM_createItemForBoss(const cXyz*, int, int, const csXyz*, const cXyz*, f32, f32, int, const char*)
#define ALBT_SYM_FOPACM_CREATE_ITEM_FOR_BOSS                                                           ALBT_SYM("?fopAcM_createItemForBoss@@YAIPEBUcXyz@@HHPEBVcsXyz@@0MMHPEBD@Z",                                  "_Z24fopAcM_createItemForBossPK4cXyziiPK5csXyzS1_ffiPKc")

// dComIfG_resLoad(request_of_phase_process_class*, const char*)
#define ALBT_SYM_RES_LOAD                                                                              ALBT_SYM("?dComIfG_resLoad@@YAHPEAUrequest_of_phase_process_class@@PEBD@Z",                                  "_Z15dComIfG_resLoadP30request_of_phase_process_classPKc")

// ============================================
// NEW CODE - outfit-transition crash family P0 (destructor teardown hook target)
//
// daAlink_c::~daAlink_c() - a destructor CANNOT be named by member pointer
// (C++ forbids &T::~T), so DEFINE_HOOK(&daAlink_c::~daAlink_c, ...) is
// ill-formed and the DEFINE_HOOK_SYMBOL mangled-name route is required even
// though d_a_alink.h:1827 declares the destructor.
//
// MSVC name VERIFIED against STOCK v2.0.0 dusklight_exports.def:1232
// ("??1daAlink_c@@UEAA@XZ" - present, exact).
//
// Itanium name is the standard ABI D1 (complete-object) destructor. UNLIKE the
// entries above it was NOT read back from clang (no non-Windows toolchain in
// this session) - a destructor also emits D2 (base-object) and, being virtual,
// D0 (deleting); if the non-Windows symbol manifests carry D1/D2 at distinct
// addresses, D1 is still the one the virtual explicit call in daAlink_Delete
// (i_this->~daAlink_c()) lands on. VERIFY on the Linux/macOS CI lanes before
// merging; the undecorated display name "daAlink_c::~daAlink_c" is NOT a safe
// fallback (multi-address ambiguity, see cc_at_check note above).
// ============================================
#define ALBT_SYM_ALINK_DTOR ALBT_SYM("??1daAlink_c@@UEAA@XZ", "_ZN9daAlink_cD1Ev")

// ============================================
// NEW CODE ENDS HERE
// ============================================
