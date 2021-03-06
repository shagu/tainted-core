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
SDName: Boss_Razorgore
SD%Complete: 50
SDComment: Needs additional review. Phase 1 NYI (Grethok the Controller)
SDCategory: Blackwing Lair
EndScriptData */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"

// Razorgore Phase 2 Script

#define SAY_EGGS_BROKEN1 -1469022
#define SAY_EGGS_BROKEN2 -1469023
#define SAY_EGGS_BROKEN3 -1469024
#define SAY_DEATH -1469025

#define SPELL_CLEAVE 22540
#define SPELL_WARSTOMP 24375
#define SPELL_FIREBALLVOLLEY 22425
#define SPELL_CONFLAGRATION 23023

class boss_razorgore : public CreatureScript {
public:
  boss_razorgore() : CreatureScript("boss_razorgore") {}

  struct boss_razorgoreAI : public ScriptedAI {
    boss_razorgoreAI(Creature *c) : ScriptedAI(c) {}

    uint32 Cleave_Timer;
    uint32 WarStomp_Timer;
    uint32 FireballVolley_Timer;
    uint32 Conflagration_Timer;

    void Reset() {
      Cleave_Timer = 15000; // These times are probably wrong
      WarStomp_Timer = 35000;
      FireballVolley_Timer = 7000;
      Conflagration_Timer = 12000;
    }

    void EnterCombat(Unit * /*who*/) { DoZoneInCombat(); }

    void JustDied(Unit * /*Killer*/) { DoScriptText(SAY_DEATH, me); }

    void UpdateAI(const uint32 diff) {
      if (!UpdateVictim())
        return;

      // Cleave_Timer
      if (Cleave_Timer <= diff) {
        DoCastVictim(SPELL_CLEAVE);
        Cleave_Timer = urand(7000, 10000);
      } else
        Cleave_Timer -= diff;

      // WarStomp_Timer
      if (WarStomp_Timer <= diff) {
        DoCastVictim(SPELL_WARSTOMP);
        WarStomp_Timer = urand(15000, 25000);
      } else
        WarStomp_Timer -= diff;

      // FireballVolley_Timer
      if (FireballVolley_Timer <= diff) {
        DoCastVictim(SPELL_FIREBALLVOLLEY);
        FireballVolley_Timer = urand(12000, 15000);
      } else
        FireballVolley_Timer -= diff;

      // Conflagration_Timer
      if (Conflagration_Timer <= diff) {
        DoCastVictim(SPELL_CONFLAGRATION);
        // We will remove this threat reduction and add an aura check.

        // if (DoGetThreat(me->GetVictim()))
        // DoModifyThreatPercent(me->GetVictim(),-50);

        Conflagration_Timer = 12000;
      } else
        Conflagration_Timer -= diff;

      // Aura Check. If the gamer is affected by confliguration we attack a random gamer.
      if (me->GetVictim() && me->GetVictim()->HasAura(SPELL_CONFLAGRATION, 0))
        if (Unit *pTarget = SelectTarget(SELECT_TARGET_RANDOM, 1, 100, true))
          me->TauntApply(pTarget);

      DoMeleeAttackIfReady();
    }
  };

  CreatureAI *GetAI(Creature *pCreature) const { return new boss_razorgoreAI(pCreature); }
};

void AddSC_boss_razorgore() { new boss_razorgore(); }
