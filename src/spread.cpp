#include "spread.h"
#include "regame_api_plugin.h"
#include "rehlds_api_plugin.h"
#include <chrono>
#include <ctime>

#ifndef _WIN32
#include <iostream>
#include <sstream>
#include <iomanip>
#endif

CSpread gSpread;


#ifndef DO_DEBUG

#ifdef _MSC_VER
#define FORCEDINLINE __forceinline
#else
#define FORCEDINLINE __attribute__((always_inline))
#endif

#else

#define FORCEDINLINE

#endif

#ifdef DO_DEBUG

// Persist these throughout the whole server life to measure statistics.
int sc_DeadCenter = 0;
int sc_Airborne = 0;
int sc_StillStanding = 0;
int sc_StillDucking = 0;
int sc_MovingStanding = 0;
int sc_MovingDucking = 0;
int sc_Default = 0;
auto last = std::chrono::steady_clock::now();
#define DEBUG_CONSOLE(...) LOG_CONSOLE(PLID, __VA_ARGS__)
#define LOG_FILE(msg) (this->LogToFile(msg))

void CSpread::LogToFile(const std::string& message) {

	if (this->m_logFile.is_open())
		this->m_logFile << message << std::endl;
}

void CSpread::SetupLog()
{
	auto now = std::chrono::system_clock::now();
	std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
	std::string nowStr(std::ctime(&now_time_t));

	const std::string dir = "cstrike/addons/spread";
	const std::string file = dir + "/spread_log.txt";

	if (!this->m_logFile.is_open())
	{
		this->m_logFile.open(file, std::ios::app);

		// Failed? Try creating directory and reopening.
		if (!this->m_logFile)
		{
			this->m_logFile.open("cstrike/addons/spread_log.txt", std::ios::app);

			// Clear stream state before retrying.
			this->m_logFile.clear();

			this->m_logFile.open(file, std::ios::app);
		}

		if (!this->m_logFile)
		{
			LOG_ERROR(PLID, "ERROR OPENING SPREAD LOG FILE");
			LOG_CONSOLE(PLID, "ERROR OPENING SPREAD LOG FILE");
			return;
		}

		LOG_FILE("Log Started - " + nowStr);
	}
	else
	{
		LOG_FILE("Server Reloaded - " + nowStr);
	}
}

#else

#define DEBUG_CONSOLE(...)
#define LOG_FILE(msg)

#endif

#define IS_STANDING(flags) (!((flags) & FL_DUCKING))
//#define IS_ON_GROUND(flags) ((flags)&FL_ONGROUND)
#define IS_AIRBORNE(flags) (!((flags) & FL_ONGROUND))

// Horizontal movement threshold.
// 1.0f is usually enough to ignore tiny physics jitter while still
// detecting real movement immediately.
#define MOVEMENT_EPSILON 3.0f

#define HAS_NO_VELOCITY_2D(speed2D) \
    ((speed2D) < MOVEMENT_EPSILON)

#define HAS_VELOCITY_2D(speed2D) \
    ((speed2D) >= MOVEMENT_EPSILON)

// ReGameDLL increments m_iShotsFired before spread calculation,
// therefore first bullet == 1, not 0.
#define IS_FIRST_SHOT(pWeapon) \
    ((pWeapon)->m_iShotsFired == 1)

FORCEDINLINE bool ShouldForceDeadCenterShot(CBasePlayer*, bool, float);

