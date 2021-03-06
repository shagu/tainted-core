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

/* ScriptData
SDName: Instance_Steam_Vault
SD%Complete: 80
SDComment:  Instance script and access panel GO
SDCategory: Coilfang Resevoir, The Steamvault
EndScriptData */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "steam_vault.h"

#define ENCOUNTERS 4

#define MAIN_CHAMBERS_DOOR 183049
#define ACCESS_PANEL_HYDRO 184125
#define ACCESS_PANEL_MEK 184126

/* Steam Vaults encounters:
1 - Hydromancer Thespia Event
2 - Mekgineer Steamrigger Event
3 - Warlord Kalithresh Event
*/

class go_main_chambers_access_panel : public GameObjectScript {
public:
  go_main_chambers_access_panel() : GameObjectScript("go_main_chambers_access_panel") {}

  bool OnGossipHello(Player * /*player*/, GameObject *_GO) override {
    ScriptedInstance *pInstance = (ScriptedInstance *)_GO->GetInstanceData();

    if (!pInstance)
      return false;

    if (_GO->GetEntry() == ACCESS_PANEL_HYDRO && (pInstance->GetData(TYPE_HYDROMANCER_THESPIA) == DONE ||
                                                  pInstance->GetData(TYPE_HYDROMANCER_THESPIA) == SPECIAL))
      pInstance->SetData(TYPE_HYDROMANCER_THESPIA, SPECIAL);

    if (_GO->GetEntry() == ACCESS_PANEL_MEK && (pInstance->GetData(TYPE_MEKGINEER_STEAMRIGGER) == DONE ||
                                                pInstance->GetData(TYPE_MEKGINEER_STEAMRIGGER) == SPECIAL))
      pInstance->SetData(TYPE_MEKGINEER_STEAMRIGGER, SPECIAL);

    return true;
  }
};

class instance_steam_vault : public InstanceMapScript {
public:
  instance_steam_vault() : InstanceMapScript("instance_steam_vault", 545) {}

  struct instance_steam_vaultAI : public ScriptedInstance {
    instance_steam_vaultAI(Map *map) : ScriptedInstance(map) { Initialize(); };

    uint32 Encounter[ENCOUNTERS];

    uint64 ThespiaGUID;
    uint64 MekgineerGUID;
    uint64 KalithreshGUID;

    uint64 MainChambersDoor;
    uint64 AccessPanelHydro;
    uint64 AccessPanelMek;

    void Initialize() {
      ThespiaGUID = 0;
      MekgineerGUID = 0;
      KalithreshGUID = 0;
      MainChambersDoor = 0;
      AccessPanelHydro = 0;
      AccessPanelMek = 0;

      for (uint8 i = 0; i < ENCOUNTERS; i++)
        Encounter[i] = NOT_STARTED;
    }

    bool IsEncounterInProgress() const {
      for (uint8 i = 0; i < ENCOUNTERS; i++)
        if (Encounter[i] == IN_PROGRESS)
          return true;

      return false;
    }

    Player *GetPlayerInMap() {
      Map::PlayerList const &players = instance->GetPlayers();

      if (!players.isEmpty()) {
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr) {
          if (Player *plr = itr->GetSource())
            return plr;
        }
      }

      sLog.outDebug("OSCR: Instance Steamvault: GetPlayerInMap, but PlayerList is empty!");
      return NULL;
    }

    void OnCreatureCreate(Creature *pCreature, bool /*add*/) {
      switch (pCreature->GetEntry()) {
      case 17797:
        ThespiaGUID = pCreature->GetGUID();
        break;
      case 17796:
        MekgineerGUID = pCreature->GetGUID();
        break;
      case 17798:
        KalithreshGUID = pCreature->GetGUID();
        break;
      }
    }

    void OnGameObjectCreate(GameObject *pGo, bool /*add*/) {
      switch (pGo->GetEntry()) {
      case MAIN_CHAMBERS_DOOR:
        MainChambersDoor = pGo->GetGUID();
        break;
      case ACCESS_PANEL_HYDRO:
        AccessPanelHydro = pGo->GetGUID();
        break;
      case ACCESS_PANEL_MEK:
        AccessPanelMek = pGo->GetGUID();
        break;
      }
    }

