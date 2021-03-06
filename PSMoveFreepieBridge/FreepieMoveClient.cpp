/*
* Edited from test_console_client.cpp in PSMoveService
*/

#include "stdafx.h"
#include <iomanip>
//#include "../thirdparty/headers/FreePIE/freepie_io.h"
#include "../thirdparty/headers/glm/glm.hpp"
#include "../thirdparty/headers/glm/gtc/matrix_transform.hpp"
#include "../thirdparty/headers/glm/gtc/quaternion.hpp"
#include "../thirdparty/headers/glm/gtc/type_ptr.hpp"
#include "../thirdparty/headers/glm/gtx/euler_angles.hpp"
#include "FreepieMoveClient.h"

#include <stdio.h>
#include "FreePIE.h"

int g_iFreePIESlotIn = 0;
int g_iFreePIESlotOut = 1;
float g_fMultiplier = 1.0f;
bool g_bDebug = false;

float x_s[100];
float y_s[100];
float z_s[100];
int max_mean_idx = 1;
int mean_idx = 0;
float num_pos = 0;

// Average the lasts max_mean_idx measurements (adds lag, but removes some noise)
/*
void add_pos(float x, float y, float z, float *x_out, float *y_out, float *z_out) {
	if (num_pos < max_mean_idx) {
		*x_out = x;
		*y_out = y;
		*z_out = z;
		x_s[mean_idx] = x;
		y_s[mean_idx] = y;
		z_s[mean_idx] = z;
		mean_idx++;
		num_pos++;
		return;
	}
	mean_idx = (mean_idx + 1) % max_mean_idx;
	x_s[mean_idx] = x;
	y_s[mean_idx] = y;
	z_s[mean_idx] = z;
	// Compute the mean
	*x_out = 0;
	*y_out = 0;
	*z_out = 0;
	for (register int i = 0; i < num_pos; i++) {
		*x_out += x_s[i];
		*y_out += y_s[i];
		*z_out += z_s[i];
	}
	*x_out /= num_pos;
	*y_out /= num_pos;
	*z_out /= num_pos;
}
*/

FreepieMoveClient::FreepieMoveClient()
{
	InitFreePIE();
}

