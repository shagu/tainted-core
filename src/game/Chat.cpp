/*
 * This file is part of the OregonCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Common.h"
#include "ObjectMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Database/DatabaseEnv.h"

#include "CellImpl.h"
#include "Chat.h"
#include "GridNotifiersImpl.h"
#include "Language.h"
#include "Log.h"
#include "Opcodes.h"
#include "Player.h"
#include "UpdateMask.h"
#include "MapManager.h"
#include "SpellMgr.h"
#include "ScriptMgr.h"
#include "LuaEngine.h"

bool ChatHandler::load_command_table = true;

template<bool (ChatHandler::*F)(const char*)>
bool OldHandler(ChatHandler* chatHandler, const char* args)
{
    return (chatHandler->*F)(args);
}

// get number of commands in table
static size_t getCommandTableSize(const ChatCommand* commands)
{
    if (!commands)
        return 0;
    size_t count = 0;
    while (commands[count].Name != NULL)
        count++;
    return count;
}

// append source command table to target, return number of appended commands
static size_t appendCommandTable(ChatCommand* target, const ChatCommand* source)
{
    const size_t count = getCommandTableSize(source);
    if (count)
        memcpy(target, source, count * sizeof(ChatCommand));
    return count;
}


ChatCommand* ChatHandler::getCommandTable()
{
    static ChatCommand accountSetCommandTable[] =
    {
        { "addon",          SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleAccountSetAddonCommand>,     "", NULL },
        { "gmlevel",        SEC_CONSOLE,        true,  OldHandler<&ChatHandler::HandleAccountSetGmLevelCommand>,   "", NULL },
        { "password",       SEC_CONSOLE,        true,  OldHandler<&ChatHandler::HandleAccountSetPasswordCommand>,  "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand accountCommandTable[] =
    {
        { "create",         SEC_CONSOLE,        true,  OldHandler<&ChatHandler::HandleAccountCreateCommand>,       "", NULL },
        { "delete",         SEC_CONSOLE,        true,  OldHandler<&ChatHandler::HandleAccountDeleteCommand>,       "", NULL },
        { "onlinelist",     SEC_CONSOLE,        true,  OldHandler<&ChatHandler::HandleAccountOnlineListCommand>,   "", NULL },
        { "lock",           SEC_PLAYER,         false, OldHandler<&ChatHandler::HandleAccountLockCommand>,         "", NULL },
        { "set",            SEC_ADMINISTRATOR,  true,  NULL,                                           "", accountSetCommandTable },
        { "password",       SEC_PLAYER,         false, OldHandler<&ChatHandler::HandleAccountPasswordCommand>,     "", NULL },
        { "",               SEC_PLAYER,         false, OldHandler<&ChatHandler::HandleAccountCommand>,             "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand serverSetCommandTable[] =
    {
        { "logmask",        SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleServerSetLogMaskCommand>,    "", NULL },
        { "difftime",       SEC_CONSOLE,        true,  OldHandler<&ChatHandler::HandleServerSetDiffTimeCommand>,   "", NULL },
        { "motd",           SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleServerSetMotdCommand>,       "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand serverIdleRestartCommandTable[] =
    {
        { "cancel",         SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleServerShutDownCancelCommand>, "", NULL },
        { ""   ,            SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleServerIdleRestartCommand>,   "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand serverIdleShutdownCommandTable[] =
    {
        { "cancel",         SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleServerShutDownCancelCommand>, "", NULL },
        { ""   ,            SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleServerIdleShutDownCommand>,  "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand serverRestartCommandTable[] =
    {
        { "cancel",         SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleServerShutDownCancelCommand>, "", NULL },
        { ""   ,            SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleServerRestartCommand>,       "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand serverShutdownCommandTable[] =
    {
        { "cancel",         SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleServerShutDownCancelCommand>, "", NULL },
        { ""   ,            SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleServerShutDownCommand>,      "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand serverCommandTable[] =
    {
        { "corpses",        SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleServerCorpsesCommand>,       "", NULL },
        { "exit",           SEC_CONSOLE,        true,  OldHandler<&ChatHandler::HandleServerExitCommand>,          "", NULL },
        { "idlerestart",    SEC_ADMINISTRATOR,  true,  NULL,                                           "", serverIdleRestartCommandTable },
        { "idleshutdown",   SEC_ADMINISTRATOR,  true,  NULL,                                           "", serverIdleShutdownCommandTable },
        { "info",           SEC_PLAYER,         true,  OldHandler<&ChatHandler::HandleServerInfoCommand>,          "", NULL },
        { "motd",           SEC_PLAYER,         true,  OldHandler<&ChatHandler::HandleServerMotdCommand>,          "", NULL },
        { "plimit",         SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleServerPLimitCommand>,        "", NULL },
        { "restart",        SEC_ADMINISTRATOR,  true,  NULL,                                           "", serverRestartCommandTable },
        { "shutdown",       SEC_ADMINISTRATOR,  true,  NULL,                                           "", serverShutdownCommandTable },
        { "set",            SEC_ADMINISTRATOR,  true,  NULL,                                           "", serverSetCommandTable },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand modifyCommandTable[] =
    {
        { "hp",             SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleModifyHPCommand>,            "", NULL },
        { "mana",           SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleModifyManaCommand>,          "", NULL },
        { "rage",           SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleModifyRageCommand>,          "", NULL },
        { "energy",         SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleModifyEnergyCommand>,        "", NULL },
        { "money",          SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleModifyMoneyCommand>,         "", NULL },
        { "speed",          SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleModifySpeedCommand>,         "", NULL },
        { "swim",           SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleModifySwimCommand>,          "", NULL },
        { "scale",          SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleModifyScaleCommand>,         "", NULL },
        { "bit",            SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleModifyBitCommand>,           "", NULL },
        { "bwalk",          SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleModifyBWalkCommand>,         "", NULL },
        { "fly",            SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleModifyFlyCommand>,           "", NULL },
        { "aspeed",         SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleModifyASpeedCommand>,        "", NULL },
        { "faction",        SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleModifyFactionCommand>,       "", NULL },
        { "spell",          SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleModifySpellCommand>,         "", NULL },
        { "tp",             SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleModifyTalentCommand>,        "", NULL },
        { "mount",          SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleModifyMountCommand>,         "", NULL },
        { "honor",          SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleModifyHonorCommand>,         "", NULL },
        { "rep",            SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleModifyRepCommand>,           "", NULL },
        { "arena",          SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleModifyArenaCommand>,         "", NULL },
        { "drunk",          SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleDrunkCommand>,               "", NULL },
        { "standstate",     SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleStandStateCommand>,          "", NULL },
        { "morph",          SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleMorphCommand>,               "", NULL },
        { "gender",         SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleModifyGenderCommand>,        "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand wpCommandTable[] =
    {
        { "show",           SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleWpShowCommand>,              "", NULL },
        { "addwp",          SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleWpAddCommand>,               "", NULL },
        { "load",           SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleWpLoadPathCommand>,          "", NULL },
        { "modify",         SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleWpModifyCommand>,            "", NULL },
        { "event",          SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleWpEventCommand>,             "", NULL },
        { "unload",         SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleWpUnLoadPathCommand>,        "", NULL },

        { NULL,             0,                  false, NULL,                                           "", NULL }
    };


    static ChatCommand banCommandTable[] =
    {
        { "account",        SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleBanAccountCommand>,          "", NULL },
        { "character",      SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleBanCharacterCommand>,        "", NULL },
        { "ip",             SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleBanIPCommand>,               "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand baninfoCommandTable[] =
    {
        { "account",        SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleBanInfoAccountCommand>,      "", NULL },
        { "character",      SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleBanInfoCharacterCommand>,    "", NULL },
        { "ip",             SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleBanInfoIPCommand>,           "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand banlistCommandTable[] =
    {
        { "account",        SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleBanListAccountCommand>,      "", NULL },
        { "character",      SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleBanListCharacterCommand>,    "", NULL },
        { "ip",             SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleBanListIPCommand>,           "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand titlesCommandTable[] =
    {
        { "add",            SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleTitlesAddCommand>,           "", NULL },
        { "current",        SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleTitlesCurrentCommand>,       "", NULL },
        { "remove",         SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleTitlesRemoveCommand>,        "", NULL },
        { "setmask",        SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleTitlesSetMaskCommand>,       "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand unbanCommandTable[] =
    {
        { "account",        SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleUnBanAccountCommand>,        "", NULL },
        { "character",      SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleUnBanCharacterCommand>,      "", NULL },
        { "ip",             SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleUnBanIPCommand>,             "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand characterDeletedCommandTable[] =
    {
        { "delete",         SEC_CONSOLE,        true,  OldHandler<&ChatHandler::HandleCharacterDeletedDeleteCommand>, "", NULL },
        { "list",           SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleCharacterDeletedListCommand>, "", NULL },
        { "restore",        SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleCharacterDeletedRestoreCommand>, "", NULL },
        { "old",            SEC_CONSOLE,        true,  OldHandler<&ChatHandler::HandleCharacterDeletedOldCommand>, "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand characterCommandTable[] =
    {
        { "deleted",        SEC_GAMEMASTER,     true,  NULL,                                           "", characterDeletedCommandTable},
        { "erase",          SEC_CONSOLE,        true,  OldHandler<&ChatHandler::HandleCharacterEraseCommand>,      "", NULL },
        { "rename",         SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleCharacterRenameCommand>,     "", NULL },
        { "titles",         SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleCharacterTitlesCommand>,     "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand debugPlayCommandTable[] =
    {
        { "cinematic",      SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleDebugPlayCinematicCommand>,       "", NULL },
        { "sound",          SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleDebugPlaySoundCommand>,           "", NULL },
        { NULL,             0,                  false, NULL,                                                "", NULL }
    };

    static ChatCommand debugCommandTable[] =
    {
        { "inarc",          SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleDebugInArcCommand>,          "", NULL },
        { "spellfail",      SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleDebugSpellFailCommand>,      "", NULL },
        { "raferror",       SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleDebugRAFError>,              "", NULL },
        { "setpoi",         SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleSetPoiCommand>,              "", NULL },
        { "qpartymsg",      SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleSendQuestPartyMsgCommand>,   "", NULL },
        { "qinvalidmsg",    SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleSendQuestInvalidMsgCommand>, "", NULL },
        { "equiperr",       SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleEquipErrorCommand>,          "", NULL },
        { "sellerr",        SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleSellErrorCommand>,           "", NULL },
        { "buyerr",         SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleBuyErrorCommand>,            "", NULL },
        { "sendopcode",     SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleSendOpcodeCommand>,          "", NULL },
        { "uws",            SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleUpdateWorldStateCommand>,    "", NULL },
        { "scn",            SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleSendChannelNotifyCommand>,   "", NULL },
        { "scm",            SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleSendChatMsgCommand>,         "", NULL },
        { "getitemstate",   SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleGetItemState>,               "", NULL },
        { "play",           SEC_MODERATOR,      false, NULL,                                           "", debugPlayCommandTable },
        { "update",         SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleUpdate>,                     "", NULL },
        { "setvalue",       SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleSetValue>,                   "", NULL },
        { "getvalue",       SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleGetValue>,                   "", NULL },
        { "Mod32Value",     SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleMod32Value>,                 "", NULL },
        { "anim",           SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleAnimCommand>,                "", NULL },
        { "lootrecipient",  SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleGetLootRecipient>,           "", NULL },
        { "arena",          SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleDebugArenaCommand>,          "", NULL },
        { "bg",             SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleDebugBattlegroundCommand>,   "", NULL },
        { "threatlist",     SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleDebugThreatList>,            "", NULL },
        { "setinstdata",    SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleSetInstanceDataCommand>,     "", NULL },
        { "getinstdata",    SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleGetInstanceDataCommand>,     "", NULL },
        { "spellcrashtest", SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleSpellCrashTestCommand>,      "", NULL },
        { "partyresult",    SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandlePartyResultCommand>,         "", NULL },
        { "animate",        SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleDebugAnimationCommand>,      "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand eventCommandTable[] =
    {
        { "activelist",     SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleEventActiveListCommand>,     "", NULL },
        { "start",          SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleEventStartCommand>,          "", NULL },
        { "stop",           SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleEventStopCommand>,           "", NULL },
        { "",               SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleEventInfoCommand>,           "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand learnCommandTable[] =
    {
        { "all",            SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleLearnAllCommand>,            "", NULL },
        { "all_gm",         SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleLearnAllGMCommand>,          "", NULL },
        { "all_crafts",     SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleLearnAllCraftsCommand>,      "", NULL },
        { "all_default",    SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleLearnAllDefaultCommand>,     "", NULL },
        { "all_lang",       SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleLearnAllLangCommand>,        "", NULL },
        { "all_myclass",    SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleLearnAllMyClassCommand>,     "", NULL },
        { "all_myspells",   SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleLearnAllMySpellsCommand>,    "", NULL },
        { "all_mytalents",  SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleLearnAllMyTalentsCommand>,   "", NULL },
        { "all_recipes",    SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleLearnAllRecipesCommand>,     "", NULL },
        { "",               SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleLearnCommand>,               "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand reloadCommandTable[] =
    {
        { "all",            SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleReloadAllCommand>,           "", NULL },
        { "all_loot",       SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleReloadAllLootCommand>,       "", NULL },
        { "all_npc",        SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleReloadAllNpcCommand>,        "", NULL },
        { "all_quest",      SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleReloadAllQuestCommand>,      "", NULL },
        { "all_scripts",    SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleReloadAllScriptsCommand>,    "", NULL },
        { "all_spell",      SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleReloadAllSpellCommand>,      "", NULL },
        { "all_item",       SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleReloadAllItemCommand>,       "", NULL },
        { "all_locales",    SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleReloadAllLocalesCommand>,    "", NULL },

        { "config",         SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleReloadConfigCommand>,        "", NULL },

        { "areatrigger_tavern",          SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadAreaTriggerTavernCommand>,       "", NULL },
        { "areatrigger_teleport",        SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadAreaTriggerTeleportCommand>,     "", NULL },
        { "access_requirement",          SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadAccessRequirementCommand>,       "", NULL },
        { "areatrigger_involvedrelation", SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadQuestAreaTriggersCommand>,       "", NULL },
        { "autobroadcast",               SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadAutobroadcastCommand>,           "", NULL },
        { "event_scripts",               SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadEventScriptsCommand>,            "", NULL },
        { "command",                     SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadCommandCommand>,                 "", NULL },
        { "conditions",                  SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadConditions>,                     "", NULL },
        { "creature_ai_scripts",         SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadEventAIScriptsCommand>,          "", NULL },
        { "creature_ai_summons",         SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadEventAISummonsCommand>,          "", NULL },
        { "creature_ai_texts",           SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadEventAITextsCommand>,            "", NULL },
        { "creature_questender",         SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadCreatureQuestEnderCommand>,      "", NULL },
        { "creature_linked_respawn",     SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadCreatureLinkedRespawnCommand>,   "", NULL },
        { "creature_loot_template",      SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadLootTemplatesCreatureCommand>,   "", NULL },
        { "creature_queststarter",       SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadCreatureQuestStarterCommand>,    "", NULL },
        //{ "db_script_string",            SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadDbScriptStringCommand,          "", NULL },
        { "disables",                    SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadDisablesCommand>,                "", NULL },
        { "disenchant_loot_template",    SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadLootTemplatesDisenchantCommand>, "", NULL },
        { "fishing_loot_template",       SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadLootTemplatesFishingCommand>,    "", NULL },
        { "graveyard_zone",              SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadGameGraveyardZoneCommand>,       "", NULL },
        { "game_tele",                   SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadGameTeleCommand>,                "", NULL },
        { "gameobject_questender",       SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadGOQuestEnderCommand>,            "", NULL },
        { "gameobject_loot_template",    SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadLootTemplatesGameobjectCommand>, "", NULL },
        { "gameobject_queststarter",     SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadGOQuestStarterCommand>,          "", NULL },
        { "gameobject_scripts",          SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadGameObjectScriptsCommand>,       "", NULL },
        { "gossip_menu",                 SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadGossipMenuCommand>,              "", NULL },
        { "gossip_menu_option",          SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadGossipMenuOptionCommand>,        "", NULL },
        { "item_enchantment_template",   SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadItemEnchantementsCommand>,       "", NULL },
        { "item_loot_template",          SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadLootTemplatesItemCommand>,       "", NULL },
        { "mail_loot_template",          SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadLootTemplatesMailCommand>,       "", NULL },
        { "oregon_string",               SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadOregonStringCommand>,            "", NULL },
        { "npc_gossip",                  SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadNpcGossipCommand>,               "", NULL },
        { "npc_trainer",                 SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadNpcTrainerCommand>,              "", NULL },
        { "npc_vendor",                  SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadNpcVendorCommand>,               "", NULL },
        { "page_text",                   SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadPageTextsCommand>,               "", NULL },
        { "pickpocketing_loot_template", SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadLootTemplatesPickpocketingCommand>, "", NULL},
        { "prospecting_loot_template",   SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadLootTemplatesProspectingCommand>, "", NULL },
        { "quest_end_scripts",           SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadQuestEndScriptsCommand>,         "", NULL },
        { "quest_start_scripts",         SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadQuestStartScriptsCommand>,       "", NULL },
        { "quest_template",              SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadQuestTemplateCommand>,           "", NULL },
        { "reference_loot_template",     SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadLootTemplatesReferenceCommand>,  "", NULL },
        { "reserved_name",               SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadReservedNameCommand>,            "", NULL },
        { "reputation_spillover_template", SEC_ADMINISTRATOR, true, OldHandler<&ChatHandler::HandleReloadReputationSpilloverTemplateCommand>, "", NULL },
        { "skill_discovery_template",    SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadSkillDiscoveryTemplateCommand>,  "", NULL },
        { "skill_extra_item_template",   SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadSkillExtraItemTemplateCommand>,  "", NULL },
        { "skill_fishing_base_level",    SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadSkillFishingBaseLevelCommand>,   "", NULL },
        { "skinning_loot_template",      SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadLootTemplatesSkinningCommand>,   "", NULL },
        { "spell_affect",                SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadSpellAffectCommand>,             "", NULL },
        { "spell_required",              SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadSpellRequiredCommand>,           "", NULL },
        { "spell_groups",                SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadSpellGroupsCommand>,             "", NULL },
        { "spell_group_stack_rules",     SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadSpellGroupStackRulesCommand>,    "", NULL },
        { "spell_learn_spell",           SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadSpellLearnSpellCommand>,         "", NULL },
        { "spell_linked_spell",          SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadSpellLinkedSpellCommand>,        "", NULL },
        { "spell_pet_auras",             SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadSpellPetAurasCommand>,           "", NULL },
        { "spell_proc_event",            SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadSpellProcEventCommand>,          "", NULL },
        { "spell_scripts",               SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadSpellScriptsCommand>,            "", NULL },
        { "spell_target_position",       SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadSpellTargetPositionCommand>,     "", NULL },
        { "spell_threats",               SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadSpellThreatsCommand>,            "", NULL },
        { "locales_creature",            SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadLocalesCreatureCommand>,         "", NULL },
        { "locales_gameobject",          SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadLocalesGameobjectCommand>,       "", NULL },
        { "locales_item",                SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadLocalesItemCommand>,             "", NULL },
        { "locales_npc_text",            SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadLocalesNpcTextCommand>,          "", NULL },
        { "locales_page_text",           SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadLocalesPageTextCommand>,         "", NULL },
        { "locales_quest",               SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadLocalesQuestCommand>,            "", NULL },
        { "auctions",                    SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadAuctionsCommand>,                "", NULL },
        { "waypoint_scripts",            SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadWpScriptsCommand>,               "", NULL },
        { "gm_tickets",                  SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleGMTicketReloadCommand>,                "", NULL },
        { "account_referred",            SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleRAFReloadCommand>,                     "", NULL },

        { "eluna",                       SEC_ADMINISTRATOR, true, OldHandler<&ChatHandler::HandleElunaReloadCommand>,                    "", NULL },

        { "",                            SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleReloadCommand>,                        "", NULL },
        { NULL,                          0,                 false, NULL,                                                     "", NULL }
    };

    static ChatCommand honorCommandTable[] =
    {
        { "add",            SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleAddHonorCommand>,            "", NULL },
        { "addkill",        SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleHonorAddKillCommand>,        "", NULL },
        { "update",         SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleUpdateHonorFieldsCommand>,   "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand guildCommandTable[] =
    {
        { "create",         SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleGuildCreateCommand>,         "", NULL },
        { "delete",         SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleGuildDeleteCommand>,         "", NULL },
        { "invite",         SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleGuildInviteCommand>,         "", NULL },
        { "uninvite",       SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleGuildUninviteCommand>,       "", NULL },
        { "rank",           SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleGuildRankCommand>,           "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand petCommandTable[] =
    {
        { "create",         SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleCreatePetCommand>,           "", NULL },
        { "learn",          SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandlePetLearnCommand>,            "", NULL },
        { "unlearn",        SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandlePetUnlearnCommand>,          "", NULL },
        { "tp",             SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandlePetTpCommand>,               "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };


    static ChatCommand groupCommandTable[] =
    {
        { "leader",         SEC_ADMINISTRATOR,     false,  OldHandler<&ChatHandler::HandleGroupLeaderCommand>,         "", NULL },
        { "disband",        SEC_ADMINISTRATOR,     false,  OldHandler<&ChatHandler::HandleGroupDisbandCommand>,        "", NULL },
        { "remove",         SEC_ADMINISTRATOR,     false,  OldHandler<&ChatHandler::HandleGroupRemoveCommand>,         "", NULL },
        { "join",           SEC_ADMINISTRATOR,     false,  OldHandler<&ChatHandler::HandleGroupJoinCommand>,           "", NULL },
        { NULL,             0,                     false, NULL,                                            "", NULL }
    };

    static ChatCommand lookupPlayerCommandTable[] =
    {
        { "ip",             SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleLookupPlayerIpCommand>,       "", NULL },
        { "account",        SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleLookupPlayerAccountCommand>,  "", NULL },
        { "email",          SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleLookupPlayerEmailCommand>,    "", NULL },
        { NULL,             0,                  false, NULL,                                            "", NULL }
    };

    static ChatCommand lookupCommandTable[] =
    {
        { "area",           SEC_MODERATOR,      true,  OldHandler<&ChatHandler::HandleLookupAreaCommand>,          "", NULL },
        { "creature",       SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleLookupCreatureCommand>,      "", NULL },
        { "event",          SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleLookupEventCommand>,         "", NULL },
        { "faction",        SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleLookupFactionCommand>,       "", NULL },
        { "item",           SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleLookupItemCommand>,          "", NULL },
        { "itemset",        SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleLookupItemSetCommand>,       "", NULL },
        { "object",         SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleLookupObjectCommand>,        "", NULL },
        { "quest",          SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleLookupQuestCommand>,         "", NULL },
        { "player",         SEC_GAMEMASTER,     true,  NULL,                                           "", lookupPlayerCommandTable },
        { "skill",          SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleLookupSkillCommand>,         "", NULL },
        { "spell",          SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleLookupSpellCommand>,         "", NULL },
        { "tele",           SEC_MODERATOR,      true,  OldHandler<&ChatHandler::HandleLookupTeleCommand>,          "", NULL },
        { "title",          SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleLookupTitleCommand>,         "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand resetCommandTable[] =
    {
        { "honor",          SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleResetHonorCommand>,          "", NULL },
        { "level",          SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleResetLevelCommand>,          "", NULL },
        { "spells",         SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleResetSpellsCommand>,         "", NULL },
        { "stats",          SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleResetStatsCommand>,          "", NULL },
        { "talents",        SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleResetTalentsCommand>,        "", NULL },
        { "all",            SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleResetAllCommand>,            "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand castCommandTable[] =
    {
        { "back",           SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleCastBackCommand>,            "", NULL },
        { "dist",           SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleCastDistCommand>,            "", NULL },
        { "self",           SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleCastSelfCommand>,            "", NULL },
        { "target",         SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleCastTargetCommand>,          "", NULL },
        { "",               SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleCastCommand>,                "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand pdumpCommandTable[] =
    {
        { "load",           SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleLoadPDumpCommand>,           "", NULL },
        { "write",          SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleWritePDumpCommand>,          "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand listCommandTable[] =
    {
        { "creature",       SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleListCreatureCommand>,        "", NULL },
        { "item",           SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleListItemCommand>,            "", NULL },
        { "object",         SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleListObjectCommand>,          "", NULL },
        { "auras",          SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleListAurasCommand>,           "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand teleCommandTable[] =
    {
        { "add",            SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleAddTeleCommand>,             "", NULL },
        { "del",            SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleDelTeleCommand>,             "", NULL },
        { "name",           SEC_MODERATOR,      true,  OldHandler<&ChatHandler::HandleNameTeleCommand>,            "", NULL },
        { "group",          SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGroupTeleCommand>,           "", NULL },
        { "",               SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleTeleCommand>,                "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand npcCommandTable[] =
    {
        { "say",            SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleNpcSayCommand>,              "", NULL },
        { "textemote",      SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleNpcTextEmoteCommand>,        "", NULL },
        { "add",            SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleNpcAddCommand>,              "", NULL },
        { "delete",         SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleNpcDeleteCommand>,           "", NULL },
        { "spawndist",      SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleNpcSpawnDistCommand>,        "", NULL },
        { "spawntime",      SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleNpcSpawnTimeCommand>,        "", NULL },
        { "factionid",      SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleNpcFactionIdCommand>,        "", NULL },
        { "setmovetype",    SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleNpcSetMoveTypeCommand>,      "", NULL },
        { "move",           SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleNpcMoveCommand>,             "", NULL },
        { "changelevel",    SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleChangeLevelCommand>,         "", NULL },
        { "setmodel",       SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleNpcSetModelCommand>,         "", NULL },
        { "additem",        SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleAddVendorItemCommand>,       "", NULL },
        { "delitem",        SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleDelVendorItemCommand>,       "", NULL },
        { "flag",           SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleNpcFlagCommand>,             "", NULL },
        { "changeentry",    SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleNpcChangeEntryCommand>,      "", NULL },
        { "info",           SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleNpcInfoCommand>,             "", NULL },
        { "playemote",      SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleNpcPlayEmoteCommand>,        "", NULL },
        { "follow",         SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleNpcFollowCommand>,           "", NULL },
        { "unfollow",       SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleNpcUnFollowCommand>,         "", NULL },
        { "whisper",        SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleNpcWhisperCommand>,          "", NULL },
        { "yell",           SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleNpcYellCommand>,             "", NULL },
        { "addtemp",        SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleTempAddSpwCommand>,          "", NULL },
        { "setlink",        SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleNpcSetLinkCommand>,          "", NULL },

        //{ @todo fix or remove this commands
        { "name",           SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleNameCommand>,                "", NULL },
        { "subname",        SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleSubNameCommand>,             "", NULL },
        { "addweapon",      SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleAddWeaponCommand>,           "", NULL },
        //}

        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand goCommandTable[] =
    {
        { "grid",           SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGoGridCommand>,              "", NULL },
        { "creature",       SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleGoCreatureCommand>,          "", NULL },
        { "object",         SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleGoObjectCommand>,            "", NULL },
        { "ticket",         SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGoTicketCommand>,            "", NULL },
        { "trigger",        SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleGoTriggerCommand>,           "", NULL },
        { "graveyard",      SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleGoGraveyardCommand>,         "", NULL },
        { "zonexy",         SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGoZoneXYCommand>,            "", NULL },
        { "xy",             SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGoXYCommand>,                "", NULL },
        { "xyz",            SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGoXYZCommand>,               "", NULL },
        { "",               SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGoXYZCommand>,               "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand gobjectCommandTable[] =
    {
        { "add",            SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleGameObjectCommand>,          "", NULL },
        { "delete",         SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleDelObjectCommand>,           "", NULL },
        { "target",         SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleTargetObjectCommand>,        "", NULL },
        { "turn",           SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleTurnObjectCommand>,          "", NULL },
        { "move",           SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleMoveObjectCommand>,          "", NULL },
        { "near",           SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleNearObjectCommand>,          "", NULL },
        { "activate",       SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleActivateObjectCommand>,      "", NULL },
        { "addtemp",        SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleTempGameObjectCommand>,      "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand questCommandTable[] =
    {
        { "add",            SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleAddQuest>,                   "", NULL },
        { "complete",       SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleCompleteQuest>,              "", NULL },
        { "remove",         SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleRemoveQuest>,                "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand gmCommandTable[] =
    {
        { "chat",           SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGMChatCommand>,              "", NULL },
        { "ingame",         SEC_PLAYER,         true,  OldHandler<&ChatHandler::HandleGMListIngameCommand>,        "", NULL },
        { "list",           SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleGMListFullCommand>,          "", NULL },
        { "visible",        SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleVisibleCommand>,             "", NULL },
        { "fly",            SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleFlyModeCommand>,             "", NULL },
        { "",               SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGMmodeCommand>,              "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand instanceCommandTable[] =
    {
        { "listbinds",      SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleInstanceListBindsCommand>,   "", NULL },
        { "unbind",         SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleInstanceUnbindCommand>,      "", NULL },
        { "stats",          SEC_MODERATOR,      true,  OldHandler<&ChatHandler::HandleInstanceStatsCommand>,       "", NULL },
        { "savedata",       SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleInstanceSaveDataCommand>,    "", NULL },
        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand ticketCommandTable[] =
    {
        { "list",           SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGMTicketListCommand>,             "", NULL },
        { "onlinelist",     SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGMTicketListOnlineCommand>,       "", NULL },
        { "viewname",       SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGMTicketGetByNameCommand>,        "", NULL },
        { "viewid",         SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGMTicketGetByIdCommand>,          "", NULL },
        { "close",          SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGMTicketCloseByIdCommand>,        "", NULL },
        { "closedlist",     SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGMTicketListClosedCommand>,       "", NULL },
        { "delete",         SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleGMTicketDeleteByIdCommand>,       "", NULL },
        { "assign",         SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGMTicketAssignToCommand>,         "", NULL },
        { "unassign",       SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGMTicketUnAssignCommand>,         "", NULL },
        { "comment",        SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGMTicketCommentCommand>,          "", NULL },
        { NULL,             0,                  false, NULL,                                                "", NULL }
    };

    static ChatCommand referFriendCommandTable[] =
    {
        { "info",           SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleRAFInfoCommand>,                  "", NULL },
        { "link",           SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleRAFLinkCommand>,                  "", NULL },
        { "unlink",         SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleRAFUnlinkCommand>,                "", NULL },
        { "summon",         SEC_PLAYER,         false, OldHandler<&ChatHandler::HandleRAFSummonCommand>,                "", NULL },
        { "grantlevel",     SEC_PLAYER,         false, OldHandler<&ChatHandler::HandleRAFGrantLevelCommand>,            "", NULL },
        { NULL,             0,                  false, NULL,                                                "", NULL }
    };

    static ChatCommand cheatCommandTable[] =
    {
        { "god",            SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleGodModeCheatCommand>,             "", NULL },
        { "casttime",       SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleCasttimeCheatCommand>,            "", NULL },
        { "cooldown",       SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleCoolDownCheatCommand>,            "", NULL },
        { "power",          SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandlePowerCheatCommand>,               "", NULL },
        { "taxicheat",      SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleTaxiCheatCommand>,                "", NULL },
        { "explorecheat",   SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleExploreCheatCommand>,             "", NULL },
        { "waterwalk",      SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleWaterwalkCheatCommand>,           "", NULL },
        { NULL,             0,                  false, NULL,                                                "", NULL }
    };

    static ChatCommand commandTable[] =
    {
        { "account",        SEC_PLAYER,         true,  NULL,                                           "", accountCommandTable  },
        { "gm",             SEC_MODERATOR,      true,  NULL,                                           "", gmCommandTable       },
        { "ticket",         SEC_MODERATOR,      false, NULL,                                           "", ticketCommandTable   },
        { "npc",            SEC_MODERATOR,      false, NULL,                                           "", npcCommandTable      },
        { "go",             SEC_MODERATOR,      false, NULL,                                           "", goCommandTable       },
        { "learn",          SEC_MODERATOR,      false, NULL,                                           "", learnCommandTable    },
        { "modify",         SEC_MODERATOR,      false, NULL,                                           "", modifyCommandTable   },
        { "debug",          SEC_MODERATOR,      false, NULL,                                           "", debugCommandTable    },
        { "tele",           SEC_MODERATOR,      true,  NULL,                                           "", teleCommandTable     },
        { "character",      SEC_GAMEMASTER,     true,  NULL,                                           "", characterCommandTable},
        { "event",          SEC_GAMEMASTER,     false, NULL,                                           "", eventCommandTable    },
        { "gobject",        SEC_GAMEMASTER,     false, NULL,                                           "", gobjectCommandTable  },
        { "honor",          SEC_GAMEMASTER,     false, NULL,                                           "", honorCommandTable    },
        { "group",          SEC_GAMEMASTER,     false, NULL,                                           "", groupCommandTable    },

        //wp commands
        { "wp",             SEC_GAMEMASTER,     false, NULL,                                           "", wpCommandTable },
        { "loadpath",       SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleReloadAllPaths>,             "", NULL },

        { "quest",          SEC_ADMINISTRATOR,  false, NULL,                                           "", questCommandTable    },
        { "titles",         SEC_GAMEMASTER,     false, NULL,                                           "", titlesCommandTable   },
        { "reload",         SEC_ADMINISTRATOR,  true,  NULL,                                           "", reloadCommandTable   },
        { "list",           SEC_ADMINISTRATOR,  true,  NULL,                                           "", listCommandTable     },
        { "lookup",         SEC_ADMINISTRATOR,  true,  NULL,                                           "", lookupCommandTable   },
        { "pdump",          SEC_ADMINISTRATOR,  true,  NULL,                                           "", pdumpCommandTable    },
        { "guild",          SEC_ADMINISTRATOR,  true,  NULL,                                           "", guildCommandTable    },
        { "cast",           SEC_ADMINISTRATOR,  false, NULL,                                           "", castCommandTable     },
        { "reset",          SEC_ADMINISTRATOR,  false, NULL,                                           "", resetCommandTable    },
        { "instance",       SEC_ADMINISTRATOR,  true,  NULL,                                           "", instanceCommandTable },
        { "server",         SEC_ADMINISTRATOR,  true,  NULL,                                           "", serverCommandTable   },
        { "pet",            SEC_GAMEMASTER,     false, NULL,                                           "", petCommandTable      },
        { "cheat",          SEC_GAMEMASTER,     false, NULL,                                           "", cheatCommandTable    },

        { "aura",           SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleAuraCommand>,                "", NULL },
        { "unaura",         SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleUnAuraCommand>,              "", NULL },
        { "nameannounce",   SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleNameAnnounceCommand>,        "", NULL },
        { "gmnameannounce", SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGMNameAnnounceCommand>,      "", NULL },
        { "announce",       SEC_MODERATOR,      true,  OldHandler<&ChatHandler::HandleAnnounceCommand>,            "", NULL },
        { "gmannounce",     SEC_MODERATOR,      true,  OldHandler<&ChatHandler::HandleGMAnnounceCommand>,          "", NULL },
        { "notify",         SEC_MODERATOR,      true,  OldHandler<&ChatHandler::HandleNotifyCommand>,              "", NULL },
        { "gmnotify",       SEC_MODERATOR,      true,  OldHandler<&ChatHandler::HandleGMNotifyCommand>,            "", NULL },
        { "appear",         SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleAppearCommand>,              "", NULL },
        { "summon",         SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleSummonCommand>,              "", NULL },
        { "groupsummon",    SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGroupSummonCommand>,         "", NULL },
        { "commands",       SEC_PLAYER,         true,  OldHandler<&ChatHandler::HandleCommandsCommand>,            "", NULL },
        { "demorph",        SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleDeMorphCommand>,             "", NULL },
        { "die",            SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleDieCommand>,                 "", NULL },
        { "revive",         SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleReviveCommand>,              "", NULL },
        { "dismount",       SEC_PLAYER,         false, OldHandler<&ChatHandler::HandleDismountCommand>,            "", NULL },
        { "gps",            SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleGPSCommand>,                 "", NULL },
        { "guid",           SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleGUIDCommand>,                "", NULL },
        { "help",           SEC_PLAYER,         true,  OldHandler<&ChatHandler::HandleHelpCommand>,                "", NULL },
        { "itemmove",       SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleItemMoveCommand>,            "", NULL },
        { "cooldown",       SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleCooldownCommand>,            "", NULL },
        { "unlearn",        SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleUnLearnCommand>,             "", NULL },
        { "distance",       SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleGetDistanceCommand>,         "", NULL },
        { "recall",         SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleRecallCommand>,              "", NULL },
        { "save",           SEC_PLAYER,         false, OldHandler<&ChatHandler::HandleSaveCommand>,                "", NULL },
        { "saveall",        SEC_MODERATOR,      true,  OldHandler<&ChatHandler::HandleSaveAllCommand>,             "", NULL },
        { "kick",           SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleKickPlayerCommand>,          "", NULL },
        { "ban",            SEC_ADMINISTRATOR,  true,  NULL,                                           "", banCommandTable },
        { "unban",          SEC_ADMINISTRATOR,  true,  NULL,                                           "", unbanCommandTable },
        { "baninfo",        SEC_ADMINISTRATOR,  false, NULL,                                           "", baninfoCommandTable },
        { "banlist",        SEC_ADMINISTRATOR,  true,  NULL,                                           "", banlistCommandTable },
        { "start",          SEC_PLAYER,         false, OldHandler<&ChatHandler::HandleStartCommand>,               "", NULL },
        { "allowmove",      SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleAllowMovementCommand>,       "", NULL },
        { "linkgrave",      SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleLinkGraveCommand>,           "", NULL },
        { "neargrave",      SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleNearGraveCommand>,           "", NULL },
        { "hover",          SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleHoverCommand>,               "", NULL },
        { "levelup",        SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleLevelUpCommand>,             "", NULL },
        { "showarea",       SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleShowAreaCommand>,            "", NULL },
        { "hidearea",       SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleHideAreaCommand>,            "", NULL },
        { "additem",        SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleAddItemCommand>,             "", NULL },
        { "additemset",     SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleAddItemSetCommand>,          "", NULL },
        { "bank",           SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleBankCommand>,                "", NULL },
        { "wchange",        SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleChangeWeather>,              "", NULL },
        { "maxskill",       SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleMaxSkillCommand>,            "", NULL },
        { "setskill",       SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleSetSkillCommand>,            "", NULL },
        { "whispers",       SEC_MODERATOR,      false, OldHandler<&ChatHandler::HandleWhispersCommand>,            "", NULL },
        { "pinfo",          SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandlePInfoCommand>,               "", NULL },
        { "respawn",        SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleRespawnCommand>,             "", NULL },
        { "senditems",      SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleSendItemsCommand>,           "", NULL },
        { "sendmail",       SEC_MODERATOR,      true,  OldHandler<&ChatHandler::HandleSendMailCommand>,            "", NULL },
        { "sendmoney",      SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleSendMoneyCommand>,           "", NULL },
        { "mute",           SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleMuteCommand>,                "", NULL },
        { "unmute",         SEC_GAMEMASTER,     true,  OldHandler<&ChatHandler::HandleUnmuteCommand>,              "", NULL },
        { "movegens",       SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleMovegensCommand>,            "", NULL },
        { "cometome",       SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleComeToMeCommand>,            "", NULL },
        { "damage",         SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleDamageCommand>,              "", NULL },
        { "combatstop",     SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleCombatStopCommand>,          "", NULL },
        { "ahbotoptions",   SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleAHBotOptionsCommand>,        "", NULL },
        { "flusharenapoints", SEC_ADMINISTRATOR, true,  OldHandler<&ChatHandler::HandleFlushArenaPointsCommand>,    "", NULL },
        { "sendmessage",    SEC_ADMINISTRATOR,  true,  OldHandler<&ChatHandler::HandleSendMessageCommand>,         "", NULL },
        { "playall",        SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandlePlayAllCommand>,             "", NULL },
        { "repairitems",    SEC_GAMEMASTER,     false, OldHandler<&ChatHandler::HandleRepairitemsCommand>,         "", NULL },
        { "freeze",         SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleFreezeCommand>,              "", NULL },
        { "unfreeze",       SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleUnFreezeCommand>,            "", NULL },
        { "listfreeze",     SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleListFreezeCommand>,          "", NULL },
        { "possess",        SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandlePossessCommand>,             "", NULL },
        { "unpossess",      SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleUnPossessCommand>,           "", NULL },
        { "bindsight",      SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleBindSightCommand>,           "", NULL },
        { "unbindsight",    SEC_ADMINISTRATOR,  false, OldHandler<&ChatHandler::HandleUnbindSightCommand>,         "", NULL },
        { "raf",            SEC_ADMINISTRATOR,  true,  NULL,                                           "", referFriendCommandTable },

        { NULL,             0,                  false, NULL,                                           "", NULL }
    };

    static ChatCommand* commandTableCache = 0;

    if (load_command_table)
    {
        load_command_table = false;

        {
            // count total number of top-level commands
            size_t total = getCommandTableSize(commandTable);
            std::vector<ChatCommand*> const& dynamic = sScriptMgr.GetChatCommands();
            for (std::vector<ChatCommand*>::const_iterator it = dynamic.begin(); it != dynamic.end(); ++it)
                total += getCommandTableSize(*it);
            total += 1; // ending zero

            // cache top-level commands
            commandTableCache = (ChatCommand*)malloc(sizeof(ChatCommand) * total);
            memset(commandTableCache, 0, sizeof(ChatCommand) * total);
            ACE_ASSERT(commandTableCache);
            size_t added = appendCommandTable(commandTableCache, commandTable);
            for (std::vector<ChatCommand*>::const_iterator it = dynamic.begin(); it != dynamic.end(); ++it)
                added += appendCommandTable(commandTableCache + added, *it);
        }

        QueryResult_AutoPtr result = WorldDatabase.Query("SELECT name,security,help FROM command");
        if (result)
        {
            do
            {
                Field *fields = result->Fetch();
                std::string name = fields[0].GetString();

                SetDataForCommandInTable(commandTableCache, name.c_str(), fields[1].GetUInt16(), fields[2].GetString(), name);

            } while (result->NextRow());
        }
    }

    return commandTableCache;
}

const char* ChatHandler::GetOregonString(int32 entry) const
{
    return m_session->GetOregonString(entry);
}

bool ChatHandler::isAvailable(ChatCommand const& cmd) const
{
    // check security level only for simple  command (without child commands)
    return m_session->GetSecurity() >= cmd.SecurityLevel;
}

bool ChatHandler::hasStringAbbr(const char* name, const char* part)
{
    // non "" command
    if (*name)
    {
        // "" part from non-"" command
        if (!*part)
            return false;

        for (;;)
        {
            if (!*part)
                return true;
            else if (!*name)
                return false;
            else if (tolower(*name) != tolower(*part))
                return false;
            ++name;
            ++part;
        }
    }
    // allow with any for ""

    return true;
}

void ChatHandler::SendSysMessage(const char* str)
{
    WorldPacket data;

    // need copy to prevent corruption by strtok call in LineFromMessage original string
    char* buf = strdup(str);
    char* pos = buf;

    while (char* line = LineFromMessage(pos))
    {
        FillSystemMessageData(&data, line);
        m_session->SendPacket(&data);
    }

    free(buf);
}

void ChatHandler::SendGlobalSysMessage(const char* str)
{
    // Chat output
    WorldPacket data;

    // need copy to prevent corruption by strtok call in LineFromMessage original string
    char* buf = strdup(str);
    char* pos = buf;

    while (char* line = LineFromMessage(pos))
    {
        FillSystemMessageData(&data, line);
        sWorld.SendGlobalMessage(&data);
    }

    free(buf);
}

void ChatHandler::SendGlobalGMSysMessage(const char* str)
{
    // Chat output
    WorldPacket data;

    // need copy to prevent corruption by strtok call in LineFromMessage original string
    char* buf = strdup(str);
    char* pos = buf;

    while (char* line = LineFromMessage(pos))
    {
        FillSystemMessageData(&data, line);
        sWorld.SendGlobalGMMessage(&data);
    }
    free(buf);
}

void ChatHandler::SendSysMessage(int32 entry)
{
    SendSysMessage(GetOregonString(entry));
}

void ChatHandler::PSendSysMessage(int32 entry, ...)
{
    const char* format = GetOregonString(entry);
    va_list ap;
    char str[1024];
    va_start(ap, entry);
    vsnprintf(str, 1024, format, ap);
    va_end(ap);
    SendSysMessage(str);
}

void ChatHandler::PSendSysMessage(const char* format, ...)
{
    va_list ap;
    char str[1024];
    va_start(ap, format);
    vsnprintf(str, 1024, format, ap);
    va_end(ap);
    SendSysMessage(str);
}

bool ChatHandler::ExecuteCommandInTables(std::vector<ChatCommand*>& tables, const char* text, const std::string& fullcmd)
{
    for (std::vector<ChatCommand*>::iterator it = tables.begin(); it != tables.end(); ++it)
        if (ExecuteCommandInTable((*it), text, fullcmd))
            return true;

    return false;
}

bool ChatHandler::ExecuteCommandInTable(ChatCommand* table, const char* text, const std::string& fullcmd)
{
    char const* oldtext = text;
    std::string cmd = "";

    while (*text != ' ' && *text != '\0')
    {
        cmd += *text;
        ++text;
    }

    while (*text == ' ') ++text;

    for (uint32 i = 0; table[i].Name != NULL; ++i)
    {
        if (!hasStringAbbr(table[i].Name, cmd.c_str()))
            continue;

        // select subcommand from child commands list
        if (table[i].ChildCommands != NULL)
        {
            if (!ExecuteCommandInTable(table[i].ChildCommands, text, fullcmd))
            {
                if (!sEluna->OnCommand(m_session ? m_session->GetPlayer() : NULL, fullcmd.c_str()))
                    return true;

                if (text && text[0] != '\0')
                    SendSysMessage(LANG_NO_SUBCMD);
                else
                    SendSysMessage(LANG_CMD_SYNTAX);

                ShowHelpForCommand(table[i].ChildCommands, text);
            }

            return true;
        }

        // must be available and have handler
        if (!table[i].Handler || !isAvailable(table[i]))
            continue;

        SetSentErrorMessage(false);
        // table[i].Name == "" is special case: send original command to handler
        if ((table[i].Handler)(this, strlen(table[i].Name) != 0 ? text : oldtext))
        {
            if (table[i].SecurityLevel > SEC_PLAYER)
            {
                // chat case
                if (m_session)
                {
                    Player* p = m_session->GetPlayer();
                    ObjectGuid sel_guid = p->GetSelection();
                    sLog.outCommand(m_session->GetAccountId(), "Command: %s [Player: %s (Account: %u) X: %f Y: %f Z: %f Map: %u Selected: %s]",
                        fullcmd.c_str(), p->GetName(), m_session->GetAccountId(), p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(), p->GetMapId(),
                        sel_guid.GetString().c_str());
                }
            }
        }
        // some commands have custom error messages. Don't send the default one in these cases.
        else if (!sentErrorMessage)
        {
            if (!table[i].Help.empty())
                SendSysMessage(table[i].Help.c_str());
            else
                SendSysMessage(LANG_CMD_SYNTAX);
        }

        return true;
    }

    return false;
}

bool ChatHandler::SetDataForCommandInTable(ChatCommand *table, const char* text, uint32 security, std::string const& help, std::string const& fullcommand)
{
    std::string cmd = "";

    while (*text != ' ' && *text != '\0')
    {
        cmd += *text;
        ++text;
    }

    while (*text == ' ') ++text;

    for (uint32 i = 0; table[i].Name != NULL; i++)
    {
        // for data fill use full explicit command names
        if (table[i].Name != cmd)
            continue;

        // select subcommand from child commands list (including "")
        if (table[i].ChildCommands != NULL)
        {
            if (SetDataForCommandInTable(table[i].ChildCommands, text, security, help, fullcommand))
                return true;
            else if (*text)
                return false;

            // fail with "" subcommands, then use normal level up command instead
        }
        // expected subcommand by full name DB content
        else if (*text)
        {
            sLog.outErrorDb("Table `command` have unexpected subcommand '%s' in command '%s', skip.", text, fullcommand.c_str());
            return false;
        }

        if (table[i].SecurityLevel != security)
            sLog.outDetail("Table `command` overwrite for command '%s' default security (%u) by %u", fullcommand.c_str(), table[i].SecurityLevel, security);

        table[i].SecurityLevel = security;
        table[i].Help = help;
        return true;
    }

    // in case "" command let process by caller
    if (!cmd.empty())
    {
        if (table == getCommandTable())
            sLog.outErrorDb("Table `command` have not existed command '%s', skip.", cmd.c_str());
        else
            sLog.outErrorDb("Table `command` have not existed subcommand '%s' in command '%s', skip.", cmd.c_str(), fullcommand.c_str());
    }

    return false;
}

int ChatHandler::ParseCommands(const char* text)
{
    ASSERT(text);
    ASSERT(*text);

    std::string fullcmd = text;

    // chat case (.command or !command format)
    if (m_session)
    {
        if (text[0] != '!' && text[0] != '.')
            return 0;
    }

    // ignore single . and ! in line
    if (strlen(text) < 2)
        return 0;
    // original `text` can't be used. It content destroyed in command code processing.

    // ignore messages staring from many dots.
    if ((text[0] == '.' && text[1] == '.') || (text[0] == '!' && text[1] == '!'))
        return 0;

    // skip first . or ! (in console allowed use command with . and ! and without its)
    if (text[0] == '!' || text[0] == '.')
        ++text;

    std::vector<ChatCommand*> table = sScriptMgr.GetChatCommands();

    if (!ExecuteCommandInTable(getCommandTable(), text, fullcmd))
    {
        std::vector<ChatCommand*> table = sScriptMgr.GetChatCommands();

        if (m_session && m_session->GetSecurity() == SEC_PLAYER)
            return 0;

        SendSysMessage(LANG_NO_CMD);
    }
    return 1;
}

bool ChatHandler::isValidChatMessage(const char* message)
{
    /*

    valid examples:
    |cffa335ee|Hitem:812:0:0:0:0:0:0:0:70|h[Glowing Brightwood Staff]|h|r
    |cff808080|Hquest:2278:47|h[The Platinum Discs]|h|r
    |cff4e96f7|Htalent:2232:-1|h[Taste for Blood]|h|r
    |cff71d5ff|Hspell:21563|h[Command]|h|r
    |cffffd000|Henchant:3919|h[Engineering: Rough Dynamite]|h|r

    | will be escaped to ||
    */

    if (strlen(message) > 255)
        return false;

    const char validSequence[6] = "cHhhr";
    const char* validSequenceIterator = validSequence;

    // more simple checks
    if (sWorld.getConfig(CONFIG_CHAT_STRICT_LINK_CHECKING_SEVERITY) < 3)
    {
        const std::string validCommands = "cHhr|";

        while (*message)
        {
            // find next pipe command
            message = strchr(message, '|');

            if (!message)
                return true;

            ++message;
            char commandChar = *message;
            if (validCommands.find(commandChar) == std::string::npos)
                return false;

            ++message;
            // validate sequence
            if (sWorld.getConfig(CONFIG_CHAT_STRICT_LINK_CHECKING_SEVERITY) == 2)
            {
                if (commandChar == *validSequenceIterator)
                {
                    if (validSequenceIterator == validSequence + 4)
                        validSequenceIterator = validSequence;
                    else
                        ++validSequenceIterator;
                }
                else
                    return false;
            }
        }
        return true;
    }

    std::istringstream reader(message);
    char buffer[256];

    uint32 color = 0;

    ItemTemplate const* linkedItem;
    Quest const* linkedQuest;
    SpellEntry const* linkedSpell = NULL;

    while (!reader.eof())
    {
        if (validSequence == validSequenceIterator)
        {
            linkedItem = NULL;
            linkedQuest = NULL;
            linkedSpell = NULL;

            reader.ignore(255, '|');
        }
        else if (reader.get() != '|')
        {
#ifdef OREGON_DEBUG
            sLog.outBasic("ChatHandler::isValidChatMessage sequence aborted unexpectedly");
#endif
            return false;
        }

        // pipe has always to be followed by at least one char
        if (reader.peek() == '\0')
        {
#ifdef OREGON_DEBUG
            sLog.outBasic("ChatHandler::isValidChatMessage pipe followed by \\0");
#endif
            return false;
        }

        // no further pipe commands
        if (reader.eof())
            break;

        char commandChar;
        reader >> commandChar;

        // | in normal messages is escaped by ||
        if (commandChar != '|')
        {
            if (commandChar == *validSequenceIterator)
            {
                if (validSequenceIterator == validSequence + 4)
                    validSequenceIterator = validSequence;
                else
                    ++validSequenceIterator;
            }
            else
            {
#ifdef OREGON_DEBUG
                sLog.outBasic("ChatHandler::isValidChatMessage invalid sequence, expected %c but got %c", *validSequenceIterator, commandChar);
#endif
                return false;
            }
            }
        else if (validSequence != validSequenceIterator)
        {
            // no escaped pipes in sequences
#ifdef OREGON_DEBUG
            sLog.outBasic("ChatHandler::isValidChatMessage got escaped pipe in sequence");
#endif
            return false;
        }

        switch (commandChar)
        {
        case 'c':
            color = 0;
            // validate color, expect 8 hex chars
            for (int i = 0; i < 8; i++)
            {
                char c;
                reader >> c;
                if (!c)
                {
#ifdef OREGON_DEBUG
                    sLog.outBasic("ChatHandler::isValidChatMessage got \\0 while reading color in |c command");
#endif
                    return false;
                }

                color <<= 4;
                // check for hex char
                if (c >= '0' && c <= '9')
                {
                    color |= c - '0';
                    continue;
                }
                if (c >= 'a' && c <= 'f')
                {
                    color |= 10 + c - 'a';
                    continue;
                }
#ifdef OREGON_DEBUG
                sLog.outBasic("ChatHandler::isValidChatMessage got non hex char '%c' while reading color", c);
#endif
                return false;
                }
            break;
        case 'H':
            // read chars up to colon  = link type
            reader.getline(buffer, 256, ':');

            if (strcmp(buffer, "item") == 0)
            {
                // read item entry
                reader.getline(buffer, 256, ':');

                linkedItem = sObjectMgr.GetItemTemplate(atoi(buffer));
                if (!linkedItem)
                {
#ifdef OREGON_DEBUG
                    sLog.outBasic("ChatHandler::isValidChatMessage got invalid itemID %u in |item command", atoi(buffer));
#endif
                    return false;
                }

                if (color != ItemQualityColors[linkedItem->Quality])
                {
#ifdef OREGON_DEBUG
                    sLog.outBasic("ChatHandler::isValidChatMessage linked item has color %u, but user claims %u", ItemQualityColors[linkedItem->Quality],
                        color);
#endif
                    return false;
                }

                char c = reader.peek();

                // ignore enchants etc.
                while ((c >= '0' && c <= '9') || c == ':')
                {
                    reader.ignore(1);
                    c = reader.peek();
                }
                }
            else if (strcmp(buffer, "quest") == 0)
            {
                // no color check for questlinks, each client will adapt it anyway
                uint32 questid = 0;
                // read questid
                char c = reader.peek();
                while (c >= '0' && c <= '9')
                {
                    reader.ignore(1);
                    questid *= 10;
                    questid += c - '0';
                    c = reader.peek();
                }

                linkedQuest = sObjectMgr.GetQuestTemplate(questid);

                if (!linkedQuest)
                {
#ifdef OREGON_DEBUG
                    sLog.outBasic("ChatHandler::isValidChatMessage Questtemplate %u not found", questid);
#endif
                    return false;
                }
                c = reader.peek();
                // level
                while (c != '|' && c != '\0')
                {
                    reader.ignore(1);
                    c = reader.peek();
                }
                }
            else if (strcmp(buffer, "talent") == 0)
            {
                // talent links are always supposed to be blue
                if (color != CHAT_LINK_COLOR_TALENT)
                    return false;

                // read talent entry
                reader.getline(buffer, 256, ':');
                TalentEntry const* talentInfo = sTalentStore.LookupEntry(atoi(buffer));
                if (!talentInfo)
                    return false;

                linkedSpell = sSpellStore.LookupEntry(talentInfo->RankID[0]);
                if (!linkedSpell)
                    return false;

                char c = reader.peek();
                // skillpoints? whatever, drop it
                while (c != '|' && c != '\0')
                {
                    reader.ignore(1);
                    c = reader.peek();
                }
            }
            else if (strcmp(buffer, "spell") == 0)
            {
                if (color != CHAT_LINK_COLOR_SPELL)
                    return false;

                uint32 spellid = 0;
                // read spell entry
                char c = reader.peek();
                while (c >= '0' && c <= '9')
                {
                    reader.ignore(1);
                    spellid *= 10;
                    spellid += c - '0';
                    c = reader.peek();
                }
                linkedSpell = sSpellStore.LookupEntry(spellid);
                if (!linkedSpell)
                    return false;
            }
            else if (strcmp(buffer, "enchant") == 0)
            {
                if (color != CHAT_LINK_COLOR_ENCHANT)
                    return false;

                uint32 spellid = 0;
                // read spell entry
                char c = reader.peek();
                while (c >= '0' && c <= '9')
                {
                    reader.ignore(1);
                    spellid *= 10;
                    spellid += c - '0';
                    c = reader.peek();
                }
                linkedSpell = sSpellStore.LookupEntry(spellid);
                if (!linkedSpell)
                    return false;
            }
            else
            {
#ifdef OREGON_DEBUG
                sLog.outBasic("ChatHandler::isValidChatMessage user sent unsupported link type '%s'", buffer);
#endif
                return false;
            }
            break;
        case 'h':
            // if h is next element in sequence, this one must contain the linked text :)
            if (*validSequenceIterator == 'h')
            {
                // links start with '['
                if (reader.get() != '[')
                {
#ifdef OREGON_DEBUG
                    sLog.outBasic("ChatHandler::isValidChatMessage link caption doesn't start with '['");
#endif
                    return false;
                }
                reader.getline(buffer, 256, ']');

                // verify the link name
                if (linkedSpell)
                {
                    // spells with that flag have a prefix of "$PROFESSION: "
                    if (linkedSpell->Attributes & SPELL_ATTR0_TRADESPELL)
                    {
                        // lookup skillid
                        SkillLineAbilityMap::const_iterator lower = sSpellMgr.GetBeginSkillLineAbilityMap(linkedSpell->Id);
                        SkillLineAbilityMap::const_iterator upper = sSpellMgr.GetEndSkillLineAbilityMap(linkedSpell->Id);

                        if (lower == upper)
                            return false;

                        SkillLineAbilityEntry const* skillInfo = lower->second;

                        if (!skillInfo)
                            return false;

                        SkillLineEntry const* skillLine = sSkillLineStore.LookupEntry(skillInfo->skillId);
                        if (!skillLine)
                            return false;

                        for (uint8 i = 0; i < MAX_LOCALE; ++i)
                        {
                            uint32 skillLineNameLength = strlen(skillLine->name[i]);
                            if (skillLineNameLength > 0 && strncmp(skillLine->name[i], buffer, skillLineNameLength) == 0)
                            {
                                // found the prefix, remove it to perform spellname validation below
                                // -2 = strlen(": ")
                                uint32 spellNameLength = strlen(buffer) - skillLineNameLength - 2;
                                memmove(buffer, buffer + skillLineNameLength + 2, spellNameLength + 1);
                            }
                        }
                    }
                    bool foundName = false;
                    for (uint8 i = 0; i < MAX_LOCALE; ++i)
                    {
                        if (*linkedSpell->SpellName[i] && strcmp(linkedSpell->SpellName[i], buffer) == 0)
                        {
                            foundName = true;
                            break;
                        }
                    }
                    if (!foundName)
                        return false;
                }
                else if (linkedQuest)
                {
                    if (linkedQuest->GetTitle() != buffer)
                    {
                        QuestLocale const* ql = sObjectMgr.GetQuestLocale(linkedQuest->GetQuestId());

                        if (!ql)
                        {
#ifdef OREGON_DEBUG
                            sLog.outBasic("ChatHandler::isValidChatMessage default questname didn't match and there is no locale");
#endif
                            return false;
                        }

                        bool foundName = false;
                        for (uint8 i = 0; i < ql->Title.size(); i++)
                        {
                            if (ql->Title[i] == buffer)
                            {
                                foundName = true;
                                break;
                            }
                        }
                        if (!foundName)
                        {
#ifdef OREGON_DEBUG
                            sLog.outBasic("ChatHandler::isValidChatMessage no quest locale title matched");
#endif
                            return false;
                        }
                }
            }
                else if (linkedItem)
                {
                    if (strcmp(linkedItem->Name1, buffer) != 0)
                    {
                        ItemLocale const* il = sObjectMgr.GetItemLocale(linkedItem->ItemId);

                        if (!il)
                        {
#ifdef OREGON_DEBUG
                            sLog.outBasic("ChatHandler::isValidChatMessage linked item name doesn't is wrong and there is no localization");
#endif
                            return false;
                        }

                        bool foundName = false;
                        for (uint8 i = 0; i < il->Name.size(); ++i)
                        {
                            if (il->Name[i] == buffer)
                            {
                                foundName = true;
                                break;
                            }
                        }
                        if (!foundName)
                        {
#ifdef OREGON_DEBUG
                            sLog.outBasic("ChatHandler::isValidChatMessage linked item name wasn't found in any localization");
#endif
                            return false;
                        }
                }
            }
                // that place should never be reached - if nothing linked has been set in |H
                // it will return false before
                else
                    return false;
            }
            break;
        case 'r':
        case '|':
            // no further payload
            break;
        default:
#ifdef OREGON_DEBUG
            sLog.outBasic("ChatHandler::isValidChatMessage got invalid command |%c", commandChar);
#endif
            return false;
        }
        }

    // check if every opened sequence was also closed properly
