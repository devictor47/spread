#include <extdll.h> // Needed by a lot of things;
#include "extdef.h" // C_DLLEXPORT;
#include "eiface.h" // enginefuncs_t;
#include "progdefs.h" // globalvars_t;
#include "mutil.h" // mutil_funcs_t;

#include <meta_api.h> // To define Metamod global variables (gpMetaGlobals, gpMetaUtilFuncs, gpGamedllFuncs);
#include "plinfo.h" // plugin_info_t;
#include "rehlds_api_plugin.h" // ReHLDS setup;
#include "regame_api_plugin.h" // ReGame_DLL setup;
#include "weapon_tuning.h" // This plugin;
#include "enginecallbacks.h" // Define g_engfuncs; SERVER_PRINT;

#ifdef _WIN32
#include <minwindef.h> // WINAPI;
#endif

#include <cstring> // memcpy;

enginefuncs_t g_engfuncs; // Definition expected by enginecallback.h
globalvars_t* gpGlobals;  // Definition expected by qstring.h
meta_globals_t* gpMetaGlobals; // Definition expected by meta_api.h
mutil_funcs_t* gpMetaUtilFuncs; // Definition expected by meta_api.h
gamedll_funcs_t* gpGamedllFuncs; // Definition expected by meta_api.h

plugin_info_t Plugin_info =
{
	META_INTERFACE_VERSION,		// ifvers
	"Weapon Tuning",		// name
	"1.0",						// version
	__DATE__,					// date
	"VictorOak",				// author
	"https://github.com/devictor47/spread/",	// url
	"WEAP TUNING",				// logtag
	PT_ANYTIME,					// (when) loadable
	PT_ANYTIME,					// (when) unloadable
};

// Whatever we don't use, just set it to NULL.
// Must provide at least one function returning a standard HL function table,
// from either the following standard HLSDK functions.
// These are defined in the engine_api.cpp and dllapi.cpp files.
META_FUNCTIONS gMetaFunctionTable =
{
	NULL,						//            pfnGetEntityAPI	HL SDK; called before game DLL;
	NULL,						//       pfnGetEntityAPI_Post	META; called after game DLL;
	NULL,						//           pfnGetEntityAPI2	HL SDK2; called before game DLL;
	GetEntityAPI2_Post,			//      pfnGetEntityAPI2_Post	META; called after game DLL;
	NULL,						//      pfnGetNewDLLFunctions	HL SDK2; called before game DLL;
	NULL,						// pfnGetNewDLLFunctions_Post	META; called after game DLL;
	NULL,						//      pfnGetEngineFunctions	META; called before HL engine;
	NULL,						// pfnGetEngineFunctions_Post	META; called after HL engine;
};

DLL_FUNCTIONS g_DllFunctionTable_Post =
{
	NULL,					// pfnGameInit
	NULL,					// pfnSpawn
	NULL,					// pfnThink
	NULL,					// pfnUse
	NULL,					// pfnTouch
	NULL,					// pfnBlocked
	NULL,					// pfnKeyValue
	NULL,					// pfnSave
	NULL,					// pfnRestore
	NULL,					// pfnSetAbsBox
	NULL,					// pfnSaveWriteFields
	NULL,					// pfnSaveReadFields
	NULL,					// pfnSaveGlobalState
	NULL,					// pfnRestoreGlobalState
	NULL,					// pfnResetGlobalState
	NULL,					// pfnClientConnect
	NULL,					// pfnClientDisconnect
	NULL,					// pfnClientKill
	NULL,					// pfnClientPutInServer
	NULL,					// pfnClientCommand
	NULL,					// pfnClientUserInfoChanged
	NULL,					// pfnServerActivate
	NULL,					// pfnServerDeactivate
	NULL,					// pfnPlayerPreThink
	NULL,					// pfnPlayerPostThink
	NULL,					// pfnStartFrame
	NULL,					// pfnParmsNewLevel
	NULL,					// pfnParmsChangeLevel
	NULL,					// pfnGetGameDescription
	NULL,					// pfnPlayerCustomization
	NULL,					// pfnSpectatorConnect
	NULL,					// pfnSpectatorDisconnect
	NULL,					// pfnSpectatorThink
	NULL,					// pfnSys_Error
	NULL,					// pfnPM_Move
	NULL,					// pfnPM_Init
	NULL,					// pfnPM_FindTextureType
	NULL,					// pfnSetupVisibility
	NULL,					// pfnUpdateClientData
	NULL,					// pfnAddToFullPack
	NULL,					// pfnCreateBaseline
	NULL,					// pfnRegisterEncoders
	NULL,					// pfnGetWeaponData
	NULL,					// pfnCmdStart
	NULL,					// pfnCmdEnd
	NULL,					// pfnConnectionlessPacket
	NULL,					// pfnGetHullBounds
	NULL,					// pfnCreateInstancedBaselines
	NULL,					// pfnInconsistentFile
	NULL,					// pfnAllowLagCompensation
};

void GameInit();