bool CSpread::RegisterCvar()
{
	char cmd_name[] = "spread_wpn";
	g_engfuncs.pfnAddServerCommand(cmd_name, this->SetWeapon);

	const char* cvarName = "spread_deadCenterFirstShot";
	cvar_t* cvarPtr = CVAR_GET_POINTER(cvarName);
	if (!cvarPtr)
	{
		// Defines whether the first shot is always dead center when standing or ducking, but not moving.
		cvar_t CvarData;
		char str_val[] = "0";
		CvarData = { cvarName, str_val, 0, 0.0f, NULL };
		CVAR_REGISTER(&CvarData);
		cvarPtr = CVAR_GET_POINTER(cvarName);

		if (cvarPtr) {
			this->m_pDeadCenterFirstShotCvar = cvarPtr;
		}
		else {
			LOG_ERROR(PLID, "Failed to register \"%s\" cvar: an error occurred!", cvarName);
			LOG_CONSOLE(PLID, "Failed to register \"%s\" cvar: an error occurred!", cvarName);
			this->m_pDeadCenterFirstShotCvar = NULL;
			return false;
		}
	}
	else {
		LOG_ERROR(PLID, "Failed to register \"%s\" cvar: already exists!", cvarName);
		LOG_CONSOLE(PLID, "Failed to register \"%s\" cvar: already exists!", cvarName);
		return false;
	}

	return true;
}

void CSpread::SetWeapon()
{
	if (g_engfuncs.pfnCmd_Argc() >= 8)
	{
		std::string weaponName = g_engfuncs.pfnCmd_Argv(1);

		if (weaponName.find("weapon_") == std::string::npos)
		{
			weaponName = "weapon_" + weaponName;
		}

		if (!weaponName.empty())
		{
			auto slot = g_ReGameApi->GetWeaponSlot(weaponName.c_str());

			if (slot &&
				(slot->slot == PRIMARY_WEAPON_SLOT ||
				slot->slot == PISTOL_SLOT))
			{
				gSpread.AddWeapon(slot->id,
					std::stof(g_engfuncs.pfnCmd_Argv(2)),  // InAir
					std::stof(g_engfuncs.pfnCmd_Argv(3)),  // MovingStanding
					std::stof(g_engfuncs.pfnCmd_Argv(4)),  // MovingDucking
					std::stof(g_engfuncs.pfnCmd_Argv(5)),  // StandingStill
					std::stof(g_engfuncs.pfnCmd_Argv(6)),  // DuckingStill
					std::stof(g_engfuncs.pfnCmd_Argv(7))); // Default

				LOG_CONSOLE(PLID, "Spread control for \"%s\" set successfully", g_engfuncs.pfnCmd_Argv(1));
			}
			else
			{
				LOG_CONSOLE(PLID, "Unknown weapon \"%s\"", g_engfuncs.pfnCmd_Argv(1));
			}
		}
	}
	else
	{
		LOG_CONSOLE(PLID, "[%s] Usage: %s <weapon_name> <in_air> <moving_standing> <moving_ducking> <standing_still> <ducking_still> <default>. Example: \"%s ak47 -1.0 -1.0 0.8 0.75 0.70 -1.01\".", Plugin_info.logtag, g_engfuncs.pfnCmd_Argv(0), g_engfuncs.pfnCmd_Argv(0));
	}
}

void CSpread::AddWeapon(int WeaponIndex, float InAir, float MovingStanding, float MovingDucking, float StandingStill, float DuckingStill, float Default)
{
	if (WeaponIndex >= 0 && WeaponIndex < MAX_WEAPONS) {

		this->m_rgWeaponsCfg[WeaponIndex] =
			WEAPON_SPREAD_CFG{
				true,
				InAir,
				MovingStanding,
				MovingDucking,
				StandingStill,
				DuckingStill,
				Default
		};
	}
}