#ifdef OREGON_DEBUG
    if (validSequence != validSequenceIterator)
        sLog.outBasic("ChatHandler::isValidChatMessage EOF in active sequence");
#endif
    return validSequence == validSequenceIterator;
        }

bool ChatHandler::ShowHelpForSubCommands(ChatCommand* table, char const* cmd, char const* subcmd)
{
    std::string list;
    for (uint32 i = 0; table[i].Name != NULL; ++i)
    {
        // must be available (ignore handler existence for show command with possibe avalable subcomands
        if (!isAvailable(table[i]))
            continue;

        // for empty subcmd show all available
        if (*subcmd && !hasStringAbbr(table[i].Name, subcmd))
            continue;

        if (m_session)
            list += "\n    ";
        else
            list += "\n\r    ";

        list += table[i].Name;

        if (table[i].ChildCommands)
            list += " ...";
    }

    if (list.empty())
        return false;

    if (table == getCommandTable())
    {
        SendSysMessage(LANG_AVIABLE_CMD);
        PSendSysMessage("%s", list.c_str());
    }
    else
        PSendSysMessage(LANG_SUBCMDS_LIST, cmd, list.c_str());

    return true;
}

bool ChatHandler::ShowHelpForCommand(ChatCommand* table, const char* cmd)
{
    if (*cmd)
    {
        for (uint32 i = 0; table[i].Name != NULL; ++i)
        {
            // must be available (ignore handler existence for show command with possibe avalable subcomands
            if (!isAvailable(table[i]))
                continue;

            if (!hasStringAbbr(table[i].Name, cmd))
                continue;

            // have subcommand
            char const* subcmd = (*cmd) ? strtok(NULL, " ") : "";

            if (table[i].ChildCommands && subcmd && *subcmd)
            {
                if (ShowHelpForCommand(table[i].ChildCommands, subcmd))
                    return true;
            }

            if (!table[i].Help.empty())
                SendSysMessage(table[i].Help.c_str());

            if (table[i].ChildCommands)
                if (ShowHelpForSubCommands(table[i].ChildCommands, table[i].Name, subcmd ? subcmd : ""))
                    return true;

            return !table[i].Help.empty();
        }
    }
    else
    {
        for (uint32 i = 0; table[i].Name != NULL; ++i)
        {
            // must be available (ignore handler existence for show command with possibe avalable subcomands
            if (!isAvailable(table[i]))
                continue;

            if (strlen(table[i].Name))
                continue;

            if (!table[i].Help.empty())
                SendSysMessage(table[i].Help.c_str());

            if (table[i].ChildCommands)
                if (ShowHelpForSubCommands(table[i].ChildCommands, "", ""))
                    return true;

            return !table[i].Help.empty();
        }
    }

    return ShowHelpForSubCommands(table, "", cmd);
}

