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
SDName: Boss_Bloodmage_Thalnos
SD%Complete: 100
SDComment:
SDCategory: Scarlet Monastery
EndScriptData */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"

enum eEnums {
  SAY_AGGRO = -1189016,
  SAY_HEALTH = -1189017,
  SAY_KILL = -1189018,

  SPELL_FLAMESHOCK = 8053,
  SPELL_SHADOWBOLT = 1106,
  SPELL_FLAMESPIKE = 8814,
  SPELL_FIRENOVA = 16079,
};

class boss_bloodmage_thalnos : public CreatureScript {
public:
  boss_bloodmage_thalnos() : CreatureScript("boss_bloodmage_thalnos") {}

  struct boss_bloodmage_thalnosAI : public ScriptedAI {
    boss_bloodmage_thalnosAI(Creature *c) : ScriptedAI(c) {}

    bool HpYell;
    uint32 FlameShock_Timer;
    uint32 ShadowBolt_Timer;
    uint32 FlameSpike_Timer;
    uint32 FireNova_Timer;

    void Reset() {
      HpYell = false;
      FlameShock_Timer = 10000;
      ShadowBolt_Timer = 3000;
      FlameSpike_Timer = 8000;
      FireNova_Timer = 40000;
    }

    void EnterCombat(Unit * /*who*/) { DoScriptText(SAY_AGGRO, me); }

    void KilledUnit(Unit * /*Victim*/) { DoScriptText(SAY_KILL, me); }

    void UpdateAI(const uint32 diff) {
      if (!UpdateVictim())
        return;

      if (me->IsNonMeleeSpellCast(false)) // Checks if spell NPC is already casting a spell
        return;

      // If we are <35% hp
      if (!HpYell && ((me->GetHealth() * 100) / me->GetMaxHealth() <= 35)) {
        DoScriptText(SAY_HEALTH, me);
        HpYell = true;
      }

      // FlameShock_Timer
      if (FlameShock_Timer <= diff) {
        DoCastVictim(SPELL_FLAMESHOCK);
        FlameShock_Timer = 10000 + rand() % 5000;
      } else
        FlameShock_Timer -= diff;

      // FlameSpike_Timer

      if (FlameSpike_Timer <= diff) {
        DoCastVictim(SPELL_FLAMESPIKE);
        FlameSpike_Timer = 30000;
      } else
        FlameSpike_Timer -= diff;

      // FireNova_Timer

      if (FireNova_Timer <= diff) {
        DoCastVictim(SPELL_FIRENOVA);
        FireNova_Timer = 40000;
      } else
        FireNova_Timer -= diff;

      // ShadowBolt_Timer

      if (ShadowBolt_Timer <= diff) {
        DoCastVictim(SPELL_SHADOWBOLT);
        ShadowBolt_Timer = 3000;
      } else
        ShadowBolt_Timer -= diff;

      DoMeleeAttackIfReady();
    }
  };

  CreatureAI *GetAI(Creature *pCreature) const { return new boss_bloodmage_thalnosAI(pCreature); }
};

void AddSC_boss_bloodmage_thalnos() { new boss_bloodmage_thalnos(); }
