#include <YYToolkit/YYTK_Shared.hpp>
#include <set>
using namespace Aurie;
using namespace YYTK;
static YYTKInterface* g_ModuleInterface = nullptr;

static const char* const VERSION = "1.2.0";
static const char* const GML_SCRIPT_CREATE_NOTIFICATION = "gml_Script_create_notification";
static const std::string FORECAST_ACCEPTED = "Notifications/Mods/Forecast/updatedForecast";


static int s_weather = -1;
// Valid values for weather are 0 for sunny, 1 for rainy, 2 for thunder, 3 special season weather. -1 "disables" the mod.
std::set<int> weather_types = {-1, 0, 1, 2, 3};


// Taken from AnnaNomolys' StatueOfBoons mod
void CreateNotification(std::string notification_localization_str, CInstance* Self, CInstance* Other)
{
	CScript* gml_script_create_notification = nullptr;
	g_ModuleInterface->GetNamedRoutinePointer(
		GML_SCRIPT_CREATE_NOTIFICATION,
		(PVOID*)&gml_script_create_notification
	);

	RValue result;
	RValue notification = RValue(notification_localization_str);
	RValue* notification_ptr = &notification;
	gml_script_create_notification->m_Functions->m_ScriptFunction(
		Self,
		Other,
		result,
		1,
		{ &notification_ptr }
	);
}


RValue& UpdateClock(
	IN CInstance* Self,
	IN CInstance* Other,
	OUT RValue& Result,
	IN int ArgumentCount,
	IN RValue** Arguments
)
{
	const auto original = reinterpret_cast<PFUNC_YYGMLScript>(MmGetHookTrampoline(g_ArSelfModule, "ClockUpdate"));
	// This part is a modified version of the popup done by ArchieUwU in Time of Mistria
	if (GetAsyncKeyState(VK_NEXT) & 1)
	{
		// Attempt to get a number from the user.
		RValue integer_result = g_ModuleInterface->CallBuiltin(
			"get_integer",
			{
				"Please decide tomorrows weather.\r\n"
				"(0 Sun, 1 Rain, 2 Thunder, 3 Special)\r\n",
				s_weather
			}
		);

		if (integer_result.m_Kind == VALUE_UNDEFINED || integer_result.m_Kind == VALUE_UNSET)
			return Result;

		if (!weather_types.contains(integer_result.ToInt64()))
		{
			g_ModuleInterface->GetRunnerInterface().YYError(
				"\r\nInvalid weather choose from a number from 0 to 3!\r\n"
			);
		}

		s_weather = static_cast<int16_t>(integer_result.ToInt64());
		CreateNotification(FORECAST_ACCEPTED, Self, Other);

	}
	return original(
		Self,
		Other,
		Result,
		ArgumentCount,
		Arguments
	);

}

// Callback hooked into "gml_Script_start_new_weather_event@WeatherManager@Weather", early return if s_weather is -1.
RValue& StartWeatherEvent(
	IN CInstance* Self,
	IN CInstance* Other,
	OUT RValue& Result,
	IN int ArgumentCount,
	IN RValue** Arguments
)
{
	const auto original = reinterpret_cast<PFUNC_YYGMLScript>(MmGetHookTrampoline(g_ArSelfModule, "StartWeatherEvent"));

	CInstance* global_instance = nullptr;
	if (!AurieSuccess(g_ModuleInterface->GetGlobalInstance(&global_instance)))
	{
		return original(
			Self,
			Other,
			Result,
			ArgumentCount,
			Arguments
		);
	}

	if (s_weather == -1)
		return original(
			Self,
			Other,
			Result,
			ArgumentCount,
			Arguments
		);

	RValue weather = s_weather;
	Arguments[0] = &weather;

	return original(
		Self,
		Other,
		Result,
		ArgumentCount,
		Arguments
	);

	// The debugging I used for this project
	// g_ModuleInterface->Print(CM_LIGHTGREEN, "gml_Script_start_new_weather_event@WeatherManager@Weather: %i", ArgumentCount);
	// g_ModuleInterface->Print(CM_LIGHTGREEN, "gml_Script_start_new_weather_event@WeatherManager@Weather: %i", Arguments[0]->ToInt64());

}


