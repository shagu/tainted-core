delete from `updates` where `update` = '2020_01_17_realmd_mod_test.sql';
REPLACE INTO `module_config` (`id`, `config`, `value`, `comment`) VALUES
	(1, 'modsample.enableHelloWorld', '0', NULL),
	(2, 'modsample.stringtest', 'String is working! :D', NULL),
	(3, 'modsample.intTest', '1908', NULL);
