#include <amxmodx>
// #include <reapi>
#include <amxmisc>
// #include <cstrike>
// #include <engine>
// #include <fakemeta>
// #include <hamsandwich>
// #include <fun>
// #include <xs>
// #include <sqlx>


#define SPREAD_MULTIPLIER_MIN 0.0 	// OFF
#define SPREAD_MULTIPLIER_MAX 100.0 // ARBITRARY MAX (WHY INCREASE SPREAD BEYOND THIS?)

new const logFile[] = "weapon_tuning_log.txt";

new const g_WeaponNames[][] =
{
	"GLOCK18", "USP", "P228", "DEAGLE", "ELITE", "FIVESEVEN",
	"AWP", "SCOUT", "G3SG1", "SG550",
	"GALIL", "FAMAS", "AK47", "M4A1", "SG552", "AUG",
	"MAC10", "TMP", "MP5NAVY", "UMP45", "P90",
	"M249"
};

enum SpreadWeaponIndex
{
	GLOCK18,
	USP,      
	P228,     
	DEAGLE,   
	ELITE,
	FIVESEVEN,
	AWP, 
	SCOUT, 
	G3SG1, 
	SG550,
	GALIL, 
	FAMAS,  
	AK47,  
	M4A1, 
	SG552,
	AUG,
	MAC10,
	TMP,
	MP5NAVY,
	UMP45,
	P90,
	M249,
}

enum SpreadField
{
    Float:InAir,
    Float:MovingStanding,
    Float:MovingDucking,
    Float:StandingStill,
    Float:DuckingStill,
    Float:DefaultSpread
}

new g_RecoilCvar;
new g_zeroSpreadFirstShotCvar;
new Float:g_Spread[sizeof(g_WeaponNames)][SpreadField];

public plugin_init()
{
	register_plugin("Weapon Tuning Adm", "1.0", "VictorOak");
	
	register_clcmd("amx_recoil", "cmdRecoil", ADMIN_RCON, "- sets the recoil multiplier (0 to 100)");
	register_clcmd("amx_zerospreadfs", "cmdZeroSpreadFirstShot", ADMIN_RCON, "- sets whether first shot is dead center when not moving (0 or 1)");
	register_clcmd("amx_spread", "cmdSpread", ADMIN_RCON, "- <weapon_name> <in_air> <moving_standing> <moving_ducking> <standing_still> <ducking_still> <default> where each value is a float from 0.0 above, being 1 the default");
	register_clcmd("say /recoil", "getRecoil", ADMIN_ALL, "- displays current recoil multiplier & dead center shot status");
	register_clcmd("say_team /recoil", "getRecoil", ADMIN_ALL, "- displays current recoil multiplier & dead center shot status");

	g_zeroSpreadFirstShotCvar = get_cvar_pointer("wt_zeroSpreadFirstShot_mm");
	g_RecoilCvar = get_cvar_pointer("wt_recoil_mm");

	// Default all spread configs to "off".
	for (new i = 0; i < sizeof(g_Spread); ++i)
	{
		g_Spread[i][InAir] =
		g_Spread[i][MovingStanding] =
		g_Spread[i][MovingDucking] =
		g_Spread[i][StandingStill] =
		g_Spread[i][DuckingStill] =
		g_Spread[i][DefaultSpread] = 1.0;
	}

	loadConfig();	
}

public plugin_end()
{
	// Probably not gonna be used.
}

public cmdZeroSpreadFirstShot(id, level, cmdId)
{
	if (cmd_access(id, level, cmdId, 2) != 1) {
		console_print(id, "ZERO SPREAD FIRST SHOT IS %d", get_pcvar_num(g_zeroSpreadFirstShotCvar));
		return PLUGIN_HANDLED;
	}

	new multiplier[2];
	read_argv(1, multiplier, charsmax(multiplier));

	new value = str_to_num(multiplier);

	if (value != 0 && value != 1)
		value = 1;

	set_pcvar_num(g_zeroSpreadFirstShotCvar, value);

	new name[32];
	get_user_name(id, name, charsmax(name));

	client_print_color(0, id, "^3%s ^4%s ^1GARANTIA DE PRIMEIRO TIRO 100% CERTEIRO", name, value == 1 ? "ATIVOU" : "DESATIVOU");

	saveConfig();

	return PLUGIN_HANDLED;
}

