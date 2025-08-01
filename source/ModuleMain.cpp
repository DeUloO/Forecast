#include <YYToolkit/YYTK_Shared.hpp>
#include <set>
using namespace Aurie;
using namespace YYTK;
static YYTKInterface* g_ModuleInterface = nullptr;

// Valid values for weather are 0 for sunny, 1 for 
static int s_weather = -1;
std::set<int> weather_types = {-1, 0, 1, 2, 3};

RValue& UpdateClock(
	IN CInstance* Self,
	IN CInstance* Other,
	OUT RValue& Result,
	IN int ArgumentCount,
	IN RValue** Arguments
)
{
	const auto original = reinterpret_cast<PFUNC_YYGMLScript>(MmGetHookTrampoline(g_ArSelfModule, "ClockUpdate"));
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

	}
	return original(
		Self,
		Other,
		Result,
		ArgumentCount,
		Arguments
	);

}

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

	// Theoretically unneeded Print-debugging
	// g_ModuleInterface->Print(CM_LIGHTGREEN, "gml_Script_start_new_weather_event@WeatherManager@Weather: %i", ArgumentCount);
	// g_ModuleInterface->Print(CM_LIGHTGREEN, "gml_Script_start_new_weather_event@WeatherManager@Weather: %i", Arguments[0]->ToInt64());

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
			"[Forecast v1.0.0] Failed to get YYTK_Main interface! Reason: %s. Is YYToolkit installed?",
			AurieStatusToString(last_status)
		);

		return last_status;
	}

	

	CScript* script_3 = nullptr;
	last_status = g_ModuleInterface->GetNamedRoutinePointer(
		"gml_Script_start_new_weather_event@WeatherManager@Weather",
		reinterpret_cast<PVOID*>(&script_3)
	);

	if (!AurieSuccess(last_status))
	{
		g_ModuleInterface->PrintError(
			__FILE__,
			__LINE__,
			"[Forecast v1.0.0] Failed to find gml_Script_start_new_weather_event@WeatherManager@Weather! Reason: %s",
			AurieStatusToString(last_status)
		);

		return last_status;
	}

	last_status = MmCreateHook(
		g_ArSelfModule,
		"StartWeatherEvent",
		script_3->m_Functions->m_ScriptFunction,
		StartWeatherEvent,
		nullptr
	);

	if (!AurieSuccess(last_status))
	{
		g_ModuleInterface->PrintError(
			__FILE__,
			__LINE__,
			"[Forecast v1.0.0] Failed to hook gml_Script_start_new_weather_event@WeatherManager@Weather! Reason: %s",
			AurieStatusToString(last_status)
		);

		return last_status;
	}

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
			"[Forecast v1.0.0] Failed to find gml_Script_update@Clock@Clock! Reason: %s",
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
			"[Forecast v1.0.0] Failed to set a hook on gml_Script_update@Clock@Clock! Reason: %s",
			AurieStatusToString(last_status)
		);

		return last_status;
	}

	return AURIE_SUCCESS;
}
