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
#include "Spell.h"

void CFBG::CFJoinBattleGround(Player* player)
{

    if (!sWorld.getConfig(CONFIG_CROSSFACTION_BG_ENABLE))
        return;

    if (!player->GetMap()->IsBattleground() || player->GetMap()->IsBattleArena())
        return;

    AssignTeam(player);

    if (NativeTeam(player))
        return;

    TeleportToStart(player);
    player->SetByteValue(UNIT_FIELD_BYTES_0, 0, getFRace());
    player->SetFaction(getFFaction());
    FakeDisplayID(player);
    sWorld.InvalidatePlayerDataToAllClient(player->GetGUID());
}

bool CFBG::NativeTeam(Player* player) const
{
    return player->GetTeam() == player->GetBGTeam();
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
    if (!sWorld.getConfig(CONFIG_CROSSFACTION_BG_ENABLE))
        return;

    m_oFaction = player->GetFaction();
    m_oRace = player->getRace();
    m_oTeam = player->GetTeam();
    m_oRace = player->GetByteValue(UNIT_FIELD_BYTES_0, 0);
    m_oFaction = player->GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE);

    m_fRace = 0;

    while (m_fRace == 0)
    {
        for (uint8 i = RACE_HUMAN; i <= RACE_DRAENEI; ++i)
        {
            if (i == RACE_GOBLIN)
                continue;

            PlayerInfo const* info = sObjectMgr.GetPlayerInfo(i, player->getClass());
            if (!info || Player::TeamForRace(i) == player->GetTeam())
                continue;

            if (urand(0, 5) == 0)
                m_fRace = i;
        }
    }

    m_fFaction = Player::getFactionForRace(m_fRace);

}

void CFBG::CFLeaveBattleGround(Player* player)
{
    if (!sWorld.getConfig(CONFIG_CROSSFACTION_BG_ENABLE))
        return;

    player->SetByteValue(UNIT_FIELD_BYTES_0, 0, getORace());
    player->SetFaction(getOFaction());
    player->SetTeam(getOTeam());
    player->InitDisplayIds();

    sWorld.InvalidatePlayerDataToAllClient(player->GetGUID());
}


void CFBG::TeleportToStart(Player* player)
{
    Battleground* bg = player->GetBattleground();

    if (bg->GetStatus() == IN_PROGRESS)
    {
        float x, y, z, o;
        bg->GetTeamStartLoc(player->GetBGTeam(), x, y, z, o);

        player->Relocate(x, y, z, o);
    }
}

void CFBG::AssignTeam(Player* player)
{
    Battleground* bg = player->GetBattleground();
    uint32 qHorde = bg->GetPlayersCountByTeam(HORDE);
    uint32 qAlliance = bg->GetPlayersCountByTeam(ALLIANCE);

    if (sWorld.getConfig(CONFIG_CROSSFACTION_BG_ENABLE))
    {
        if (qAlliance = qHorde)
            return;
        else if (qAlliance < qHorde)
        {
            sCrossFaction.f_bgTeam = TEAM_ALLIANCE;
            qAlliance++;
        }
        else if (qHorde < qAlliance)
        {
            sCrossFaction.f_bgTeam = TEAM_HORDE;
            qHorde++;
        }
    }

    player->SetBGTeam(f_bgTeam);
}