public cmdRecoil(id, level, cmdId)
{
	if (cmd_access(id, level, cmdId, 2) != 1) {
		console_print(id, "RECOIL IS %d", get_pcvar_num(g_RecoilCvar));
		return PLUGIN_HANDLED;
	}

	new multiplier[4];
	read_argv(1, multiplier, charsmax(multiplier));

	new value = str_to_num(multiplier);
	value = clamp(value, 0, 100);

	// log_amx("cmdRecoil %d", value);
	set_pcvar_num(g_RecoilCvar, value);

	new name[32];
	get_user_name(id, name, charsmax(name));

	client_print_color(0, id, "^3%s ^1ALTEROU RECOIL PARA ^4%d%", name, value);

	saveConfig();

	return PLUGIN_HANDLED;
}

public cmdSpread(id, level, cmdId)
{
	if (!cmd_access(id, level, cmdId, 2))
		return PLUGIN_HANDLED;

	if (read_argc() == 2) {

		// Return current values.

		new i = 0;
		new weaponName[16];
		read_argv(1, weaponName, charsmax(weaponName));
		
		for (; i < sizeof(g_WeaponNames); ++i)
		{
			if (equali(weaponName, g_WeaponNames[i]))
				break;
		}

		if (i == sizeof(g_WeaponNames)) {
			console_print(id, "ARMA INVALIDA");
			return PLUGIN_HANDLED;
		}

		console_print(id, "SPREAD ATUAL %s: %.2f %.2f %.2f %.2f %.2f %.2f", g_WeaponNames[i], g_Spread[i][InAir], g_Spread[i][MovingStanding], g_Spread[i][MovingDucking],
			g_Spread[i][StandingStill], g_Spread[i][DuckingStill], g_Spread[i][DefaultSpread]);

		return PLUGIN_HANDLED;
	}

	new name[32];
	get_user_name(id, name, charsmax(name));

	new fullCmd[128], cmdArgs[128];
    read_args(cmdArgs, charsmax(cmdArgs));

    log_amx("ARGS: %s", cmdArgs);
    formatex(fullCmd, charsmax(fullCmd), "wt_spread_mm %s", cmdArgs);
    log_amx("CMD: %s", fullCmd);

	new SpreadWeaponIndex:weaponIndex;
	new Float:inAir;
	new Float:movingStanding;
	new Float:movingDucking;
	new Float:standingStill;
	new Float:duckingStill;
	new Float:_default;

	if (parseSpreadEntry(fullCmd,
		weaponIndex,
		inAir,
		movingStanding, movingDucking,
		standingStill, duckingStill,
		_default)) {

		g_Spread[_:weaponIndex][InAir] = inAir;
		g_Spread[_:weaponIndex][MovingStanding] = movingStanding;
		g_Spread[_:weaponIndex][MovingDucking] = movingDucking;
		g_Spread[_:weaponIndex][StandingStill] = standingStill;
		g_Spread[_:weaponIndex][DuckingStill] = duckingStill;
		g_Spread[_:weaponIndex][DefaultSpread] = _default;

		server_cmd(
			"wt_spread_mm %s %.2f %.2f %.2f %.2f %.2f %.2f",
			g_WeaponNames[_:weaponIndex],
			inAir, movingStanding, movingDucking,
			standingStill, duckingStill, _default);
		server_exec();

		log_to_file(logFile, "[%s] set wt_spread_mm [%s] to [%.2f] [%.2f] [%.2f] [%.2f] [%.2f] [%.2f]",
			name, g_WeaponNames[_:weaponIndex], inAir, movingStanding, movingDucking, standingStill, duckingStill, _default);

		client_print_color(0, id, "^3%s ^1ALTEROU O ^4SPREAD DA [%s] PARA: [%.2f] [%.2f] [%.2f] [%.2f] [%.2f] [%.2f]",
			name, g_WeaponNames[_:weaponIndex], inAir, movingStanding, movingDucking, standingStill, duckingStill, _default);

		saveConfig();
	}
	else {

		log_to_file(logFile, "[%s] TRIED TO set wt_spread_mm [%s] to [%.2f] [%.2f] [%.2f] [%.2f] [%.2f] [%.2f]",
			name, g_WeaponNames[_:weaponIndex], inAir, movingStanding, movingDucking, standingStill, duckingStill, _default);

		console_print(id, "BAD SYNTAX: amx_spread %s %.2f %.2f %.2f %.2f %.2f %.2f. Example: amx_spread UMP45 -1.0 -1.0 0.85 -1.0 -1.0 -1.0",
			g_WeaponNames[_:weaponIndex], inAir, movingStanding, movingDucking, standingStill, duckingStill, _default);
	}	

	return PLUGIN_HANDLED;
}

