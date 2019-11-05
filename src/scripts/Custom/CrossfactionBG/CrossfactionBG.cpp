#include "CrossfactionBG.h"
#include "ObjectMgr.h"
#include "World.h"
#include "Battleground.h"
#include "Chat.h"
#include "Object.h"
#include "ObjectMgr.h"
#include "ScriptMgr.h"
#include "Map.h"
#include "Group.h"
#include "SpellMgr.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "Utilities/DataMap.h"
#include "Spell.h"

class CFBGDPlayerInfo : public DataMap::Base
{
public:
    CFBGDPlayerInfo() {}

    CFBGDPlayerInfo(uint8 m_fRace, uint8 m_oRace, uint32 m_fFaction, uint32 m_oFaction) : m_fRace(m_fRace), m_oRace(m_oRace), m_fFaction(m_fFaction), m_oFaction(m_oFaction) {}
    uint8 m_fRace; uint8 m_oRace; uint32 m_fFaction; uint32 m_oFaction;

    uint32 getFFaction() const { return m_fFaction; }
    uint32 getOFaction() const { return m_oFaction; }
    uint8 getORace() const { return m_oRace; }
    uint8 getFRace() const { return m_fRace; }
};

void CFBG::CFJoinBattleGround(Player* player)
{

    if (!sWorld.getConfig(CONFIG_CROSSFACTION_BG_ENABLE))
        return;

    CFBGDPlayerInfo* CFBGplayerInfo = player->CustomData.GetDefault<CFBGDPlayerInfo>("CFBGDPlayerInfo");

    if (!player->GetMap()->IsBattleground() || player->GetMap()->IsBattleArena())
        return;

    if (player->GetTeam() == player->GetBGTeam())
        return;


    player->SetFaction(CFBGplayerInfo->getFFaction());
    player->SetByteValue(UNIT_FIELD_BYTES_0, 0, CFBGplayerInfo->getFRace());
    ReplaceRacials(player, false);
    FakeDisplayID(player);
    sWorld.InvalidatePlayerDataToAllClient(player->GetGUID());
}

void CFBG::FakeDisplayID(Player* player)
{

    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(player->getRace(), player->getClass());
    if (!info)
        for (uint32 i = 1; i <= CLASS_DRUID; i++)
            if ((info = sObjectMgr.GetPlayerInfo(player->getRace(), i)))
                break;

    if (!info)
    {
        sLog.outError("Player %u has incorrect race/class pair. Can't init display ids.", player->GetGUIDLow());
        return;
    }

    player->SetObjectScale(1.0f);

    uint8 gender = player->getGender();

    switch (gender)
    {
    case GENDER_FEMALE:
        player->SetDisplayId(info->displayId_f);
        player->SetNativeDisplayId(info->displayId_f);
        break;
    case GENDER_MALE:
        player->SetDisplayId(info->displayId_m);
        player->SetNativeDisplayId(info->displayId_m);
        break;
    default:
        sLog.outError("Invalid gender %u for player", gender);
        return;
    }
}

void CFBG::SetFakeValues(Player* player)
{
    CFBGDPlayerInfo* CFBGplayerInfo = player->CustomData.GetDefault<CFBGDPlayerInfo>("CFBGDPlayerInfo");

    CFBGplayerInfo->m_oRace = player->GetByteValue(UNIT_FIELD_BYTES_0, 0);
    CFBGplayerInfo->m_oFaction = player->GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE);
    CFBGplayerInfo->m_fRace = 0;

    while (CFBGplayerInfo->m_fRace == 0)
    {
        for (uint8 i = RACE_HUMAN; i <= RACE_DRAENEI; ++i)
        {
            if (i == RACE_GOBLIN)
                continue;

            PlayerInfo const* info = sObjectMgr.GetPlayerInfo(i, player->getClass());
            if (!info || Player::TeamForRace(i) == player->GetTeam())
                continue;

            if (urand(0, 5) == 0)
                CFBGplayerInfo->m_fRace = i;
        }
    }

    CFBGplayerInfo->m_fFaction = Player::getFactionForRace(CFBGplayerInfo->m_fRace);
}

void CFBG::CFLeaveBattleGround(Player* player)
{
    if (!sWorld.getConfig(CONFIG_CROSSFACTION_BG_ENABLE))
        return;

    CFBGDPlayerInfo* CFBGplayerInfo = player->CustomData.GetDefault<CFBGDPlayerInfo>("CFBGDPlayerInfo");

    player->SetByteValue(UNIT_FIELD_BYTES_0, 0, CFBGplayerInfo->getORace());
    player->SetFaction(CFBGplayerInfo->getOFaction());
    ReplaceRacials(player, true);
    player->InitDisplayIds();
    sWorld.InvalidatePlayerDataToAllClient(player->GetGUID());
}

void CFBG::ReplaceRacials(Player* player, bool native)
{
    if (!sWorld.getConfig(CONFIG_CROSSFACTION_REPLACE_RACIALS) ||!sWorld.getConfig(CONFIG_CROSSFACTION_BG_ENABLE))
        return;

    CFBGDPlayerInfo* CFBGplayerInfo = player->CustomData.GetDefault<CFBGDPlayerInfo>("CFBGDPlayerInfo");

    // SpellId, Should have spell
    auto spells = std::unordered_map<uint32, bool>();

    for (auto& i : sObjectMgr.GetPlayerInfo(native ? CFBGplayerInfo->getFRace() : CFBGplayerInfo->getORace(), player->getClass())->spell)
        if (auto spell = sSpellStore.LookupEntry(i.first))
            if (sWorld.getConfig(CONFIG_CROSSFACTION_REPLACE_LANGUAGE) && !sWorld.getConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_CHAT))
                spells[spell->Id] = false;
            else if (spell->Effect[0] != SPELL_EFFECT_LANGUAGE && !sWorld.getConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_CHAT))
                spells[spell->Id] = false;

    for (auto& i : sObjectMgr.GetPlayerInfo(native ? CFBGplayerInfo->getORace() : CFBGplayerInfo->getFRace(), player->getClass())->spell)
        if (auto spell = sSpellStore.LookupEntry(i.first))
            if (sWorld.getConfig(CONFIG_CROSSFACTION_REPLACE_LANGUAGE) && !sWorld.getConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_CHAT))
                spells[spell->Id] = true;
            else if (spell->Effect[0] != SPELL_EFFECT_LANGUAGE && !sWorld.getConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_CHAT))
                spells[spell->Id] = true;

    for (auto& i : spells)
    {
        if (i.second)
            player->LearnSpell(i.first);
        else
            player->RemoveSpell(i.first);
    }
}