int FreepieMoveClient::run(
    eDeviceType deviceType, 
    int32_t deviceCount, 
    int32_t deviceIDs[], 
    PSMTrackingColorType bulbColors[], 
    int32_t freepieIndices[], 
    bool sendSensorData, 
    int triggerAxisIndex)
{
	// Attempt to start and run the client
	try
	{
		m_device_type = deviceType;

		if (deviceType == _deviceTypeHMD)
		{
			trackedHmdIDs = deviceIDs;
			trackedHmdCount = deviceCount;
		}
		else
		{
			trackedControllerIDs = deviceIDs;
			trackedControllerCount = deviceCount;
		}

		trackedFreepieIndices = freepieIndices;
		trackedBulbColors = bulbColors;
		m_sendSensorData = sendSensorData;
        m_triggerAxisIndex = triggerAxisIndex;

		if (startup())
		{
			while (m_keepRunning)
			{
				update();

				Sleep(1);
			}
		}
		else
		{
			std::cerr << "Failed to startup the Freepie Move Client" << std::endl;
		}
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	// Attempt to shutdown the client
	try
	{
		shutdown();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return 0;
}

void FreepieMoveClient::handle_client_psmove_event(PSMEventMessage::eEventType event_type)
{
	switch (event_type)
	{
	case PSMEventMessage::PSMEvent_connectedToService:
		std::cout << "FreepieMoveClient - Connected to service" << std::endl;
		if (m_device_type == _deviceTypeHMD)
		{
			init_hmd_views();
		}
		else
		{
			init_controller_views();
		}
		break;
	case PSMEventMessage::PSMEvent_failedToConnectToService:
		std::cout << "FreepieMoveClient - Failed to connect to service" << std::endl;
		m_keepRunning = false;
		break;
	case PSMEventMessage::PSMEvent_disconnectedFromService:
		std::cout << "FreepieMoveClient - Disconnected from service" << std::endl;
		m_keepRunning = false;
		break;
	case PSMEventMessage::PSMEvent_opaqueServiceEvent:
		std::cout << "FreepieMoveClient - Opaque service event(%d). Ignored." << static_cast<int>(event_type) << std::endl;
		break;
	case PSMEventMessage::PSMEvent_controllerListUpdated:
		if (m_device_type == _deviceTypeController)
		{
			std::cout << "FreepieMoveClient - Controller list updated. Reinitializing controller views." << std::endl;
			free_controller_views();
			init_controller_views();
		}
		else
		{
			std::cout << "FreepieMoveClient - Controller list updated. Ignored." << std::endl;
		}
		break;
	case PSMEventMessage::PSMEvent_trackerListUpdated:
		std::cout << "FreepieMoveClient - Tracker list updated. Ignored." << std::endl;
		break;
	case PSMEventMessage::PSMEvent_hmdListUpdated:        
		if (m_device_type == _deviceTypeHMD)
		{
			std::cout << "FreepieMoveClient - HMD list updated. Reinitializing HMD views." << std::endl;
			free_hmd_views();
			init_hmd_views();
		}
		else
		{
			std::cout << "FreepieMoveClient - HMD list updated. Ignored." << std::endl;
		}
		break;
	case PSMEventMessage::PSMEvent_systemButtonPressed:
		std::cout << "FreepieMoveClient - System button pressed. Ignored." << std::endl;
		break;
	default:
		std::cout << "FreepieMoveClient - unhandled event(%d)" << static_cast<int>(event_type) << std::endl;
		break;
	}
}

void FreepieMoveClient::handle_acquire_hmd(PSMResult resultCode, PSMHmdID trackedHmdIndex)
{
	if (resultCode == PSMResult_Success)
	{
		std::cout << "FreepieMoveClient - Acquired HMD "
			<< hmd_views[trackedHmdIndex]->HmdID << std::endl;
	}
	else
	{
		std::cout << "FreepieMoveClient - failed to acquire HMD " << std::endl;
		//m_keepRunning = false;
	}
}

void FreepieMoveClient::handle_acquire_controller(PSMResult resultCode, PSMControllerID trackedControllerIndex)
{
	if (resultCode == PSMResult_Success)
	{
		std::cout << "FreepieMoveClient - Acquired controller "
			<< controller_views[trackedControllerIndex]->ControllerID << std::endl;
	}
	else
	{
		std::cout << "FreepieMoveClient - failed to acquire controller " << std::endl;
		//m_keepRunning = false;
	}
}

bool FreepieMoveClient::startup()
{
	bool success = true;

	// Attempt to connect to the server
	if (success)
	{
		if (PSM_InitializeAsync("localhost", "9512") == PSMResult_Error)
		{
			std::cout << "FreepieMoveClient - Failed to initialize the client network manager" << std::endl;
			success = false;
		}
	}

	if (success)
	{
		last_report_fps_timestamp =
			std::chrono::duration_cast< std::chrono::milliseconds >(
				std::chrono::system_clock::now().time_since_epoch());
	}

	return success;
}

void FreepieMoveClient::update()
{
	// Process incoming/outgoing networking requests
	PSM_UpdateNoPollMessages();

	// Poll events queued up by the call to PSM_UpdateNoPollMessages()
	PSMMessage message;
	while (PSM_PollNextMessage(&message, sizeof(message)) == PSMResult_Success)
	{
		switch (message.payload_type)
		{
		case PSMMessage::_messagePayloadType_Response:
            {
                int trackedDeviceCount = (m_device_type == _deviceTypeHMD) ? trackedHmdCount : trackedControllerCount;

			    for (int i = 0; i < trackedDeviceCount; i++)
			    {
				    if (start_stream_request_ids[i] != -1 &&
					    message.response_data.request_id == start_stream_request_ids[i])
				    {
					    if (m_device_type == _deviceTypeHMD)
					    {
						    handle_acquire_hmd(message.response_data.result_code, i);
					    }
					    else
					    {
						    handle_acquire_controller(message.response_data.result_code, i);
					    }
					    start_stream_request_ids[i] = -1;
				    }
			    }
            }
			break;
		case PSMMessage::_messagePayloadType_Event:
			handle_client_psmove_event(message.event_data.event_type);
			break;
		}
	}

	if (m_device_type == _deviceTypeHMD)
	{
		for (int i = 0; i < trackedHmdCount; i++)
		{
			if (hmd_views[i] && hmd_views[i]->bValid)
			{
				std::chrono::milliseconds now =
					std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::system_clock::now().time_since_epoch());
				std::chrono::milliseconds diff = now - last_report_fps_timestamp;

				PSMPosef hmdPose;
				if (PSM_GetHmdPose(hmd_views[i]->HmdID, &hmdPose) == PSMResult_Success)
				{
					//freepie_io_6dof_data poseData;

					// Read yaw, pitch, roll from the g_iFreePIESlotIn slot:
					ReadFreePIE(g_iFreePIESlotIn);

					// Add the positional information:
					// Data seems to be in cms, let's scale down to meters:
					g_FreePIEData.x = g_fMultiplier * hmdPose.Position.x / 100.0f;
					g_FreePIEData.y = g_fMultiplier * hmdPose.Position.y / 100.0f;
					g_FreePIEData.z = g_fMultiplier * hmdPose.Position.z / 100.0f;

					if (g_bDebug)
						printf("HMD w[%d]: (%0.4f, %0.4f, %0.4f)\n", g_iFreePIESlotOut, g_FreePIEData.x, g_FreePIEData.y, g_FreePIEData.z);
					// Write everything to slot g_iFreePIESlotOut:
					WriteFreePIE(g_iFreePIESlotOut);
				}
			}
		}
	}

	if (m_device_type == _deviceTypeController)
	{
		for (int i = 0; i < trackedControllerCount; i++) 
		{
			if (controller_views[i] && controller_views[i]->bValid) {
				std::chrono::milliseconds now =
					std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::system_clock::now().time_since_epoch());
				std::chrono::milliseconds diff = now - last_report_fps_timestamp;

				PSMPosef contPose;
				if (PSM_GetControllerPose(controller_views[i]->ControllerID, &contPose) == PSMResult_Success)
				{
					g_FreePIEData.x = g_fMultiplier * contPose.Position.x / 100.0f;
					g_FreePIEData.y = g_fMultiplier * contPose.Position.y / 100.0f;
					g_FreePIEData.z = g_fMultiplier * contPose.Position.z / 100.0f;
					if (g_bDebug) 
						printf("Cont w[%d]: (%0.4f, %0.4f, %0.4f)\n", g_iFreePIESlotOut, g_FreePIEData.x, g_FreePIEData.y, g_FreePIEData.z);
					// Write the pose to slot g_iFreePIESlotOut:
					WriteFreePIE(g_iFreePIESlotOut);
				}
			}
		}
	}
}