public getRecoil(id)
{
	new recVal = get_pcvar_num(g_RecoilCvar);
	new zeroSpreadFsVal = get_pcvar_num(g_zeroSpreadFirstShotCvar);

	client_print_color(id, print_team_red, "^4RECOIL = ^3%d% ^1| ^4PRIMEIRO TIRO CERTEIRO = ^3%s", recVal, zeroSpreadFsVal == 0 ? "OFF" : "ON");
	
	return PLUGIN_HANDLED;
}

bool:normalizeWeaponName(const input[], output[], outputLength)
{
	copy(output, outputLength, input);
	strtolower(output);

	for (new index = 0; index < sizeof(g_WeaponNames); ++index)
	{
		if (equal(output, g_WeaponNames[index]))
			return true;
	}

	return false;
}

bool:parseSpreadMultiplier(const input[], &Float:value)
{
	new length = strlen(input);
	if (!length)
		return false;
		
	new i = 0;
	for (; i < length; ++i) {
		// Then we have a digit other than 0.
		if (input[i] != '0' && input[i] != '.')
			break;
	}

	// Then we did not break out of the loop,
	// i.e, did not find a digit other than 0.
	if (i == length) {
		value = 0;
		return true;
	}

	value = str_to_float(input);	

	// Clamp.
	if (value < SPREAD_MULTIPLIER_MIN)
		value = SPREAD_MULTIPLIER_MIN;
	else if (value > SPREAD_MULTIPLIER_MAX)
		value = SPREAD_MULTIPLIER_MAX;

	// log_amx("PARSED %s TO %.2f", input, value);

	// If value == 0.0, an error occurred in str_to_float.
	return value != 0.0;
}

