-- prepare translation tables (taken from MaNGOS-One 1_LocaleTablePrepare.sql)
INSERT INTO `locales_creature` (`entry`) SELECT `entry` FROM `creature_template` WHERE `entry` NOT IN (SELECT `entry` FROM `locales_creature`);
INSERT INTO `locales_gameobject` (`entry`) SELECT `entry` FROM `gameobject_template` WHERE `entry` NOT IN (SELECT `entry` FROM `locales_gameobject`);
INSERT INTO `locales_gossip_menu_option` (`menu_id`,`id`) SELECT `menu_id`,`id` FROM `gossip_menu_option` WHERE `gossip_menu_option`.`menu_id` NOT IN (SELECT `menu_id` FROM `locales_gossip_menu_option`) AND `gossip_menu_option`.`id` NOT IN (SELECT `id` FROM `locales_gossip_menu_option`);
INSERT INTO `locales_item` (`entry`) SELECT `entry` FROM `item_template` WHERE `entry` NOT IN (SELECT `entry` FROM `locales_item`);
INSERT INTO `locales_npc_text` (`entry`) SELECT `ID` FROM `npc_text` WHERE `ID` NOT IN (SELECT `entry` FROM `locales_npc_text`);
INSERT INTO `locales_page_text` (`entry`) SELECT `entry` FROM `page_text` WHERE `entry` NOT IN (SELECT `entry` FROM `locales_page_text`);
INSERT INTO `locales_quest` (`entry`) SELECT `entry` FROM `quest_template` WHERE `entry` NOT IN (SELECT `entry` FROM `locales_quest`);