void FreepieMoveClient::shutdown()
{
	std::cout << "FreepieMoveClient is shutting down!" << std::endl;
	
	free_controller_views();
	free_hmd_views();

	// Close all active network connections
	PSM_Shutdown();
}

FreepieMoveClient::~FreepieMoveClient()
{
	shutdown();
}

void FreepieMoveClient::init_controller_views() {
	// Once created, updates will automatically get pushed into this view
	for (int i = 0; i < trackedControllerCount; i++)
	{
		PSM_AllocateControllerListener(trackedControllerIDs[i]);
		controller_views[i] = PSM_GetController(trackedControllerIDs[i]);

		// Kick off request to start streaming data from the first controller
		PSM_StartControllerDataStreamAsync(
			controller_views[i]->ControllerID, 
			m_sendSensorData ? PSMStreamFlags_includePositionData | PSMStreamFlags_includeCalibratedSensorData : PSMStreamFlags_includePositionData,
			&start_stream_request_ids[i]);

		//Set bulb color if specified
		if ((trackedBulbColors[i] >= 0) && (trackedBulbColors[i] < PSMTrackingColorType_MaxColorTypes)) {
			PSMRequestID request_id;
			PSM_SetControllerLEDColorAsync(controller_views[i]->ControllerID, trackedBulbColors[i], &request_id);
			PSM_EatResponse(request_id);
		}
	}
}

void FreepieMoveClient::free_controller_views() {
	// Free any allocated controller views
	for (int i = 0; i < trackedControllerCount; i++)
	{
		if (controller_views[i] != nullptr)
		{
			// Stop the controller stream
			PSMRequestID request_id;
			PSM_StopControllerDataStreamAsync(controller_views[i]->ControllerID, &request_id);
			PSM_EatResponse(request_id);

			// Free out controller listener
			PSM_FreeControllerListener(controller_views[i]->ControllerID);
			controller_views[i] = nullptr;
		}
	}
}

void FreepieMoveClient::init_hmd_views() {
	// Once created, updates will automatically get pushed into this view
	for (int i = 0; i < trackedHmdCount; i++)
	{
		PSM_AllocateHmdListener(trackedHmdIDs[i]);
		hmd_views[i] = PSM_GetHmd(trackedHmdIDs[i]);

		// Kick off request to start streaming data from the first hmd
		const bool bHasSensor= hmd_views[i]->HmdType == PSMHmd_Morpheus;
		PSM_StartHmdDataStreamAsync(
			hmd_views[i]->HmdID, 
			(bHasSensor && m_sendSensorData) 
			? PSMStreamFlags_includePositionData | PSMStreamFlags_includeCalibratedSensorData 
			: PSMStreamFlags_includePositionData,
			&start_stream_request_ids[i]);
	}
}

void FreepieMoveClient::free_hmd_views() {
	// Free any allocated hmd views
	for (int i = 0; i < trackedHmdCount; i++)
	{
		if (hmd_views[i] != nullptr)
		{
			// Stop the hmd stream
			PSMRequestID request_id;
			PSM_StopHmdDataStreamAsync(hmd_views[i]->HmdID, &request_id);
			PSM_EatResponse(request_id);

			// Free out controller listener
			PSM_FreeHmdListener(hmd_views[i]->HmdID);
			hmd_views[i] = nullptr;
		}
	}
}