//Note: target_guid used only in CHAT_MSG_WHISPER_INFORM mode (in this case channelName ignored)
void ChatHandler::FillMessageData(WorldPacket* data, WorldSession* session, uint8 type, uint32 language, const char* channelName, uint64 target_guid, const char* message, Unit* speaker)
{
    uint32 messageLength = (message ? strlen(message) : 0) + 1;

    data->Initialize(SMSG_MESSAGECHAT, 100);                // guess size
    *data << uint8(type);
    if ((type != CHAT_MSG_CHANNEL && type != CHAT_MSG_WHISPER) || language == LANG_ADDON)
        *data << uint32(language);
    else
        *data << uint32(LANG_UNIVERSAL);

    switch (type)
    {
    case CHAT_MSG_SAY:
    case CHAT_MSG_PARTY:
    case CHAT_MSG_RAID:
    case CHAT_MSG_GUILD:
    case CHAT_MSG_OFFICER:
    case CHAT_MSG_YELL:
    case CHAT_MSG_WHISPER:
    case CHAT_MSG_CHANNEL:
    case CHAT_MSG_RAID_LEADER:
    case CHAT_MSG_RAID_WARNING:
    case CHAT_MSG_BG_SYSTEM_NEUTRAL:
    case CHAT_MSG_BG_SYSTEM_ALLIANCE:
    case CHAT_MSG_BG_SYSTEM_HORDE:
    case CHAT_MSG_BATTLEGROUND:
    case CHAT_MSG_BATTLEGROUND_LEADER:
        target_guid = session ? session->GetPlayer()->GetGUID() : 0;
        break;
    case CHAT_MSG_MONSTER_SAY:
    case CHAT_MSG_MONSTER_PARTY:
    case CHAT_MSG_MONSTER_YELL:
    case CHAT_MSG_MONSTER_WHISPER:
    case CHAT_MSG_MONSTER_EMOTE:
    case CHAT_MSG_RAID_BOSS_WHISPER:
    case CHAT_MSG_RAID_BOSS_EMOTE:
    {
        *data << uint64(speaker->GetGUID());
        *data << uint32(0);                             // 2.1.0
        *data << uint32(strlen(speaker->GetName()) + 1);
        *data << speaker->GetName();
        uint64 listener_guid = 0;
        *data << uint64(listener_guid);
        if (listener_guid && !IS_PLAYER_GUID(listener_guid))
        {
            *data << uint32(1);                         // string listener_name_length
            *data << uint8(0);                          // string listener_name
        }
        *data << uint32(messageLength);
        *data << message;
        *data << uint8(0);
        return;
    }
    default:
        if (type != CHAT_MSG_REPLY && type != CHAT_MSG_IGNORED && type != CHAT_MSG_DND && type != CHAT_MSG_AFK)
            target_guid = 0;                            // only for CHAT_MSG_WHISPER_INFORM used original value target_guid
        break;
    }

    *data << uint64(target_guid);                           // there 0 for BG messages
    *data << uint32(0);                                     // can be chat msg group or something

    if (type == CHAT_MSG_CHANNEL)
    {
        ASSERT(channelName);
        *data << channelName;
    }

    *data << uint64(target_guid);
    *data << uint32(messageLength);
    *data << message;
    if (session != 0 && type != CHAT_MSG_REPLY && type != CHAT_MSG_DND && type != CHAT_MSG_AFK)
        *data << uint8(session->GetPlayer()->GetChatTag());
    else
        *data << uint8(0);
}

