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
SDName: Razorfen Kraul
SD%Complete: 100
SDComment: Quest support: 1144
SDCategory: Razorfen Kraul
EndScriptData */

/* ContentData
npc_willix
EndContentData */

#include "razorfen_kraul.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedEscortAI.h"

#define SAY_READY -1047000
#define SAY_POINT -1047001
#define SAY_AGGRO1 -1047002
#define SAY_BLUELEAF -1047003
#define SAY_DANGER -1047004
#define SAY_BAD -1047005
#define SAY_THINK -1047006
#define SAY_SOON -1047007
#define SAY_FINALY -1047008
#define SAY_WIN -1047009
#define SAY_END -1047010

#define QUEST_WILLIX_THE_IMPORTER 1144
#define ENTRY_BOAR 4514
#define SPELL_QUILLBOAR_CHANNELING 7083

class npc_willix : public CreatureScript {
public:
  npc_willix() : CreatureScript("npc_willix") {}
  struct npc_willixAI : public npc_escortAI {
    npc_willixAI(Creature *c) : npc_escortAI(c) {}

    void WaypointReached(uint32 i) {
      Player *pPlayer = GetPlayerForEscort();

      if (!pPlayer)
        return;

      switch (i) {
      case 3:
        me->HandleEmoteCommand(EMOTE_STATE_POINT);
        DoScriptText(SAY_POINT, me, pPlayer);
        break;
      case 4:
        me->SummonCreature(ENTRY_BOAR, 2137.66f, 1843.98f, 48.08f, 1.54f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                           25000);
        break;
      case 8:
        DoScriptText(SAY_BLUELEAF, me, pPlayer);
        break;
      case 9:
        DoScriptText(SAY_DANGER, me, pPlayer);
        break;
      case 13:
        DoScriptText(SAY_BAD, me, pPlayer);
        break;
      case 14:
        me->SummonCreature(ENTRY_BOAR, 2078.91f, 1704.54f, 56.77f, 1.54f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                           25000);
        break;
      case 25:
        DoScriptText(SAY_THINK, me, pPlayer);
        break;
      case 31:
        DoScriptText(SAY_SOON, me, pPlayer);
        break;
      case 42:
        DoScriptText(SAY_FINALY, me, pPlayer);
        break;
      case 43:
        me->SummonCreature(ENTRY_BOAR, 1956.43f, 1596.97f, 81.75f, 1.54f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                           25000);
        break;
      case 45:
        DoScriptText(SAY_WIN, me, pPlayer);
        me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
        pPlayer->GroupEventHappens(QUEST_WILLIX_THE_IMPORTER, me);
        break;
      case 46:
        DoScriptText(SAY_END, me, pPlayer);
        break;
      }
    }

    void Reset() {}

    void EnterCombat(Unit * /*who*/) { DoScriptText(SAY_AGGRO1, me, NULL); }

    void JustSummoned(Creature *summoned) { summoned->AI()->AttackStart(me); }

    void JustDied(Unit * /*killer*/) {
      if (Player *pPlayer = GetPlayerForEscort())
        CAST_PLR(pPlayer)->FailQuest(QUEST_WILLIX_THE_IMPORTER);
    }
  };

  CreatureAI *GetAI(Creature *pCreature) const { return new npc_willixAI(pCreature); }

  bool OnQuestAccept(Player *pPlayer, Creature *pCreature, Quest const *quest) override {
    if (quest->GetQuestId() == QUEST_WILLIX_THE_IMPORTER) {
      CAST_AI(npc_escortAI, (pCreature->AI()))->Start(true, false, pPlayer->GetGUID());
      DoScriptText(SAY_READY, pCreature, pPlayer);
      pCreature->SetFaction(113);
    }

    return true;
  }
};

class npc_deaths_head_ward_keeper : public CreatureScript {
public:
  npc_deaths_head_ward_keeper() : CreatureScript("npc_deaths_head_ward_keeper") {}
  struct npc_deaths_head_ward_keeperAI : public ScriptedAI {
    npc_deaths_head_ward_keeperAI(Creature *c) : ScriptedAI(c) { pInstance = (ScriptedInstance *)c->GetInstanceData(); }

    ScriptedInstance *pInstance;
    uint32 QuillboarChanneling_Timer;

    void Reset() { QuillboarChanneling_Timer = 1500; }

    void UpdateAI(const uint32 diff) {
      if (!me->IsAlive())
        return;

      if (pInstance)
        pInstance->SetData(TYPE_WARD_KEEPERS, NOT_STARTED);

      if (QuillboarChanneling_Timer <= diff) {
        if (me->IsNonMeleeSpellCast(false))
          me->InterruptNonMeleeSpells(true);
        DoCast(me, SPELL_QUILLBOAR_CHANNELING);
        QuillboarChanneling_Timer = 1100;
      } else
        QuillboarChanneling_Timer -= diff;
    }
  };

  CreatureAI *GetAI(Creature *pCreature) const { return new npc_deaths_head_ward_keeperAI(pCreature); }
};

void AddSC_razorfen_kraul() {
  new npc_willix();
  new npc_deaths_head_ward_keeper();
}