float CSpread::CalcSpread(CBaseEntity* pEntity, float vecSpread)
{

#ifdef DO_DEBUG	

	// Log every 30 seconds.
	if (std::chrono::duration<double>(std::chrono::steady_clock::now() - last).count() > 30)
	{
		int numPl = 0;
		for (int i = 1; i <= gpGlobals->maxClients; ++i)
		{
			edict_t* pEdict = g_engfuncs.pfnPEntityOfEntIndex(i);

			if (pEdict && !pEdict->free && pEdict->v.flags & FL_CLIENT)
				numPl++;
		}

		if (numPl >= 7)
		{
			auto now = std::chrono::system_clock::now();
			std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
			std::ostringstream oss;

#ifdef _WIN32
			std::tm* tmPtr = std::localtime(&currentTime);
			oss << std::put_time(tmPtr, "%H:%M:%S");
#else
			std::tm tmStruct;
			localtime_r(&currentTime, &tmStruct);
			oss << std::setfill('0') << std::setw(2) << tmStruct.tm_hour << ":"
				<< std::setfill('0') << std::setw(2) << tmStruct.tm_min << ":"
				<< std::setfill('0') << std::setw(2) << tmStruct.tm_sec;
#endif

			LOG_FILE(oss.str());
			LOG_FILE("AIRBORNE " + std::to_string(sc_Airborne));
			LOG_FILE("DEADCENTER " + std::to_string(sc_DeadCenter));
			LOG_FILE("STILL STANDING " + std::to_string(sc_StillStanding));
			LOG_FILE("STILL DUCKING " + std::to_string(sc_StillDucking));
			LOG_FILE("MOVING STANDING " + std::to_string(sc_MovingStanding));
			LOG_FILE("MOVING DUCKING " + std::to_string(sc_MovingDucking));
			LOG_FILE("DEFAULT " + std::to_string(sc_Default));
		}

		if (this->m_logFile.is_open())
			this->m_logFile.flush();

		last = std::chrono::steady_clock::now();
	}
#endif

	CBasePlayer* pPlayer = static_cast<CBasePlayer*>(pEntity);
	if (!(pPlayer && pPlayer->m_pActiveItem))
	{
#ifdef DO_DEBUG
		if (!pPlayer)
			DEBUG_CONSOLE("[%s] !Player", __FUNCTION__);
		else if (!pPlayer->m_pActiveItem)
			DEBUG_CONSOLE("[%s] !Player->m_pActiveItem", __FUNCTION__);
#endif
		return vecSpread;
	}

	// See if current player weapon is configured.
	const int wepId = pPlayer->m_pActiveItem->m_iId;

	if (wepId < 0 || wepId >= MAX_WEAPONS)
		return vecSpread;

	const WEAPON_SPREAD_CFG weaponCfg = this->m_rgWeaponsCfg[wepId];
	
	if (!weaponCfg.IsValid)
		return vecSpread;

	const int flags = pPlayer->pev->flags;
	const auto pev = pPlayer->pev;
	const float speed2D = pev->velocity.Length2D();

	// Player is in the air.
	if (IS_AIRBORNE(flags))
	{
#ifdef DO_DEBUG			
		sc_Airborne += 1;
#endif
		if (weaponCfg.InAir >= 0.0f)
		{
			//DEBUG_CONSOLE("[%s] (airborne) [OLD SP: %f] [NEW SP: %f]", __FUNCTION__, vecSpread, vecSpread * weaponCfg.InAir);
			return vecSpread * weaponCfg.InAir;
		}
	}
	else
	{
		if (ShouldForceDeadCenterShot(pPlayer, this->m_pDeadCenterFirstShotCvar->value > 0.0f, speed2D))
		{
#ifdef DO_DEBUG
			sc_DeadCenter += 1;
#endif
			//DEBUG_CONSOLE("[%s] (first shot dead center) [OLD SP: %f] [NEW SP: %f]", __FUNCTION__, vecSpread, 0.0f);
			return 0.0f;
		}

		// If the player has any *horizontal* movement at all...
		if (HAS_VELOCITY_2D(speed2D))
		{
			// Player is standing.
			if (IS_STANDING(flags))
			{
#ifdef DO_DEBUG
				sc_MovingStanding += 1;
#endif
				if (weaponCfg.MovingStanding >= 0.0f)
				{
					//DEBUG_CONSOLE("[%s] (moving, standing) [OLD SP: %f] [NEW SP: %f]", __FUNCTION__, vecSpread, vecSpread * weaponCfg.MovingStanding);
					return vecSpread * weaponCfg.MovingStanding;
				}
			}
			else
			{
#ifdef DO_DEBUG
				sc_MovingDucking += 1;
#endif
				// Player is ducking.
				if (weaponCfg.MovingDucking >= 0.0f)
				{
					//DEBUG_CONSOLE("[%s] (moving, ducking) [OLD SP: %f] [NEW SP: %f]", __FUNCTION__, vecSpread, vecSpread * weaponCfg.MovingDucking);
					return vecSpread * weaponCfg.MovingDucking;
				}
			}
		}
		else
		{
			// Player is still/stationary/motionless.

			// Player is standing.
			if (IS_STANDING(flags))
			{
#ifdef DO_DEBUG
				sc_StillStanding += 1;
#endif
				if (weaponCfg.StandingStill >= 0.0f)
				{
					//DEBUG_CONSOLE("[%s] (still, standing) [OLD SP: %f] [NEW SP: %f]", __FUNCTION__, vecSpread, vecSpread * weaponCfg.StandingStill);
					return vecSpread * weaponCfg.StandingStill;
				}

			}
			else
			{
#ifdef DO_DEBUG
				sc_StillDucking += 1;
#endif
				// Player is ducking.
				if (weaponCfg.DuckingStill >= 0.0f)
				{
					//DEBUG_CONSOLE("[%s] (still, ducking) [OLD SP: %f] [NEW SP: %f]", __FUNCTION__, vecSpread, vecSpread * weaponCfg.DuckingStill);
					return vecSpread * weaponCfg.DuckingStill;
				}
			}
		}

	}

	// At this point the player may be airborne,
	// moving horizontally, or some settings
	// may not be set to override the spread.
	// However, we must be careful as to not
	// override the spread already mitigated
	// by previous conditions.
	if (weaponCfg.Default >= 0.0f)
	{
		//DEBUG_CONSOLE("[%s] (default) [OLD SP: %f] [NEW SP: %f]", __FUNCTION__, vecSpread, vecSpread * weaponCfg.Default);
#ifdef DO_DEBUG
		sc_Default += 1;
#endif
		return vecSpread * weaponCfg.Default;
	}

	return vecSpread;
}