Player* ChatHandler::getSelectedPlayer()
{
    if (!m_session)
        return NULL;

    uint64 guid = m_session->GetPlayer()->GetSelection();

    if (guid == 0)
        return m_session->GetPlayer();

    return sObjectMgr.GetPlayer(guid);
}

Player* ChatHandler::getSelectedPlayerOrSelf()
{
    if (!m_session)
        return NULL;

    uint64 selected = m_session->GetPlayer()->GetTarget();
    if (!selected)
        return m_session->GetPlayer();

    // first try with selected target
    Player* targetPlayer = ObjectAccessor::FindPlayer(selected);
    // if the target is not a player, then return self
    if (!targetPlayer)
        targetPlayer = m_session->GetPlayer();

    return targetPlayer;
}

Unit* ChatHandler::getSelectedUnit()
{
    if (!m_session)
        return NULL;

    uint64 guid = m_session->GetPlayer()->GetSelection();

    if (guid == 0)
        return m_session->GetPlayer();

    return ObjectAccessor::GetUnit(*m_session->GetPlayer(), guid);
}

Creature* ChatHandler::getSelectedCreature()
{
    if (!m_session)
        return NULL;

    return ObjectAccessor::GetCreatureOrPet(*m_session->GetPlayer(), m_session->GetPlayer()->GetSelection());
}

