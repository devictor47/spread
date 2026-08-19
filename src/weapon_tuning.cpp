#include "weapon_tuning.h"
#include "regame_api_plugin.h"
#include "rehlds_api_plugin.h"
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>

#ifndef _WIN32
#include <iostream>
#include <sstream>
#include <iomanip>
#endif

CWeaponTuning gWeaponTuning;


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
int sc_ZeroSpread = 0;
int sc_Airborne = 0;
int sc_StillStanding = 0;
int sc_StillDucking = 0;
int sc_MovingStanding = 0;
int sc_MovingDucking = 0;
int sc_Default = 0;
auto last = std::chrono::steady_clock::now();
#define DEBUG_CONSOLE(...) LOG_CONSOLE(PLID, __VA_ARGS__)
#define LOG_FILE(msg) (this->LogToFile(msg))

void CWeaponTuning::LogToFile(const std::string& message) {

	if (this->m_logFile.is_open())
		this->m_logFile << message << std::endl;
}

void CWeaponTuning::SetupLog()
{
	auto now = std::chrono::system_clock::now();
	std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
	std::string nowStr(std::ctime(&now_time_t));

	const std::string dir = "cstrike/addons/weapon_tuning";
	const std::string file = dir + "/weapon_tuning_log.txt";

	if (!this->m_logFile.is_open())
	{
		this->m_logFile.open(file, std::ios::app);

		// Failed? Try creating directory and reopening.
		if (!this->m_logFile)
		{
			this->m_logFile.open("cstrike/addons/weapon_tuning_log.txt", std::ios::app);

			// Clear stream state before retrying.
			this->m_logFile.clear();

			this->m_logFile.open(file, std::ios::app);
		}

		if (!this->m_logFile)
		{
			LOG_ERROR(PLID, "ERROR OPENING WEAPON TUNING LOG FILE");
			LOG_CONSOLE(PLID, "ERROR OPENING WEAPON TUNING LOG FILE");
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

#define MOVEMENT_BUTTONS  (IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT)

// Horizontal movement threshold.
// 1.0f is usually enough to ignore tiny physics jitter while still
// detecting real movement immediately.
#define MOVEMENT_EPSILON 1.0f

#define HAS_NO_VELOCITY_2D(speed2D) \
    ((speed2D) < MOVEMENT_EPSILON)

#define HAS_VELOCITY_2D(speed2D) \
    ((speed2D) >= MOVEMENT_EPSILON)

// ReGameDLL increments m_iShotsFired before spread calculation,
// therefore first bullet == 1, not 0.
#define IS_FIRST_SHOT(pWeapon) \
    ((pWeapon)->m_iShotsFired == 1)

FORCEDINLINE bool ShouldForceZeroSpreadFirstShot(CBasePlayer*, bool, float);

namespace
{
	constexpr float MIN_SPREAD_MULTIPLIER = 0.0f;
	constexpr float MAX_SPREAD_MULTIPLIER = 100.0f;

	bool ParseSpreadMultiplier(const char* input, float& value)
	{
		if (!input || !*input)
			return false;

		char* end = nullptr;
		errno = 0;
		const float parsedValue = std::strtof(input, &end);
		if (end == input || *end != '\0' || errno == ERANGE || !std::isfinite(parsedValue))
			return false;

		value = std::max(MIN_SPREAD_MULTIPLIER, std::min(parsedValue, MAX_SPREAD_MULTIPLIER));
		return true;
	}

	bool NormalizeWeaponName(const char* input, std::string& weaponName)
	{
		if (!input || !*input)
			return false;

		weaponName = input;
		std::transform(weaponName.begin(), weaponName.end(), weaponName.begin(), [](unsigned char character) {
			return static_cast<char>(std::tolower(character));
		});

		constexpr char WEAPON_PREFIX[] = "weapon_";
		constexpr size_t PREFIX_LEN = sizeof(WEAPON_PREFIX) - 1;

		if (weaponName.compare(0, PREFIX_LEN, WEAPON_PREFIX) == 0)
			weaponName.erase(0, PREFIX_LEN);

		struct WeaponName
		{
			const char* input;
			const char* canonical;
		};

		static const WeaponName supportedWeapons[] =
		{
			{ "glock18", "glock18" }, { "glock", "glock18" },
			{ "usp", "usp" },
			{ "p228", "p228" },
			{ "deagle", "deagle" },
			{ "elite", "elite" },
			{ "fiveseven", "fiveseven" }, { "fn57", "fiveseven" },
			{ "awp", "awp" },
			{ "scout", "scout" },
			{ "g3sg1", "g3sg1" },
			{ "sg550", "sg550" },
			{ "sg55", "sg550" },
			{ "galil", "galil" },
			{ "famas", "famas" },
			{ "ak47", "ak47" },
			{ "m4a1", "m4a1" },
			{ "sg552", "sg552" },
			{ "aug", "aug" },
			{ "mac10", "mac10" },
			{ "tmp", "tmp" },
			{ "mp5navy", "mp5navy" },
			{ "mp5", "mp5navy" }, { "ump45", "ump45" },
			{ "p90", "p90" },
			{ "m249", "m249" }
		};

		for (const WeaponName& supportedWeapon : supportedWeapons)
		{
			if (weaponName == supportedWeapon.input)
			{
				weaponName = WEAPON_PREFIX + std::string(supportedWeapon.canonical);
				return true;
			}
		}

		return false;
	}
}

bool CWeaponTuning::RegisterCvar()
{
	char cmd_name[] = "wt_spread_mm";
	g_engfuncs.pfnAddServerCommand(cmd_name, this->SetWeapon);

	if (!CVAR_GET_POINTER(m_ZeroSpreadFirstShot.name))
		CVAR_REGISTER(&m_ZeroSpreadFirstShot);

	m_pZeroSpreadFirstShotCvar = CVAR_GET_POINTER(m_ZeroSpreadFirstShot.name);
	if (!m_pZeroSpreadFirstShotCvar)
	{
		LOG_ERROR(PLID, "Failed to register \"%s\" cvar.", m_ZeroSpreadFirstShot.name);
		return false;
	}

	if (!CVAR_GET_POINTER(m_RecoilMultiplierCvar.name))
		CVAR_REGISTER(&m_RecoilMultiplierCvar);

	m_pRecoilMultiplierCvar = CVAR_GET_POINTER(m_RecoilMultiplierCvar.name);
	if (!m_pRecoilMultiplierCvar)
	{
		LOG_ERROR(PLID, "Failed to register \"%s\" cvar.", m_RecoilMultiplierCvar.name);
		return false;
	}

	return true;
}

float CWeaponTuning::GetRecoilMultiplier() const
{
	if (!m_pRecoilMultiplierCvar)
		return 1.0f;

	const float recoilPercent = m_pRecoilMultiplierCvar->value;
	if (!std::isfinite(recoilPercent))
		return 1.0f;

	if (recoilPercent <= 0.0f)
		return 0.0f;
	if (recoilPercent >= 100.0f)
		return 1.0f;

	return recoilPercent / 100.0f;
}

void CWeaponTuning::SetWeapon()
{
	if (g_engfuncs.pfnCmd_Argc() == 8)
	{
		std::string weaponName;
		if (!NormalizeWeaponName(g_engfuncs.pfnCmd_Argv(1), weaponName))
		{
			LOG_CONSOLE(PLID, "Unsupported weapon \"%s\"", g_engfuncs.pfnCmd_Argv(1));
			return;
		}

		float multipliers[6];
		for (int index = 0; index < 6; ++index)
		{
			if (!ParseSpreadMultiplier(g_engfuncs.pfnCmd_Argv(index + 2), multipliers[index]))
			{
				LOG_CONSOLE(PLID, "Invalid spread multiplier \"%s\"", g_engfuncs.pfnCmd_Argv(index + 2));
				return;
			}
		}

		if (!g_ReGameApi)
		{
			LOG_ERROR(PLID, "ReGameDLL API is unavailable; cannot configure weapon spread.");
			return;
		}

		auto slot = g_ReGameApi->GetWeaponSlot(weaponName.c_str());
		if (!slot || (slot->slot != PRIMARY_WEAPON_SLOT && slot->slot != PISTOL_SLOT))
		{
			LOG_CONSOLE(PLID, "Unsupported weapon \"%s\"", g_engfuncs.pfnCmd_Argv(1));
			return;
		}

		gWeaponTuning.AddWeapon(slot->id,
			multipliers[0], // InAir
			multipliers[1], // MovingStanding
			multipliers[2], // MovingDucking
			multipliers[3], // StandingStill
			multipliers[4], // DuckingStill
			multipliers[5]);// Default

		LOG_CONSOLE(PLID, "Spread control for \"%s\" set successfully", g_engfuncs.pfnCmd_Argv(1));
	}
	else
	{
		LOG_CONSOLE(PLID, "[%s] Usage: %s <weapon_name> <in_air> <moving_standing> <moving_ducking> <standing_still> <ducking_still> <default>. Example: \"%s ak47 -1.0 -1.0 0.8 0.75 0.70 -1.01\".", Plugin_info.logtag, g_engfuncs.pfnCmd_Argv(0), g_engfuncs.pfnCmd_Argv(0));
	}
}

void CWeaponTuning::AddWeapon(int WeaponIndex, float InAir, float MovingStanding, float MovingDucking, float StandingStill, float DuckingStill, float Default)
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

float CWeaponTuning::CalcSpread(CBaseEntity* pEntity, float vecSpread)
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
			LOG_FILE("ZERO SPREAD " + std::to_string(sc_ZeroSpread));
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
		// Since the spread values are clamped in [0, 1],
		// 1 being default, we ignore if the value is 1.
		if (weaponCfg.InAir >= 0.0f)
		{
			//DEBUG_CONSOLE("[%s] (airborne) [OLD SP: %f] [NEW SP: %f]", __FUNCTION__, vecSpread, vecSpread * weaponCfg.InAir);
			return vecSpread * weaponCfg.InAir;
		}
	}
	else
	{
		if (ShouldForceZeroSpreadFirstShot(pPlayer, this->m_pZeroSpreadFirstShotCvar && this->m_pZeroSpreadFirstShotCvar->value > 0.0f, speed2D))
		{
#ifdef DO_DEBUG
			sc_ZeroSpread += 1;
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

FORCEDINLINE bool ShouldForceZeroSpreadFirstShot(CBasePlayer* pPlayer, bool zeroSpreadFirstShot, float speed2D)
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

	if (!zeroSpreadFirstShot)
		return false;

	if ((pPlayer->pev->button & MOVEMENT_BUTTONS) != 0)
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
