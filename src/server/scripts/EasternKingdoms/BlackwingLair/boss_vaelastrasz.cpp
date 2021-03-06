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
SDName: Boss_Vaelastrasz
SD%Complete: 75
SDComment: Burning Adrenaline not correctly implemented in core
SDCategory: Blackwing Lair
EndScriptData */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"

#define SAY_LINE1 -1469026
#define SAY_LINE2 -1469027
#define SAY_LINE3 -1469028
#define SAY_HALFLIFE -1469029
#define SAY_KILLTARGET -1469030

#define GOSSIP_ITEM "Start Event <Needs Gossip Text>"

#define SPELL_ESSENCEOFTHERED 23513
#define SPELL_FLAMEBREATH 23461
#define SPELL_FIRENOVA 23462
#define SPELL_TAILSWIPE 15847
#define SPELL_BURNINGADRENALINE 23620
#define SPELL_CLEAVE 20684 // Chain cleave is most likely named something different and contains a dummy effect

class boss_vaelastrasz : public CreatureScript {
public:
  boss_vaelastrasz() : CreatureScript("boss_vaelastrasz") {}

  struct boss_vaelastraszAI : public ScriptedAI {
    boss_vaelastraszAI(Creature *c) : ScriptedAI(c) {
      c->SetUInt32Value(UNIT_NPC_FLAGS, 1);
      c->SetFaction(35);
      c->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
    }

    uint64 PlayerGUID;
    uint32 SpeechTimer;
    uint32 SpeechNum;
    uint32 Cleave_Timer;
    uint32 FlameBreath_Timer;
    uint32 FireNova_Timer;
    uint32 BurningAdrenalineCaster_Timer;
    uint32 BurningAdrenalineTank_Timer;
    uint32 TailSwipe_Timer;
    bool HasYelled;
    bool DoingSpeech;

    void Reset() {
      PlayerGUID = 0;
      SpeechTimer = 0;
      SpeechNum = 0;
      Cleave_Timer = 8000; // These times are probably wrong
      FlameBreath_Timer = 11000;
      BurningAdrenalineCaster_Timer = 15000;
      BurningAdrenalineTank_Timer = 45000;
      FireNova_Timer = 5000;
      TailSwipe_Timer = 20000;
      HasYelled = false;
      DoingSpeech = false;
    }

    void BeginSpeech(Unit *pTarget) {
      // Stand up and begin speach
      PlayerGUID = pTarget->GetGUID();

      // 10 seconds
      DoScriptText(SAY_LINE1, me);

      SpeechTimer = 10000;
      SpeechNum = 0;
      DoingSpeech = true;

      me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
    }

    void KilledUnit(Unit *victim) {
      if (rand() % 5)
        return;

      DoScriptText(SAY_KILLTARGET, me, victim);
    }

    void EnterCombat(Unit * /*who*/) {
      DoCast(me, SPELL_ESSENCEOFTHERED);
      DoZoneInCombat();
      me->SetHealth(int(me->GetMaxHealth() * .3));
    }

    void UpdateAI(const uint32 diff) {
      // Speech
      if (DoingSpeech) {
        if (SpeechTimer <= diff) {
          switch (SpeechNum) {
          case 0:
            // 16 seconds till next line
            DoScriptText(SAY_LINE2, me);
            SpeechTimer = 16000;
            ++SpeechNum;
            break;
          case 1:
            // This one is actually 16 seconds but we only go to 10 seconds because he starts attacking after he says "I
            // must fight this!"
            DoScriptText(SAY_LINE3, me);
            SpeechTimer = 10000;
            ++SpeechNum;
            break;
          case 2:
            me->SetFaction(103);
            if (PlayerGUID && Unit::GetUnit((*me), PlayerGUID)) {
              AttackStart(Unit::GetUnit((*me), PlayerGUID));
              DoCast(me, SPELL_ESSENCEOFTHERED);
            }
            SpeechTimer = 0;
            DoingSpeech = false;
            break;
          }
        } else
          SpeechTimer -= diff;
      }

      // Return since we have no target
      if (!UpdateVictim())
        return;

      // Yell if hp lower than 15%
      if (HealthBelowPct(15) && !HasYelled) {
        DoScriptText(SAY_HALFLIFE, me);
        HasYelled = true;
      }

      // Cleave_Timer
      if (Cleave_Timer <= diff) {
        DoCastVictim(SPELL_CLEAVE);
        Cleave_Timer = 15000;
      } else
        Cleave_Timer -= diff;

      // FlameBreath_Timer
      if (FlameBreath_Timer <= diff) {
        DoCastVictim(SPELL_FLAMEBREATH);
        FlameBreath_Timer = urand(4000, 8000);
      } else
        FlameBreath_Timer -= diff;

      // BurningAdrenalineCaster_Timer
      if (BurningAdrenalineCaster_Timer <= diff) {
        Unit *pTarget = NULL;

        uint8 i = 0;
        while (i < 3) // max 3 tries to get a random target with power_mana
        {
          ++i;
          pTarget = SelectTarget(SELECT_TARGET_RANDOM, 1, 100, true); // not aggro leader
          if (pTarget && pTarget->getPowerType() == POWER_MANA)
            i = 3;
        }
        if (pTarget) // cast on self (see below)
          pTarget->CastSpell(pTarget, SPELL_BURNINGADRENALINE, 1);

        BurningAdrenalineCaster_Timer = 15000;
      } else
        BurningAdrenalineCaster_Timer -= diff;

      // BurningAdrenalineTank_Timer
      if (BurningAdrenalineTank_Timer <= diff) {
        // have the victim cast the spell on himself otherwise the third effect aura will be applied
        // to Vael instead of the player
        me->GetVictim()->CastSpell(me->GetVictim(), SPELL_BURNINGADRENALINE, 1);

        BurningAdrenalineTank_Timer = 45000;
      } else
        BurningAdrenalineTank_Timer -= diff;

      // FireNova_Timer
      if (FireNova_Timer <= diff) {
        DoCastVictim(SPELL_FIRENOVA);
        FireNova_Timer = 5000;
      } else
        FireNova_Timer -= diff;

      // TailSwipe_Timer
      if (TailSwipe_Timer <= diff) {
        // Only cast if we are behind
        /*if (!me->HasInArc(M_PI, me->GetVictim()))
        {
        DoCastVictim( SPELL_TAILSWIPE);
        }*/

        TailSwipe_Timer = 20000;
      } else
        TailSwipe_Timer -= diff;

      DoMeleeAttackIfReady();
    }
  };

  void SendDefaultMenu_boss_vael(Player *pPlayer, Creature *pCreature, uint32 uiAction) {
    if (uiAction == GOSSIP_ACTION_INFO_DEF + 1) // Fight time
    {
      pPlayer->CLOSE_GOSSIP_MENU();
      CAST_AI(boss_vaelastraszAI, pCreature->AI())->BeginSpeech(pPlayer);
    }
  }

  bool OnGossipSelect(Player *pPlayer, Creature *pCreature, uint32 uiSender, uint32 uiAction) override {
    if (uiSender == GOSSIP_SENDER_MAIN)
      SendDefaultMenu_boss_vael(pPlayer, pCreature, uiAction);

    return true;
  }

  bool OnGossipHello(Player *pPlayer, Creature *pCreature) override {
    pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_ITEM, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
    pPlayer->SEND_GOSSIP_MENU(907, pCreature->GetGUID());

    return true;
  }

  CreatureAI *GetAI(Creature *pCreature) const { return new boss_vaelastraszAI(pCreature); }
};

void AddSC_boss_vael() { new boss_vaelastrasz(); }