char* ChatHandler::extractKeyFromLink(char* text, char const* linkType, char** something1)
{
    // skip empty
    if (!text)
        return NULL;

    // skip spaces
    while (*text == ' ' || *text == '\t' || *text == '\b')
        ++text;

    if (!*text)
        return NULL;

    // return non link case
    if (text[0] != '|')
        return strtok(text, " ");

    // [name] Shift-click form |color|linkType:key|h[name]|h|r
    // or
    // [name] Shift-click form |color|linkType:key:something1:...:somethingN|h[name]|h|r

    char* check = strtok(text, "|");                        // skip color
    if (!check)
        return NULL;                                        // end of data

    char* cLinkType = strtok(NULL, ":");                    // linktype
    if (!cLinkType)
        return NULL;                                        // end of data

    if (strcmp(cLinkType, linkType) != 0)
    {
        strtok(NULL, " ");                                  // skip link tail (to allow continue strtok(NULL,s) use after retturn from function
        SendSysMessage(LANG_WRONG_LINK_TYPE);
        return NULL;
    }

    char* cKeys = strtok(NULL, "|");                        // extract keys and values
    char* cKeysTail = strtok(NULL, "");

    char* cKey = strtok(cKeys, ":|");                       // extract key
    if (something1)
        *something1 = strtok(NULL, ":|");                   // extract something

    strtok(cKeysTail, "]");                                 // restart scan tail and skip name with possible spaces
    strtok(NULL, " ");                                      // skip link tail (to allow continue strtok(NULL,s) use after return from function
    return cKey;
}

