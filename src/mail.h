#pragma once

#include <cstdint>

#include "mods/api.h"

class daNpc_Post_c;
class J2DTextBox;
class dMsgStringBase_c;

static constexpr int kAlbwMailLetterIndex = 16;
static constexpr int kAlbwMailLetterIndex2 = 17;
static constexpr std::uint16_t kAlbwMailRuntimeMsgId = 0xFFFE;
static constexpr std::uint16_t kAlbwMailPendingEventIndex = 812;
static constexpr std::uint16_t kAlbwMailDeliveredEventIndex = 813;
static constexpr std::uint16_t kAlbwMailDeliverPostParam = 0x7A01;

void dAlbwMail_init();
bool dAlbwMail_isTestMode();
bool dAlbwMail_isRuntimeLetter(int letterIndex);
bool dAlbwMail_hasReceivedBundle();
bool dAlbwMail_hasTutorialScrolls();
bool dAlbwMail_isDeliverPostman(const daNpc_Post_c* postman);
bool dAlbwMail_shouldSpawnNorthFaron();
void dAlbwMail_onNorthFaronSpawn(int roomNo);
bool dAlbwMail_canCreateNorthFaronActors(int roomNo);
void dAlbwMail_trySpawnNorthFaronActors(int roomNo);
void dAlbwMail_tickNorthFaron();
bool dAlbwMail_canTriggerDeliver();
void dAlbwMail_onDeliverCutsceneOrdered();
void dAlbwMail_onDeliverCutsceneFinished(bool letterReceived);
const char* dAlbwMail_getDeliverSpeech();
void dAlbwMail_tryQueuePending();
void dAlbwMail_onDeliveryComplete();
int dAlbwMail_getDeliverLineCount();
const char* dAlbwMail_getDeliverLine(int pageIndex);
const char* dAlbwMail_getLetterSubject(int letterIndex);
const char* dAlbwMail_getLetterSender(int letterIndex);
const char* dAlbwMail_getLetterBodyPage(int letterIndex, int pageIndex);
int dAlbwMail_getLetterBodyPageCount(int letterIndex);
void dAlbwMail_drawLetterSubject(int letterIndex, J2DTextBox* textBox);
void dAlbwMail_drawLetterSender(int letterIndex, J2DTextBox* textBox);
void dAlbwMail_drawLetterBodyPage(int letterIndex, int pageIndex, int lineMax, J2DTextBox* bodyBox,
                                  J2DTextBox* rubyBox, dMsgStringBase_c* stringDrawer);
int dAlbwMail_getLetterBodyPageMax(int letterIndex, dMsgStringBase_c* stringDrawer, int lineMax);

ModResult albw_mail_init(ModError* error);
ModResult albw_mail_shutdown(ModError* error);
void albw_mail_update();
