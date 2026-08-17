#include <extdll.h> // Needed by a lot of things;
#include <meta_api.h> // To use Metamod global variable gpMetaUtilFuncs;
#include "regamedll_api.h" // IReGameApi, ReGameFuncs_t, IReGameHookchains;
#include "gamerules.h" // CGameRules;
#include "regame_api_plugin.h"// Declaration of extern global vars;
#include "weapon_tuning.h" // This plugin;
#include "vector.h" // Vector;

IReGameApi* g_ReGameApi;
const ReGameFuncs_t* g_ReGameFuncs;
IReGameHookchains* g_ReGameHookchains;
CGameRules* g_pGameRules = nullptr;
static bool g_ReGameHooksRegistered = false;

static bool regamedll_api_init()
{
	const char* szGameDLLModule = gpMetaUtilFuncs->pfnGetGameInfo(PLID, GINFO_DLL_FULLPATH);
	if (!szGameDLLModule)
		return false;

	CSysModule* gameModule = Sys_GetModuleHandle(szGameDLLModule);
	if (!gameModule)
		return false;

	CreateInterfaceFn ifaceFactory = Sys_GetFactory(gameModule);
	if (!ifaceFactory)
		return false;

	int retCode = 0;
	g_ReGameApi = (IReGameApi*)ifaceFactory(VRE_GAMEDLL_API_VERSION, &retCode);
	if (!g_ReGameApi)
	{
		return false;
	}

	int majorVersion = g_ReGameApi->GetMajorVersion();
	int minorVersion = g_ReGameApi->GetMinorVersion();

	if (majorVersion != REGAMEDLL_API_VERSION_MAJOR)
	{
		gpMetaUtilFuncs->pfnLogConsole(PLID, "[%s]: ReGameDLL API major version mismatch. Expected: %d. Found: %d\n", Plugin_info.logtag, REGAMEDLL_API_VERSION_MAJOR, majorVersion);

		// need to notify that it is necessary to update the ReGameDLL.
		if (majorVersion < REGAMEDLL_API_VERSION_MAJOR)
		{
			gpMetaUtilFuncs->pfnLogConsole(PLID, "[%s]: Please update the ReGameDLL up to a major version API >= %d\n", Plugin_info.logtag, REGAMEDLL_API_VERSION_MAJOR);
		}

		// need to notify that it is necessary to update the module.
		else if (majorVersion > REGAMEDLL_API_VERSION_MAJOR)
		{
			gpMetaUtilFuncs->pfnLogConsole(PLID, "[%s]: Please update the %s up to a major version API >= %d\n", Plugin_info.logtag, Plugin_info.logtag, majorVersion);
		}

		return false;
	}

	if (minorVersion < REGAMEDLL_API_VERSION_MINOR)
	{
		gpMetaUtilFuncs->pfnLogConsole(PLID, "[%s]: ReGameDLL API minor version mismatch. Expected: %d. Found: %d\n", Plugin_info.logtag, REGAMEDLL_API_VERSION_MINOR, minorVersion);

		gpMetaUtilFuncs->pfnLogConsole(PLID, "[%s]: Please update the ReGameDLL up to a minor version API >= %d\n", Plugin_info.logtag, REGAMEDLL_API_VERSION_MINOR);

		return false;
	}

	// Safe check CCSEntity API interface version
	if (!g_ReGameApi->BGetICSEntity(CSENTITY_API_INTERFACE_VERSION))
	{
		gpMetaUtilFuncs->pfnLogConsole(PLID, "[%s]: Interface CCSEntity API version '%s' not found.\n", Plugin_info.logtag, CSENTITY_API_INTERFACE_VERSION);

		if (g_ReGameApi->BGetICSEntity("CSENTITY_API_INTERFACE_VERSION002"))
			gpMetaUtilFuncs->pfnLogConsole(PLID, "[%s]: Please update ReGameDLL to the latest version.\n", Plugin_info.logtag);
		else
			gpMetaUtilFuncs->pfnLogConsole(PLID, "[%s]: Please update ReAPI to the latest version.\n", Plugin_info.logtag);

		return false;
	}
	
	// Safe check GameRules API interface version
	if (!g_ReGameApi->BGetIGameRules(GAMERULES_API_INTERFACE_VERSION))
	{
		gpMetaUtilFuncs->pfnLogConsole(PLID, "[%s]: Interface GameRules API version '%s' not found.\n", Plugin_info.logtag, GAMERULES_API_INTERFACE_VERSION);

		return false;
	}

	g_ReGameFuncs = g_ReGameApi->GetFuncs();
	g_ReGameHookchains = g_ReGameApi->GetHookchains();

	g_ReGameHookchains->InstallGameRules()->registerHook(InstallGameRules);
	g_ReGameHookchains->CBaseEntity_FireBullets3()->registerHook(HookFireBullets3);
	g_ReGameHookchains->CBasePlayerWeapon_KickBack()->registerHook(HookKickBack);
	g_ReGameHooksRegistered = true;

	return true;
}