RValue& WeatherTomorrow(
	IN CInstance* Self,
	IN CInstance* Other,
	OUT RValue& Result,
	IN int ArgumentCount,
	IN RValue** Arguments
)
{
	const auto original = reinterpret_cast<PFUNC_YYGMLScript>(MmGetHookTrampoline(g_ArSelfModule, "WeatherTomorrow"));

	CInstance* global_instance = nullptr;
	if (!AurieSuccess(g_ModuleInterface->GetGlobalInstance(&global_instance)))
	{
		return original(
			Self,
			Other,
			Result,
			ArgumentCount,
			Arguments
		);
	}


	original(
		Self,
		Other,
		Result,
		ArgumentCount,
		Arguments
	);

	if (s_weather == -1)
		return original(
			Self,
			Other,
			Result,
			ArgumentCount,
			Arguments
		);

	Result = s_weather;

	//g_ModuleInterface->Print(CM_LIGHTGREEN, "gml_Script_weather_tomorrow: %i", ArgumentCount);
	//g_ModuleInterface->Print(CM_LIGHTGREEN, "gml_Script_weather_tomorrow: %i", Result.ToInt64());

	return Result;
}



EXPORTED AurieStatus ModuleInitialize(
	IN AurieModule* Module,
	IN const fs::path& ModulePath
)
{
	UNREFERENCED_PARAMETER(ModulePath);

	AurieStatus last_status = AURIE_SUCCESS;

	last_status = ObGetInterface(
		"YYTK_Main",
		(AurieInterfaceBase*&)(g_ModuleInterface)
	);

	if (!AurieSuccess(last_status))
	{
		g_ModuleInterface->PrintError(
			__FILE__,
			__LINE__,
			"[Forecast v%s] Failed to get YYTK_Main interface! Reason: %s. Is YYToolkit installed?",
			VERSION,
			AurieStatusToString(last_status)
		);

		return last_status;
	}

	

	CScript* start_weather_event = nullptr;
	last_status = g_ModuleInterface->GetNamedRoutinePointer(
		"gml_Script_start_new_weather_event@WeatherManager@Weather",
		reinterpret_cast<PVOID*>(&start_weather_event)
	);

	if (!AurieSuccess(last_status))
	{
		g_ModuleInterface->PrintError(
			__FILE__,
			__LINE__,
			"[Forecast v%s] Failed to find gml_Script_start_new_weather_event@WeatherManager@Weather! Reason: %s",
			VERSION,
			AurieStatusToString(last_status)
		);

		return last_status;
	}

	last_status = MmCreateHook(
		g_ArSelfModule,
		"StartWeatherEvent",
		start_weather_event->m_Functions->m_ScriptFunction,
		StartWeatherEvent,
		nullptr
	);

	if (!AurieSuccess(last_status))
	{
		g_ModuleInterface->PrintError(
			__FILE__,
			__LINE__,
			"[Forecast v%s] Failed to hook gml_Script_start_new_weather_event@WeatherManager@Weather! Reason: %s",
			VERSION,
			AurieStatusToString(last_status)
		);

		return last_status;
	}

	CScript* weather_tomorrow = nullptr;
	last_status = g_ModuleInterface->GetNamedRoutinePointer(
		"gml_Script_weather_tomorrow",
		reinterpret_cast<PVOID*>(&weather_tomorrow)
	);

	if (!AurieSuccess(last_status))
	{
		g_ModuleInterface->PrintError(
			__FILE__,
			__LINE__,
			"[Forecast v%s] Failed to find gml_Script_weather_tomorrow! Reason: %s",
			VERSION,
			AurieStatusToString(last_status)
		);

		return last_status;
	}

	last_status = MmCreateHook(
		g_ArSelfModule,
		"WeatherTomorrow",
		weather_tomorrow->m_Functions->m_ScriptFunction,
		WeatherTomorrow,
		nullptr
	);

	if (!AurieSuccess(last_status))
	{
		g_ModuleInterface->PrintError(
			__FILE__,
			__LINE__,
			"[Forecast v%s] Failed to hook gml_Script_weather_tomorrow! Reason: %s",
			VERSION,
			AurieStatusToString(last_status)
		);

		return last_status;
	}

	// Taken from Time of Mistria
	CScript* clock_update_script = nullptr;
	last_status = g_ModuleInterface->GetNamedRoutinePointer(
		"gml_Script_update@Clock@Clock",
		reinterpret_cast<PVOID*>(&clock_update_script)
	);

	if (!AurieSuccess(last_status))
	{
		g_ModuleInterface->PrintError(
			__FILE__,
			__LINE__,
			"[Forecast v%s] Failed to find gml_Script_update@Clock@Clock! Reason: %s",
			VERSION,
			AurieStatusToString(last_status)
		);

		return last_status;
	}

	last_status = MmCreateHook(
		g_ArSelfModule,
		"ClockUpdate",
		clock_update_script->m_Functions->m_ScriptFunction,
		UpdateClock,
		nullptr
	);

	if (!AurieSuccess(last_status))
	{
		g_ModuleInterface->PrintError(
			__FILE__,
			__LINE__,
			"[Forecast v%s] Failed to set a hook on gml_Script_update@Clock@Clock! Reason: %s",
			VERSION,
			AurieStatusToString(last_status)
		);

		return last_status;
	}

	return AURIE_SUCCESS;
}
