#include <Arduino.h>
#include "../Common.h"
#include "ReButtonClient2.h"

#include <ReButton.h>
#include <DevkitDPSClient.h>

const char* REPORTED_PROPERTY_ACTION_COUNT = "actionCount";

ReButtonClient2::ReButtonClient2()
{
	_IsDeviceTwinReceived = false;
	Action = ACTION_NONE;
	ActionCount = 0;
	CustomMessageEnable = false;
}

bool ReButtonClient2::IsDeviceTwinReceived() const
{
	return _IsDeviceTwinReceived;
}

bool ReButtonClient2::ConnectIoTHubWithDPS(const char* endpoint, const char* scopeId, const char* deviceId, const char* sasKey)
{
	Serial.println("Device provisioning...");

	char sasKeyCopy[strlen(sasKey) + 1];
	strcpy(sasKeyCopy, sasKey);

	DevkitDPSSetLogTrace(false);
	DevkitDPSSetAuthType(DPS_AUTH_SYMMETRIC_KEY);
	if (!DevkitDPSClientStart(endpoint, scopeId, deviceId, sasKeyCopy, NULL, 0))
	{
		Serial.println("DPS client has failed.");
		return false;
	}

	String connectionString;
	connectionString = "HostName=";
	connectionString += DevkitDPSGetIoTHubURI();
	connectionString += ";DeviceId=";
	connectionString += DevkitDPSGetDeviceID();
	connectionString += ";SharedAccessKey=";
	connectionString += sasKey;

	return ConnectIoTHub(connectionString.c_str());
}

void ReButtonClient2::SendTelemetryActionAsync()
{
	JSON_Value* telemetryValue = json_value_init_object();
	JSON_Object* telemetryObject = json_value_get_object(telemetryValue);

	String actionNumStr(ActionToActionNum(Action));
	json_object_set_string(telemetryObject, "actionNum", actionNumStr.c_str());
	json_object_set_string(telemetryObject, "message", ActionToMessage(Action));
	switch (Action)
	{
	case ACTION_1:
		json_object_set_string(telemetryObject, "singleClick", ActionToMessage(Action));
		break;
	case ACTION_2:
		json_object_set_string(telemetryObject, "doubleClick", ActionToMessage(Action));
		break;
	case ACTION_3:
		json_object_set_string(telemetryObject, "tripleClick", ActionToMessage(Action));
		break;
	case ACTION_10:
		json_object_set_string(telemetryObject, "longPress", ActionToMessage(Action));
		break;
	case ACTION_11:
		json_object_set_string(telemetryObject, "superLongPress", ActionToMessage(Action));
		break;
	}
	json_object_set_number(telemetryObject, "batteryVoltage", ReButton::ReadPowerSupplyVoltage());
	json_object_set_number(telemetryObject, "actionCount", ActionCount);

	AzureDeviceClient::SendTelemetryAsync(telemetryObject);

	json_value_free(telemetryValue);

	if (CustomMessageEnable)
	{
		JSON_Value* rootValue = json_parse_string(Config.CustomMessageJson);
		if (rootValue != NULL)
		{
			JSON_Object* rootObject = json_value_get_object(rootValue);
			AzureDeviceClient::SendTelemetryAsync(rootObject);
		}
		json_value_free(rootValue);
	}

	SendPropertyActionCountAsync();
}

void ReButtonClient2::ReceivedProperties(JSON_Object* reportedObject)
{
	if (json_object_dothas_value_of_type(reportedObject, REPORTED_PROPERTY_ACTION_COUNT, JSONNumber))
	{
		ActionCount = json_object_dotget_number(reportedObject, REPORTED_PROPERTY_ACTION_COUNT);
		Serial.print("ReportedActionCount = ");
		Serial.println(ActionCount);
	}
}

void ReButtonClient2::ReceivedSettings(JSON_Object* desiredObject, bool complete)
{
	if (strlen(Config.CustomMessagePropertyName) >= 1 && json_object_dothas_value_of_type(desiredObject, stringformat("%s.value", Config.CustomMessagePropertyName).c_str(), JSONBoolean))
	{
		int customMessageEnable = json_object_dotget_boolean(desiredObject, stringformat("%s.value", Config.CustomMessagePropertyName).c_str());
		switch (customMessageEnable)
		{
		case 1:
			CustomMessageEnable = true;
			break;
		case 0:
			CustomMessageEnable = false;
			break;
		}

		Serial.print("CustomMessageEnable = ");
		Serial.println(CustomMessageEnable ? "true" : "false");

		SendPropertyCustomMessageEnableAsync();
	}
}

void ReButtonClient2::SendPropertyCustomMessageEnableAsync()
{
	JSON_Value* reportedValue = json_value_init_object();
	JSON_Object* reportedObject = json_value_get_object(reportedValue);

	json_object_dotset_boolean(reportedObject, Config.CustomMessagePropertyName, CustomMessageEnable);

	AzureDeviceClient::UpdateReportedPropertyAsync(reportedObject);

	json_value_free(reportedValue);
}

void ReButtonClient2::SendPropertyActionCountAsync()
{
	JSON_Value* reportedValue = json_value_init_object();
	JSON_Object* reportedObject = json_value_get_object(reportedValue);

	json_object_dotset_number(reportedObject, REPORTED_PROPERTY_ACTION_COUNT, ActionCount);

	AzureDeviceClient::UpdateReportedPropertyAsync(reportedObject);

	json_value_free(reportedValue);
}

void ReButtonClient2::DeviceTwinReceived(JSON_Object* deviceTwinObject)
{
	Serial.println("DeviceTwinReceived()");
	auto jsonString = json_serialize_to_string(json_object_get_wrapping_value(deviceTwinObject));
	Serial.println(jsonString);
	json_free_serialized_string(jsonString);

	ReceivedProperties(json_object_get_object(deviceTwinObject, "reported"));
	ReceivedSettings(json_object_get_object(deviceTwinObject, "desired"), true);

	_IsDeviceTwinReceived = true;
}

void ReButtonClient2::DesiredPropertyReceived(JSON_Object* desiredObject)
{
	Serial.println("DesiredPropertyReceived()");
	auto jsonString = json_serialize_to_string(json_object_get_wrapping_value(desiredObject));
	Serial.println(jsonString);
	json_free_serialized_string(jsonString);

	ReceivedSettings(desiredObject, false);
}