static bool regamedll_api_stop()
{
	if (g_ReGameHooksRegistered)
	{
		g_ReGameHookchains->InstallGameRules()->unregisterHook(InstallGameRules);
		g_ReGameHookchains->CBaseEntity_FireBullets3()->unregisterHook(HookFireBullets3);
		g_ReGameHookchains->CBasePlayerWeapon_KickBack()->unregisterHook(HookKickBack);
		g_ReGameHooksRegistered = false;
	}

	return true;
}

bool meta_init_regamedll_api()
{
	return regamedll_api_init();
}

bool meta_stop_regamedll_api()
{
	return regamedll_api_stop();
}

CGameRules* InstallGameRules(IReGameHook_InstallGameRules* chain)
{
	auto gamerules = chain->callNext();
	g_pGameRules = gamerules;
	return gamerules;
}

void HookKickBack(IReGameHook_CBasePlayerWeapon_KickBack* chain, CBasePlayerWeapon* pEntity, float up_base, float lateral_base, float up_modifier, float lateral_modifier, float up_max, float lateral_max, int direction_change)
{
	if (!pEntity || !pEntity->pev || !pEntity->pev->owner || !(pEntity->pev->owner->v.flags & FL_CLIENT))
	{
		chain->callNext(pEntity, up_base, lateral_base, up_modifier, lateral_modifier, up_max, lateral_max, direction_change);
		return;
	}

	entvars_t* const owner = &pEntity->pev->owner->v;
	const Vector punchAngleBefore = owner->punchangle;
	chain->callNext(pEntity, up_base, lateral_base, up_modifier, lateral_modifier, up_max, lateral_max, direction_change);

	const float multiplier = gWeaponTuning.GetRecoilMultiplier();
	if (multiplier >= 1.0f)
		return;

	// Scale only this shot's kick, preserving recoil accumulated before KickBack.
	owner->punchangle.x = punchAngleBefore.x + (owner->punchangle.x - punchAngleBefore.x) * multiplier;
	owner->punchangle.y = punchAngleBefore.y + (owner->punchangle.y - punchAngleBefore.y) * multiplier;
	owner->punchangle.z = punchAngleBefore.z + (owner->punchangle.z - punchAngleBefore.z) * multiplier;
}

Vector& HookFireBullets3(IReGameHook_CBaseEntity_FireBullets3* chain, CBaseEntity* pEntity, Vector& vecSrc, Vector& vecDirShooting, float vecSpread, float flDistance, int iPenetration, int iBulletType, int iDamage, float flRangeModifier, entvars_t* pevAttacker, bool bPistol, int shared_rand)
{
	//using std::chrono::high_resolution_clock;
	//using std::chrono::duration_cast;
	//using std::chrono::duration;
	//using std::chrono::milliseconds;

	//auto t1 = high_resolution_clock::now();
	vecSpread = gWeaponTuning.CalcSpread(pEntity, vecSpread);
	//auto t2 = high_resolution_clock::now();

	//auto ms_int = duration_cast<milliseconds>(t2 - t1);
	//duration<double, std::milli> ms_double = t2 - t1;
	//LOG_CONSOLE(PLID, "TIME: %g", ms_double.count());

	return chain->callNext(pEntity, vecSrc, vecDirShooting, vecSpread, flDistance, iPenetration, iBulletType, iDamage, flRangeModifier, pevAttacker, bPistol, shared_rand);
}