// Persist changes.
saveConfig()
{
	static const configHeader[][] =
	{
		"// Weapon Tuning Plugin via Metamod (currently spread & recoil)",
		"//",
		"// This plugin controls recoil via KickBack hook from CBasePlayerWeapon",
		"// https://github.com/rehlds/ReGameDLL_CS/blob/679973265e1ac99a43193119e0da212ee568f5f9/regamedll/dlls/API/CSPlayerWeapon.cpp#L46",
		"//",
		"// This plugin controls the vecSpread variable of FireBullets3",
		"// https://github.com/rehlds/ReGameDLL_CS/blob/679973265e1ac99a43193119e0da212ee568f5f9/regamedll/dlls/cbase.cpp#L1268",
		"//",
		"// Available configuraitions:",
		"//  wt_recoil_mm                (0-100)     Defines the recoil strength in percentage, 0 being no recoil at all, 50 means half the original recoil, and 100 means normal game recoil.",
		"//  wt_zeroSpreadFirstShot_mm   (0 or 1)    Whether the first shot is always dead center with 0 spread. This gives a great feeling during gameplay becuase pixel shots are always accurate.",
		"//  wt_spread_mm                (see below) Detailed control of a specific weapon spread mechanics.",
		"//",
		"// wt_spread_mm cVar explanation:",
		"//",
		"// 1. To ignore a configuration scenario just leave/set it to 1.0, which is the default spread;",
		"// 2. To completly remove spread of weapon shots, you may leave/set everything to 1.0, and set default to 0.0 for all;",
		"// 3. The wt_zeroSpreadFirstShot_mm cvar works by completely removing the spread from the very first shot provided that the player is not moving in any way. It is a pixel-perfect first shot.",
		"//",
		"// weapon               Name of the weapon WITHOUT prefix, e.g., ak47 (see all available weapons name below).",
		"// in air               Spread mitigation while player is airborne.",
		"// moving & standing    Spread mitigation while player is moving and NOT ducking.",
		"// moving & ducking     Spread mitigation while player is moving AND ducking.",
		"// standing still       Spread mitigation while player is NOT moving and NOT ducking.",
		"// ducking still        Spread mitigation while player is NOT moving AND ducking.",
		"// default              Default spread mitigation when the player a settings does not apply to a specific situation, or for other situations not mentioned here.",
		"//",
		"// Usage: wt_spread_mm <weapon_name> <in_air> <moving_standing> <moving_ducking> <standing_still> <ducking_still> <default>",
		"// Example: wt_spread_mm ak47 -1.0 -1.0 0.8 0.75 0.70 -1.0",
		"//",
		"//     COMMAND     |    WEAPON    |   IN    |  MOVING &  |  MOVING &  |  STANDING  |  DUCKING  |  DEFAULT  |",
		"//                 |              |   AIR   |  STANDING  |  DUCKING   |   STILL    |   STILL   |           |",
		"//",
	};

	static const path[] = "/plugins/weapon_tuning.cfg";

	new configsDir[128];
	new dirLen = get_configsdir(configsDir, charsmax(configsDir));

	// addons/amxmodx/configs/plugins/weapon_tuning.cfg.
	formatex(configsDir[dirLen], charsmax(configsDir) - dirLen, "%s", path);

	// log_amx("%s", path);
	// log_amx("%s", configsDir);
	log_to_file(logFile, "Opening config file at %s", configsDir);
	new file = fopen(configsDir,"w");

	if (!file) {
		log_to_file(logFile, "Failed to open config file to persist new config.");
		return false;
	}

	new i = 0;
	for(; i < sizeof(configHeader); ++i) {
		fprintf(file, "%s^n", configHeader[i]);
	}

	// Write weapons.
	new bool:appendShotgunWarning = false;
	for(i = 0; i < sizeof(g_WeaponNames); ++i) {

		switch(i)
		{
			case 0:
			{
				fputs(file, "// PISTOLS^n");
			}
			case 6:
			{
				fputs(file, "^n// SNIPER RIFLES^n");
			}
			case 10:
			{
				fputs(file, "^n// RIFLES^n");
			}
			case 16:
			{
				fputs(file, "^n// SUBMACHINE GUNS^n");
			}
			case 21:
			{
				fputs(file, "^n// MACHINE GUN^n");
				appendShotgunWarning = true;
			}
		}

		fprintf(file, "     wt_spread_mm      %-10s    %.2f        %.2f         %.2f         %.2f         %.2f        %.2f^n",
			g_WeaponNames[i], g_Spread[i][InAir], g_Spread[i][MovingStanding], g_Spread[i][MovingDucking],
			g_Spread[i][StandingStill], g_Spread[i][DuckingStill], g_Spread[i][DefaultSpread]);

		if (appendShotgunWarning) {
			fputs(file, "^n// !NOT SUPPORTED! !NOT SUPPORTED! !NOT SUPPORTED! !NOT SUPPORTED!^n");
			fputs(file, "// SHOTGUNS M3 AND XM1014 ARE NOT SUPPORTED BECAUSE THEY DO NOT USE SPREAD!^n");
			fputs(file, "// !NOT SUPPORTED! !NOT SUPPORTED! !NOT SUPPORTED! !NOT SUPPORTED!^n");
			appendShotgunWarning = false;
		}
	}

	fputs(file, "^n// Make first shots always 100% accurate provided the player is not moving in any way.^n");
	fprintf(file, "wt_zeroSpreadFirstShot_mm ^"%d^"^n", get_pcvar_num(g_zeroSpreadFirstShotCvar));
	
	fputs(file, "^n// Percentage of each shot's original recoil to keep. 100 keeps default recoil; 0 removes it.^n");
	fprintf(file, "wt_recoil_mm ^"%d^"", get_pcvar_num(g_RecoilCvar));

	fclose(file);	
}