char* ChatHandler::extractKeyFromLink(char* text, char const* const* linkTypes, int* found_idx, char** something1)
{
    // skip empty
    if (!text)
        return NULL;

    // skip spaces
    while (*text == ' ' || *text == '\t' || *text == '\b')
        ++text;

    if (!*text)
        return NULL;

    // return non link case
    if (text[0] != '|')
        return strtok(text, " ");

    // [name] Shift-click form |color|linkType:key|h[name]|h|r
    // or
    // [name] Shift-click form |color|linkType:key:something1:...:somethingN|h[name]|h|r

    char* check = strtok(text, "|");                        // skip color
    if (!check)
        return NULL;                                        // end of data

    char* cLinkType = strtok(NULL, ":");                    // linktype
    if (!cLinkType)
        return NULL;                                        // end of data

    for (int i = 0; linkTypes[i]; ++i)
    {
        if (strcmp(cLinkType, linkTypes[i]) == 0)
        {
            char* cKeys = strtok(NULL, "|");                // extract keys and values
            char* cKeysTail = strtok(NULL, "");

            char* cKey = strtok(cKeys, ":|");               // extract key
            if (something1)
                *something1 = strtok(NULL, ":|");           // extract something

            strtok(cKeysTail, "]");                         // restart scan tail and skip name with possible spaces
            strtok(NULL, " ");                              // skip link tail (to allow continue strtok(NULL,s) use after return from function
            if (found_idx)
                *found_idx = i;
            return cKey;
        }
    }

    strtok(NULL, " ");                                      // skip link tail (to allow continue strtok(NULL,s) use after return from function
    SendSysMessage(LANG_WRONG_LINK_TYPE);
    return NULL;
}

