#ifndef CFBG_H
#define CFBG_H

#include "Player.h"


class CFBG
{
public:
    uint8 getFRace() const { return m_fRace; }
    uint8 getORace() const { return m_oRace; }
    uint32 getOFaction() const { return m_oFaction; }
    uint32 getFFaction() const { return m_fFaction; }
    uint32 getOTeam() const { return m_oTeam; }

    void CFJoinBattleGround(Player* player);
    void CFLeaveBattleGround(Player* player);
    void SetFakeValues(Player* player);
    void FakeDisplayID(Player* player);
    void AssignTeam(Player* player);
    void TeleportToStart(Player* player);

    bool NativeTeam(Player* player) const;
    uint32 f_bgTeam;

private:
    uint8 m_fRace;
    uint8 m_oRace;
    uint32 m_fFaction;
    uint32 m_oFaction;
    uint32 m_oTeam;
};

#define sCrossFaction Oregon::Singleton<CFBG>::Instance()
#endif