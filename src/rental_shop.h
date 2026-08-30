#pragma once

#include "mods/api.h"

#include <os.h>

/** Live rental-shop open gate (any non-closed FSM state). */
bool albw_rental_shop_is_open();
void albw_rental_shop_set_open(bool open);

int albw_rental_menu_res_swap_generation();
void albw_rental_menu_res_bump_generation();

inline int dAlbwMenuRes_swapGeneration() {
    return albw_rental_menu_res_swap_generation();
}

struct dALBWVisibleEntry {
    const char* name;
    int price;
    bool purchasable;
    const char* desc;
    u8 itemNo;
    const char* customIconName;
    bool isOocooService;
    bool showNameWhenSoldOut;
    bool isStorageStore;
    bool isStorageRetrieve;
};

void dALBWRental_open();
void dALBWRental_close();
bool dALBWRental_isOpen();
void dALBWRental_tick();
bool dALBWRental_justClosed();

void dALBWRental_armVanillaTalkSuppress();
void dALBWRental_clearVanillaTalkSuppress();
bool dALBWRental_shouldSuppressVanillaTalkMsg();

void dALBWRental_advanceToShop();
void dALBWRental_advanceToClosed();
bool dALBWRental_isShopState();
bool dALBWRental_isGreetingState();
bool dALBWRental_isFarewellState();

const char* dALBWRental_getGreetingText();
const char* dALBWRental_getGreetingPage2();
const char* dALBWRental_getGreetingPage3();
const char* dALBWRental_getFarewellText();

bool dALBWRental_justEnteredGreeting();
bool dALBWRental_justEnteredShop();
bool dALBWRental_justEnteredFarewell();
bool dALBWRental_justPurchased();
bool dALBWRental_justFailedPurchase();

const dALBWVisibleEntry* dALBWRental_getVisibleList(int* outCount);
int dALBWRental_getSelectedIdx();
const char* dALBWRental_getPageTitle();
int dALBWRental_getPageNumber();
int dALBWRental_getPageCount();

ModResult albw_rental_shop_init(ModError* error);
ModResult albw_rental_shop_shutdown(ModError* error);
void albw_rental_shop_tick();