char const* fmtstring(char const* format, ...)
{
    va_list        argptr;
#define    MAX_FMT_STRING    32000
    static char        temp_buffer[MAX_FMT_STRING];
    static char        string[MAX_FMT_STRING];
    static int        index = 0;
    char*    buf;
    int len;

    va_start(argptr, format);
    vsnprintf(temp_buffer, MAX_FMT_STRING, format, argptr);
    va_end(argptr);

    len = strlen(temp_buffer);

    if (len >= MAX_FMT_STRING)
        return "ERROR";

    if (len + index >= MAX_FMT_STRING - 1)
        index = 0;

    buf = &string[index];
    memcpy(buf, temp_buffer, len + 1);

    index += len + 1;

    return buf;
}

GameObject* ChatHandler::GetObjectGlobalyWithGuidOrNearWithDbGuid(uint32 lowguid, uint32 entry)
{
    if (!m_session)
        return NULL;

    Player* pl = m_session->GetPlayer();

    GameObject* obj = pl->GetMap()->GetGameObject(MAKE_NEW_GUID(lowguid, entry, HIGHGUID_GAMEOBJECT));

    if (!obj && sObjectMgr.GetGOData(lowguid))                   // guid is DB guid of object
    {
        // search near player then
        CellCoord p(Oregon::ComputeCellCoord(pl->GetPositionX(), pl->GetPositionY()));
        Cell cell(p);

        Oregon::GameObjectWithDbGUIDCheck go_check(*pl, lowguid);
        Oregon::GameObjectSearcher<Oregon::GameObjectWithDbGUIDCheck> checker(obj, go_check);

        TypeContainerVisitor<Oregon::GameObjectSearcher<Oregon::GameObjectWithDbGUIDCheck>, GridTypeMapContainer > object_checker(checker);
        cell.Visit(p, object_checker, *pl->GetMap(), *pl, pl->GetGridActivationRange());
    }

    return obj;
}