    void SetData(uint32 type, uint32 data) {
      Player *player = GetPlayerInMap();

      if (!player) {
        sLog.outDebug("OSCR: Instance Steamvault: SetData (Type: %u Data %u) cannot find any player.", type, data);
        return;
      }

      switch (type) {
      case TYPE_HYDROMANCER_THESPIA:
        if (data == SPECIAL) {
          if (GameObject *_go = GameObject::GetGameObject(*player, AccessPanelHydro))
            _go->SetGoState(GO_STATE_ACTIVE);

          if (GetData(TYPE_MEKGINEER_STEAMRIGGER) == SPECIAL) {
            if (GameObject *_go = GameObject::GetGameObject(*player, MainChambersDoor))
              _go->SetGoState(GO_STATE_ACTIVE);
          }
          sLog.outDebug("OSCR: Instance Steamvault: Access panel used.");
        }
        Encounter[0] = data;
        break;
      case TYPE_MEKGINEER_STEAMRIGGER:
        if (data == SPECIAL) {
          if (GameObject *_go = GameObject::GetGameObject(*player, AccessPanelMek))
            _go->SetGoState(GO_STATE_ACTIVE);

          if (GetData(TYPE_HYDROMANCER_THESPIA) == SPECIAL) {
            if (GameObject *_go = GameObject::GetGameObject(*player, MainChambersDoor))
              _go->SetGoState(GO_STATE_ACTIVE);
          }
          sLog.outDebug("OSCR: Instance Steamvault: Access panel used.");
        }
        Encounter[1] = data;
        break;
      case TYPE_WARLORD_KALITHRESH:
        Encounter[2] = data;
        break;
      case TYPE_DISTILLER:
        Encounter[3] = data;
        break;
      }

      if (data == DONE || data == SPECIAL)
        SaveToDB();
    }

    uint32 GetData(uint32 type) {
      switch (type) {
      case TYPE_HYDROMANCER_THESPIA:
        return Encounter[0];
      case TYPE_MEKGINEER_STEAMRIGGER:
        return Encounter[1];
      case TYPE_WARLORD_KALITHRESH:
        return Encounter[2];
      case TYPE_DISTILLER:
        return Encounter[3];
      }
      return 0;
    }

    uint64 GetData64(uint32 data) {
      switch (data) {
      case DATA_THESPIA:
        return ThespiaGUID;
      case DATA_MEKGINEERSTEAMRIGGER:
        return MekgineerGUID;
      case DATA_KALITRESH:
        return KalithreshGUID;
      }
      return 0;
    }

    std::string GetSaveData() {
      OUT_SAVE_INST_DATA;
      std::ostringstream stream;
      stream << Encounter[0] << " " << Encounter[1] << " " << Encounter[2] << " " << Encounter[3];
      char *out = new char[stream.str().length() + 1];
      strcpy(out, stream.str().c_str());
      if (out) {
        OUT_SAVE_INST_DATA_COMPLETE;
        return out;
      }
      return NULL;
    }

    void Load(const char *in) {
      if (!in) {
        OUT_LOAD_INST_DATA_FAIL;
        return;
      }
      OUT_LOAD_INST_DATA(in);
      std::istringstream stream(in);
      stream >> Encounter[0] >> Encounter[1] >> Encounter[2] >> Encounter[3];
      for (uint8 i = 0; i < ENCOUNTERS; ++i)
        if (Encounter[i] == IN_PROGRESS)
          Encounter[i] = NOT_STARTED;
      OUT_LOAD_INST_DATA_COMPLETE;
    }
  };
  InstanceData *GetInstanceScript(InstanceMap *pMap) const override { return new instance_steam_vaultAI(pMap); }
};

void AddSC_instance_steam_vault() {
  new go_main_chambers_access_panel();
  new instance_steam_vault();
}