// Receive engine function table and globals from the engine.
// This appears to be the _first_ DLL routine called by the engine,
// so we do some setup operations here.
//
// This function is called by GoldSrc engine
// when it is loading the game/mod dll (mp.dll for Counter-Strike).
// It is used to send engine functions to the game so that
// it can do what it wants with them, such as order a render,
// load a file, play a sound, whatever the game engine provides.
// GoldSrc was not made public by Valve, but you can get an
// idea of how it works here https://github.com/headcrab-junkyard/OGS/.
//
// Metamod will receive this call AS IF IT WAS the mod to be loaded,
// and will then sit between the engine and the actual game,
// allowing it to modify function calls etc.
// Metamod itself calls this function for each metamod plugin it loads as if it was the engine (https://github.com/rehlds/Metamod-R/blob/273618c684807a4b3809b23d2fd7a3353b46a5da/metamod/src/mplugin.cpp#L795).
//
// After this, we go on to the meta_api file.
C_DLLEXPORT void WINAPI GiveFnptrsToDll(enginefuncs_t* pengfuncsFromEngine, globalvars_t* pGlobals)
{
	memcpy(&g_engfuncs, pengfuncsFromEngine, sizeof(enginefuncs_t));
	gpGlobals = pGlobals;
}

// Metamod requesting info about this plugin:
// 
// interfaceVersion		(given)		interface version (META_INTERFACE_VERSION) Metamod is using;
//           plinfo		(requested)	struct with info about plugin that we must provide to Metamod;
//   pMetaUtilFuncs		(given)		table of utility functions provided by Metamod;
C_DLLEXPORT int Meta_Query(char *interfaceVersion, plugin_info_t **plinfo, mutil_funcs_t *pMetaUtilFuncs)
{
	*plinfo = &Plugin_info;
	gpMetaUtilFuncs = pMetaUtilFuncs;
	return TRUE;
}

// Metamod attaching plugin to the server.
//
//             now		(given)		current phase, i.e., during map, during changelevel, or at startup;
//  pFunctionTable		(requested)	table of function tables this plugin catches;
//       pMGlobals		(given)		global vars from Metamod;
//   pGamedllFuncs		(given)		copy of function tables from game dll;
// https://github.com/rehlds/Metamod-R/blob/273618c684807a4b3809b23d2fd7a3353b46a5da/metamod/src/mplugin.cpp#L969
C_DLLEXPORT int Meta_Attach(PLUG_LOADTIME now, META_FUNCTIONS *pFunctionTable, meta_globals_t *pMGlobals, gamedll_funcs_t *pGamedllFuncs)
{
	// Store global vars from metamod.
	if (!pMGlobals) {
		LOG_ERROR(PLID, "Meta_Attach called with null pMGlobals\n");
		return(FALSE);
	}
	gpMetaGlobals = pMGlobals;

	// Give metamod function tables this plugin catches.
	if (!pFunctionTable) {
		LOG_ERROR(PLID, "Meta_Attach called with null pFunctionTable\n");
		return(FALSE);
	}
	memcpy(pFunctionTable, &gMetaFunctionTable, sizeof(META_FUNCTIONS));

	if (!pGamedllFuncs) {
		LOG_ERROR(PLID, "Meta_Attach called with null pGamedllFuncs\n");
		return(FALSE);
	}
	gpGamedllFuncs = pGamedllFuncs;

	// This is not plain metamod only.
	// Whole nother universe here.
	if (meta_init_rehlds_api())
		SERVER_PRINT("ReHLDS API successfully initialized.\n");

	if (meta_init_regamedll_api())
		SERVER_PRINT("ReGameDLL API successfully initialized.\n");

	return TRUE;
}

// Metamod detaching plugin from the server.
// 
//    now	(given)	current phase, i.e., during map, during changelevel, or at startup;
// reason	(given)	why detaching (refresh, console unload, forced unload, etc);
C_DLLEXPORT int Meta_Detach(PLUG_LOADTIME now, PL_UNLOAD_REASON reason)
{
	meta_stop_regamedll_api();
	return TRUE;
}

// META; called after game DLL.
C_DLLEXPORT int GetEntityAPI2_Post(DLL_FUNCTIONS* pFunctionTable, int* interfaceVersion)
{
	if (!pFunctionTable) {
		LOG_ERROR(PLID, "%s called with null pFunctionTable\n", __FUNCTION__);
		return FALSE;
	}

	if (*interfaceVersion != INTERFACE_VERSION) {
		LOG_ERROR(PLID, "%s version mismatch; Expected: %d. Found: %d\n", __FUNCTION__, *interfaceVersion, INTERFACE_VERSION);
		*interfaceVersion = INTERFACE_VERSION;
		return FALSE;
	}

	g_DllFunctionTable_Post.pfnGameInit = GameInit;

	memcpy(pFunctionTable, &g_DllFunctionTable_Post, sizeof(DLL_FUNCTIONS));

	return TRUE;
}

void GameInit()
{
	if (gWeaponTuning.RegisterCvar())
	{
		LOG_CONSOLE(PLID, "CVAR & SERVER COMMAND REGISTERED");
		// LOG_CONSOLE(PLID, "Executing addons/spread/spread.cfg...");
		// char execCfgCmd[] = "exec addons/spread/spread.cfg\n";
		// g_engfuncs.pfnServerCommand(execCfgCmd);
	}
	else {
		LOG_CONSOLE(PLID, "CVAR & SERVER COMMAND FAILED TO REGISTER. HALTING PLUGIN LOAD.");
	}

	RETURN_META(MRES_IGNORED);
}