bool:loadConfig()
{
	static path[] = "/plugins/weapon_tuning.cfg";

	new configsDir[128];
	new dirLen = get_configsdir(configsDir, charsmax(configsDir));

	// addons/amxmodx/configs/plugins/weapon_tuning.cfg.
	formatex(configsDir[dirLen], charsmax(configsDir) - dirLen, "%s", path);

	log_to_file(logFile, "Opening config file at %s", configsDir);
	new file = fopen(configsDir,"r");

	if (!file) {
		log_to_file(logFile, "Failed to open config file. Check file exists.");
		return false;
	}

	new line[512];
	new lineNum = 0;
	new len = 0;
	while (fgets(file, line, charsmax(line)))
	{
		lineNum++;

		trim(line);

		len = strlen(line);

		// Skip comments.
		if (len < 10 || (line[0] == '/' && line[1] == '/') || line[0] == ';')
			continue;

		new cmd[27]; // wt_spread_mm or wt_zeroSpreadFirstShot_mm or wt_recoil_mm
		new arg1[16]; // Might be weapon name or a cvar value.;

		log_to_file(logFile, "Parsing line %d: ^"%s^"", lineNum, line);
		new tokens = parse(line, cmd, charsmax(cmd), arg1, charsmax(arg1));
		// log_amx("Parsed tokens: %d", tokens);

		if (tokens >= 2) {

			new val = 0;
			new bool:invalidVal = false;

			if (arg1[0] != '0') {
				val = str_to_num(arg1);
				if (val == 0) {
					invalidVal = true;
				}
			}

			if (equal(cmd, "wt_zeroSpreadFirstShot_mm")) {

				if (invalidVal) {
					log_to_file(logFile, "Invalid value for command ^"%s^". Found: %s. Expected: 0 or 1. Defaulting to 0.", cmd, arg1);
					val = 0;
				}
				else if (val > 1 || val < 0) {
					log_to_file(logFile, "Invalid value for command ^"%s^". Found: %s. Expected: 0 or 1. Defaulting to 1.", cmd, arg1);
					if (val < 0) val = 0;
					else if (val > 1) val = 1;
				}

				log_to_file(logFile, "Set wt_zeroSpreadFirstShot_mm to %d", val);
				set_pcvar_num(g_zeroSpreadFirstShotCvar, val);
			}
			else if (equal(cmd, "wt_recoil_mm")) {

				if (invalidVal) {
					log_to_file(logFile, "Invalid value for command ^"%s^". Found: %s. Expected: integer from 0 to 100. Defaulting to 100 (default recoil).", cmd, arg1);
					val = 100;
				}
				else if (val > 100 || val < 0) {
					log_to_file(logFile, "Invalid value for command ^"%s^". Found: %s. Expected: integer from 0 to 100. Defaulting to 100 (default recoil).", cmd, arg1);
					if (val < 0) val = 0;
					else if (val > 100) val = 100;
				}

				log_to_file(logFile, "Set wt_recoil_mm to %d", val);
				set_pcvar_num(g_RecoilCvar, val);
			}
			else if (equal(cmd, "wt_spread_mm")) {

				new SpreadWeaponIndex:weaponIndex;
				new Float:inAir;
				new Float:movingStanding;
				new Float:movingDucking;
				new Float:standingStill;
				new Float:duckingStill;
				new Float:_default;

				if (parseSpreadEntry(line,
					weaponIndex,
					inAir,
					movingStanding, movingDucking,
					standingStill, duckingStill,
					_default)) {

					g_Spread[_:weaponIndex][InAir] = inAir;
					g_Spread[_:weaponIndex][MovingStanding] = movingStanding;
					g_Spread[_:weaponIndex][MovingDucking] = movingDucking;
					g_Spread[_:weaponIndex][StandingStill] = standingStill;
					g_Spread[_:weaponIndex][DuckingStill] = duckingStill;
					g_Spread[_:weaponIndex][DefaultSpread] = _default;

					log_to_file(logFile, "Set wt_spread_mm [%s] to [%.2f] [%.2f] [%.2f] [%.2f] [%.2f] [%.2f]",
						g_WeaponNames[_:weaponIndex], inAir, movingStanding, movingDucking, standingStill, duckingStill, _default);
				}
				else {
					// misformatted line.
					log_to_file(logFile, "Failed to parse config line %d: ^"%s^". Ignoring line.", lineNum, line);
					continue;
				}	
			}
			else {
				log_to_file(logFile, "Failed to parse config line %d: ^"%s^". Unknown command ^"%s^".", lineNum, line, cmd);
				continue;
			}
		}
	}

	fclose(file);

	log_to_file(logFile, "Configuration loading complete.");

	return true;
}