static char const* const spellTalentKeys[] =
{
    "Hspell",
    "Htalent",
    0
};

uint32 ChatHandler::extractSpellIdFromLink(char* text)
{
    // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r
    // number or [name] Shift-click form |color|Htalent:talent_id,rank|h[name]|h|r
    int type = 0;
    char* rankS = NULL;
    char* idS = extractKeyFromLink(text, spellTalentKeys, &type, &rankS);
    if (!idS)
        return 0;

    uint32 id = (uint32)atol(idS);

    // spell
    if (type == 0)
        return id;

    // talent
    TalentEntry const* talentEntry = sTalentStore.LookupEntry(id);
    if (!talentEntry)
        return 0;

    int32 rank = rankS ? (uint32)atol(rankS) : 0;
    if (rank >= 5)
        return 0;

    if (rank < 0)
        rank = 0;

    return talentEntry->RankID[rank];
}

GameTele const* ChatHandler::extractGameTeleFromLink(char* text)
{
    // id, or string, or [name] Shift-click form |color|Htele:id|h[name]|h|r
    char* cId = extractKeyFromLink(text, "Htele");
    if (!cId)
        return NULL;

    // id case (explicit or from shift link)
    if (cId[0] >= '0' || cId[0] >= '9')
        if (uint32 id = atoi(cId))
            return sObjectMgr.GetGameTele(id);

    return sObjectMgr.GetGameTele(cId);
}

char* ChatHandler::extractQuotedArg(char* args)
{
    if (!args || !*args)
        return NULL;

    if (*args == '"')
        return strtok(args + 1, "\"");
    else
    {
        char* space = strtok(args, "\"");
        if (!space)
            return NULL;
        return strtok(NULL, "\"");
    }
}

const char* ChatHandler::GetName() const
{
    return m_session->GetPlayer()->GetName();
}

bool ChatHandler::needReportToTarget(Player* chr) const
{
    Player* pl = m_session->GetPlayer();
    return pl != chr && pl->IsVisibleGloballyFor(chr);
}

const char* CliHandler::GetOregonString(int32 entry) const
{
    return sObjectMgr.GetOregonStringForDBCLocale(entry);
}

bool CliHandler::isAvailable(ChatCommand const& cmd) const
{
    // skip non-console commands in console case
    return cmd.AllowConsole;
}

void CliHandler::SendSysMessage(const char* str)
{
    m_print(m_callbackArg, str);
    m_print(m_callbackArg, "\n");
}

const char* CliHandler::GetName() const
{
    return GetOregonString(LANG_CONSOLE_COMMAND);
}

bool CliHandler::needReportToTarget(Player* /*chr*/) const
{
    return true;
}

bool ChatHandler::GetPlayerGroupAndGUIDByName(const char* cname, Player*& plr, Group*& group, uint64& guid, bool offline)
{
    plr = NULL;
    guid = 0;

    if (cname)
    {
        std::string name = cname;
        if (!name.empty())
        {
            if (!normalizePlayerName(name))
            {
                PSendSysMessage(LANG_PLAYER_NOT_FOUND);
                SetSentErrorMessage(true);
                return false;
            }

            plr = sObjectMgr.GetPlayer(name.c_str());
            if (offline)
                guid = sObjectMgr.GetPlayerGUIDByName(name.c_str());
        }
    }

    if (plr)
    {
        group = plr->GetGroup();
        if (!guid || !offline)
            guid = plr->GetGUID();
    }
    else
    {
        if (getSelectedPlayer())
            plr = getSelectedPlayer();
        else
            plr = m_session->GetPlayer();

        if (!guid || !offline)
            guid = plr->GetGUID();
        group = plr->GetGroup();
    }

    return true;
}