FORCEDINLINE bool ShouldForceDeadCenterShot(CBasePlayer* pPlayer, bool firstShotDeadCenter, float speed2D)
{
	// SCOPE WEAPONS CONSIDERATIONS:
	// AWP, SCOUT
	// SG550 -> CT tectec
	// G3SG1 -> TR tectec
	// SG552 -> TR krieg (B-4-4)
	// AUG   -> CT bullpup (B-4-4)
	//
	//   AWP & SCOUT WITH ZOOM      -> always dead center, if cvar set, and still.
	//   AWP & SCOUT WITHOUT ZOOM   -> never force dead center, just mitigate the resulting spread, if configured.
	// For both of these, punchangle is always zero, even if holding mouse button.
	// 
	// SG550 & G3SG1 WITH ZOOM      -> dead center on first shot, if cvar set.
	// SG550 & G3SG1 WITHOUT ZOOM   -> never force dead center, just mitigate the resulting spread, if configured.
	// For both of these, punchangle is also always zero, even if holding mouse button.
	// 
	//   AUG & SG552 WITH ZOOM      -> dead center on first shot, if cvar set.
	//   AUG & SG552 WITHOUT ZOOM   -> allow dead center regardless of zooming situation.

	if (!firstShotDeadCenter)
		return false;

	CBasePlayerWeapon* pWeapon =
		static_cast<CBasePlayerWeapon*>(pPlayer->m_pActiveItem);

	if (!IS_FIRST_SHOT(pWeapon))
		return false;

	if (!HAS_NO_VELOCITY_2D(speed2D))
		return false;

	switch (pWeapon->m_iId)
	{
		case WEAPON_AWP:
		case WEAPON_SCOUT:
		case WEAPON_SG550:
		case WEAPON_G3SG1:
			// Scoped weapons require active zoom.
			return pPlayer->pev->fov != DEFAULT_FOV;

		default:
			return true;
	}
}