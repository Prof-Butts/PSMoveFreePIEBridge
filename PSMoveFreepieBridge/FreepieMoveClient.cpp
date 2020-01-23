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

int   g_iHMDFreePIESlotIn = 0;
int   g_iHMDFreePIESlotOut = 1;
int   g_iContFreePIESlotOut = 2;
float g_fMultiplier = 1.0f;
bool  g_bDebug = false;

FreepieMoveClient::FreepieMoveClient()
{
	InitFreePIE();
}

int FreepieMoveClient::run(
    //eDeviceType deviceType, 
    int32_t hmdDeviceCount, int32_t contDeviceCount,
    int32_t hmdDeviceIDs[], int32_t contDeviceIDs[],
    PSMTrackingColorType bulbColors[], // PSMTrackingColorType contBulbColors[],
    int32_t freepieIndices[], 
    bool sendSensorData, 
    int triggerAxisIndex)
{
	// Attempt to start and run the client
	try
	{
		if (hmdDeviceCount > 0)
		{
			trackedHmdIDs = hmdDeviceIDs;
			trackedHmdCount = hmdDeviceCount;
		}
		else
			trackedHmdCount = 0;

		if (contDeviceCount > 0)
		{
			trackedControllerIDs = contDeviceIDs;
			trackedControllerCount = contDeviceCount;
		}
		else
			trackedControllerCount = 0;

		if (trackedHmdCount == 0 && trackedControllerCount == 0) {
			std::cout << "Nothing to track. Exiting program" << std::endl;
			return 0;
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
		if (trackedHmdCount > 0)
		{
			init_hmd_views();
		}

		if (trackedControllerCount > 0)
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
		if (trackedControllerCount > 0)
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
		if (trackedHmdCount > 0)
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

void FreepieMoveClient::handle_acquire_any(PSMResult resultCode, PSMHmdID index)
{
	if (resultCode == PSMResult_Success)
	{
		std::cout << "FreepieMoveClient - Acquired HMD or Controller: " << index << std::endl;
	}
	else
	{
		std::cout << "FreepieMoveClient - Failed to acquire HMD or Controller: " << index << std::endl;
		//m_keepRunning = false;
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
	int trackedDeviceCount = trackedHmdCount + trackedControllerCount;
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
                //int trackedDeviceCount = (trackedHmdCount > 0) ? trackedHmdCount : trackedControllerCount;
			    for (int i = 0; i < trackedDeviceCount; i++)
			    {
				    if (start_stream_request_ids[i] != -1 &&
					    message.response_data.request_id == start_stream_request_ids[i])
				    {
						/*
					    if (m_device_type == _deviceTypeHMD)
					    {
						    handle_acquire_hmd(message.response_data.result_code, i);
					    }
					    else
					    {
						    handle_acquire_controller(message.response_data.result_code, i);
					    }
						*/
						handle_acquire_any(message.response_data.result_code, i);
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

	//if (m_device_type == _deviceTypeHMD)
		for (int i = 0; i < trackedHmdCount; i++)
		{
			if (hmd_views[i] && hmd_views[i]->bValid)
			{
				PSMPosef hmdPose;
				if (PSM_GetHmdPose(hmd_views[i]->HmdID, &hmdPose) == PSMResult_Success)
				{
					// Read yaw, pitch, roll from the g_iFreePIESlotIn slot:
					ReadFreePIE(g_iHMDFreePIESlotIn);

					// Add the positional information:
					// Data seems to be in cms, let's scale down to meters:
					g_FreePIEData.x = g_fMultiplier * hmdPose.Position.x / 100.0f;
					g_FreePIEData.y = g_fMultiplier * hmdPose.Position.y / 100.0f;
					g_FreePIEData.z = g_fMultiplier * hmdPose.Position.z / 100.0f;

					if (g_bDebug)
						printf("HMD[%d] out slot: %d, (%0.4f, %0.4f, %0.4f)\n", i, g_iHMDFreePIESlotOut, g_FreePIEData.x, g_FreePIEData.y, g_FreePIEData.z);
					// Write everything to slot g_iFreePIESlotOut:
					WriteFreePIE(g_iHMDFreePIESlotOut);
				}
			}
		}

	//if (m_device_type == _deviceTypeController)
		for (int i = 0; i < trackedControllerCount; i++) 
		{
			if (controller_views[i] && controller_views[i]->bValid) {
				PSMPosef contPose;
				if (PSM_GetControllerPose(controller_views[i]->ControllerID, &contPose) == PSMResult_Success)
				{
					g_FreePIEData.x = g_fMultiplier * contPose.Position.x / 100.0f;
					g_FreePIEData.y = g_fMultiplier * contPose.Position.y / 100.0f;
					g_FreePIEData.z = g_fMultiplier * contPose.Position.z / 100.0f;

					// Retrieve button state
					uint32_t buttonsPressed = 0;
					const PSMVirtualController &controllerView = controller_views[i]->ControllerState.VirtualController;
					int buttonCount = (controllerView.numButtons < 16) ? controllerView.numButtons : 16;

					for (int buttonIndex = 0; buttonIndex < buttonCount; ++buttonIndex)
					{
						int bit = (controllerView.buttonStates[buttonIndex] == PSMButtonState_DOWN || 
							controllerView.buttonStates[buttonIndex] == PSMButtonState_PRESSED) ? 1 : 0;
						buttonsPressed |= (bit << buttonIndex);
					}
					*((uint32_t *)&(g_FreePIEData.yaw)) = buttonsPressed;
					// Send the axis data on the pitch/roll slots
					*((uint32_t *)&(g_FreePIEData.pitch)) = controllerView.axisStates[0];
					*((uint32_t *)&(g_FreePIEData.roll)) = controllerView.axisStates[1];

					if (g_bDebug) 
						printf("Cont[%d] out slot: %d, (%0.4f, %0.4f, %0.4f), axis0: %d, axis1: %d, 0x%X\n", 
							i, g_iContFreePIESlotOut, 
							g_FreePIEData.x, g_FreePIEData.y, g_FreePIEData.z,
							controllerView.axisStates[0], controllerView.axisStates[1], 	buttonsPressed);
					// Write the pose to slot g_iFreePIESlotOut:
					WriteFreePIE(g_iContFreePIESlotOut);
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