bool:parseSpreadEntry(const entry[], &SpreadWeaponIndex:weaponIndex,
	&Float:inAir,
	&Float:movingStanding, &Float:movingDucking,
	&Float:standingStill, &Float:duckingStill,
	&Float:_default)
{

	// log_amx("Parsing spread entry ^"%s^"", entry);

	new cmd[16]; // wt_spread_mm or wt_zeroSpreadFirstShot_mm or wt_recoil_mm
	new weaponName[16]; // Might be weapon name or a cvar value.
	new strInAir[6]; // -0.00 (max len = 5, same for the others)
	new strMovingStanding[6];
	new strMovingDucking[6];
	new strStandingStill[6];
	new strDuckingStill[6];
	new strDefault[6];

	new tokens = parse(entry,
		cmd, charsmax(cmd),
		weaponName, charsmax(weaponName),
		strInAir, charsmax(strInAir),
		strMovingStanding, charsmax(strMovingStanding),
		strMovingDucking, charsmax(strMovingDucking),
		strStandingStill, charsmax(strStandingStill),
		strDuckingStill, charsmax(strDuckingStill),
		strDefault, charsmax(strDefault));

	if (tokens != 8) {
		log_to_file(logFile, "Spread entry ^"%s^" has an invalid number of parameters. Expected: 7. Found: %d.", entry, tokens - 1);
		return false;
	}

	if (!equal(cmd, "wt_spread_mm")) {
		log_to_file(logFile, "Spread entry ^"%s^" has invalid syntax. Expected: ^"wt_spread_mm^". Found: ^"%s^".", entry, cmd);
		return false;
	}

	// log_amx("Parsed spread entry: [%s] [%s] [%s] [%s] [%s] [%s] [%s] [%s]", cmd, weaponName,
	// 		strInAir, strMovingStanding, strMovingDucking,
	// 		strStandingStill, strDuckingStill, strDefault);

	new i = 0;
	for (; i < sizeof(g_WeaponNames); ++i)
	{
		if (equali(weaponName, g_WeaponNames[i]))
			break;
	}

	if (i == sizeof(g_WeaponNames)) {
		log_to_file(logFile, "Unsupported weapon ^"%s^". Ignoring config entry ^"%s^"", weaponName, entry);
		return false;
	}
	
	weaponIndex = SpreadWeaponIndex:i;

	new Float:_inAir;
	if (!parseSpreadMultiplier(strInAir, _inAir)) {
		log_to_file(logFile, "Invalid value for ^"In Air^" parameter. Found: ^"%s^". Expected: greater or equal to 0.0", strInAir);
		return false;
	}
	
	new Float:_movingStanding;
	if (!parseSpreadMultiplier(strMovingStanding, _movingStanding)) {
		log_to_file(logFile, "Invalid value for ^"Moving Standing^" parameter. Found: ^"%s^". Expected: greater or equal to 0.0", strMovingStanding);
		return false;
	}

	new Float:_movingDucking;
	if (!parseSpreadMultiplier(strMovingDucking, _movingDucking)) {
		log_to_file(logFile, "Invalid value for ^"Moving Ducking^" parameter. Found: ^"%s^". Expected: greater or equal to 0.0", strMovingDucking);
		return false;
	}

	new Float:_standingStill;
	if (!parseSpreadMultiplier(strStandingStill, _standingStill)) {
		log_to_file(logFile, "Invalid value for ^"Standing Still^" parameter. Found: ^"%s^". Expected: greater or equal to 0.0", strStandingStill);
		return false;
	}

	new Float:_duckingStill;
	if (!parseSpreadMultiplier(strDuckingStill, _duckingStill)) {
		log_to_file(logFile, "Invalid value for ^"Ducking Still^" parameter. Found: ^"%s^". Expected: greater or equal to 0.0", strDuckingStill);
		return false;
	}

	new Float:__default;
	if (!parseSpreadMultiplier(strDefault, __default)) {
		log_to_file(logFile, "Invalid value for ^"Default^" parameter. Found: ^"%s^". Expected: greater or equal to 0.0", strDefault);
		return false;
	}

	inAir = _inAir;
	movingStanding = _movingStanding;
	movingDucking = _movingDucking;
	standingStill = _standingStill;
	duckingStill = _duckingStill;
	_default = __default;

	return true;
}