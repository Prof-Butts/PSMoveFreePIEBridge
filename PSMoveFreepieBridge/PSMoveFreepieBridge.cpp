#include "stdafx.h"
#include "FreepieMoveClient.h"
#include <stdarg.h>

extern int g_iHMDFreePIESlotIn, g_iHMDFreePIESlotOut, g_iContFreePIESlotOut;
extern float g_fMultiplier;
extern bool g_bDebug;

// Parameters read from PSMoveFreepieBridge.cfg
char g_sServerIP[128] = "localhost";
char g_sServerPort[64] = "9512";

void log_debug(const char *format, ...)
{
	static char buf[300];
	static char out[300];

	va_list args;
	va_start(args, format);

	vsprintf_s(buf, 300, format, args);
	sprintf_s(out, 300, "%s\n", buf);
	printf(out);
	va_end(args);
}

void prompt_arguments(int32_t &hmdDeviceCount, int32_t &contDeviceCount,
	int* hmdDeviceIDs, int* contDeviceIDs,
	PSMTrackingColorType* bulbColors, // PSMTrackingColorType* contBulbColors,
	int32_t &contTriggerAxis)
{
	int hmdDeviceID = 0, contDeviceID = 0;
	// In this version we only care about tracking the HMD:
	//deviceType  = _deviceTypeHMD;
	//deviceCount = 1;

	std::cout << "PSMove FreePIE Bridge LITE" << std::endl;
	int deviceID = 0;

	//std::cout << "Please specify how many of the following will be tracked:" << endl;
	//std::cout << "Number of HMDs: ";
	//std::cin >> hmdDeviceCount;
	hmdDeviceCount = 1;
	contDeviceCount = 1;

	std::cout << "Enter the ID of the HMD device (-1 disables this option): ";
	std::cin >> hmdDeviceID;
	hmdDeviceIDs[0] = hmdDeviceID;
	hmdDeviceCount = hmdDeviceID > -1 ? 1 : 0;

	//std::cout << "Number of controllers (1-4). Note that more than 1 disables raw sensor data access and 4 disables button access: ";
	//std::cin >> contDeviceCount;
	std::cout << "Enter the ID of the Controller (-1 disables this option): ";
	std::cin >> contDeviceID;	
	contDeviceIDs[0] = contDeviceID;
	contDeviceCount = contDeviceID > -1 ? 1 : 0;

	PSMTrackingColorType bulbColor = PSMTrackingColorType_MaxColorTypes;
	bulbColors[0] = bulbColor;

	// For HMDs we'll read yaw/pitch/roll from the input FreePIE slot and write combined 6dof to the output slot
	if (hmdDeviceCount > 0) 
	{
		std::cout << "Enter the HMD FreePIE input slot to read yaw/pitch/roll for the HMD: ";
		std::cin >> g_iHMDFreePIESlotIn;

		std::cout << "Enter the HMD FreePIE output slot: ";
		std::cin >> g_iHMDFreePIESlotOut;
	}

	if (contDeviceCount > 0) {
		// For Controllers, we won't read yaw/pitch/roll, just output positional data to the output slot
		std::cout << "Enter the Controller FreePIE output slot: ";
		std::cin >> g_iContFreePIESlotOut;

		//std::cout << "For virtual controllers, which axis corresponds to the trigger (-1 for default): ";
		//std::cin >> contTriggerAxis;
		contTriggerAxis = -1;
	}

	//std::cout << "Enter the mutiplier: ";
	//std::cin >> g_fMultiplier;

	std::cout << "Debug Mode (1 = true, 0 = false): ";
	std::cin >> g_bDebug;
}

#ifdef DISABLED
bool parse_arguments(
    int argc, 
    char** argv, 
    eDeviceType &deviceType, 
    int32_t &deviceCount, 
    PSMControllerID* deviceIDs, 
    PSMTrackingColorType* bulbColors,
    int32_t &triggerAxis,
    bool &bExitWithPSMoveService) 
{
	bool bSuccess = true;

	int index = 1;
	while (index < argc) {
		if ((strcmp(argv[index], "-controllers") == 0) && deviceCount < 1) {
			deviceType= _deviceTypeController;
		}
		else if ((strcmp(argv[index], "-hmd") == 0) && deviceCount < 1) {
			deviceType = _deviceTypeHMD;
			deviceCount = 1;

			// set the HMD id if specified
			index++;
			if ((index < argc) && isdigit(*argv[index]))
				deviceIDs[0] = atoi(argv[index]);

			index++;
		}
		else if ((strcmp(argv[index], "-t") == 0) && deviceCount < 1) {
			index++;

			//All numeric values after the -t flag are device IDs. Loop through and add them.
			while ((index < argc) && isdigit(*argv[index])) {
				//Only add up to four controllers
				if (deviceCount < 4) {
					deviceIDs[deviceCount] = static_cast<PSMControllerID>(atoi(argv[index]));
					deviceCount++;
				}
				else {
					std::cout << "More than four controllers have been specified on the command line!" << std::endl;
					bSuccess = false;
				}

				index++;
			}
		}
		else if (strcmp(argv[index], "-c") == 0) {
			index++;
			int32_t colorIndex = 0;
			//All numeric values after the -c flag are color indicies
			while ((index < argc) && (isdigit(*argv[index]) || (strcmp(argv[index], "-1") == 0))) {
				//Only add up to four controller indicies
				if (colorIndex < 4) {
					int32_t intBulbColor= atoi(argv[index]);
					if (intBulbColor >= 0 && intBulbColor < PSMTrackingColorType_MaxColorTypes)
					{
						bulbColors[colorIndex]= static_cast<PSMTrackingColorType>(intBulbColor);
					}
					else
					{
						bulbColors[colorIndex]= PSMTrackingColorType_MaxColorTypes;
					}
					colorIndex++;
				}
				else {
					std::cout << "More than four colors have been specified on the command line!" << std::endl;
					bSuccess = false;
				}

				index++;
			}
		}
		else if (strcmp(argv[index], "-triggerAxis") == 0) {
			index++;

			if ((index < argc) && isdigit(*argv[index])) {
				triggerAxis = atoi(argv[index]);
				index++;
			}
		}
		else if (strcmp(argv[index], "-x") == 0) {
			std::cout << "-x flag specified. Will not keep window open when finished" << std::endl;
			bExitWithPSMoveService = true;
			index++;
		}
		else {
			std::cout << "Unrecognized command line argument " << argv[index] << std::endl;
			bSuccess = false;
			break;
		}
	}

	// TODO: Parse controller args
    if (deviceType == _deviceTypeHMD)
    {
        if (deviceCount > 1)
        {
            std::cout << "Only one device supported when using HMD!" << std::endl;
            std::cout << "Setting device count to 1" << std::endl;
            deviceCount= 1;
        }

        if (bulbColors[0] != PSMTrackingColorType_MaxColorTypes)
        {
            std::cout << "Custom color not supported when using HMD!" << std::endl;
            std::cout << "Setting custom color to default." << std::endl;
            bulbColors[0]= PSMTrackingColorType_MaxColorTypes;
        }
    }

	return bSuccess;
}
#endif

bool LoadParams(int32_t &hmdDeviceCount, int32_t &contDeviceCount,
	int* hmdDeviceIDs, int* contDeviceIDs,
	PSMTrackingColorType* bulbColors, // PSMTrackingColorType* contBulbColors,
	int32_t &contTriggerAxis)
{
	log_debug("Loading PSMoveFreepieBridge.cfg...");
	FILE* file;
	int error = 0, line = 0;

	try {
		error = fopen_s(&file, "./PSMoveFreepieBridge.cfg", "rt");
	}
	catch (...) {
		log_debug("Could not load PSMoveFreepieBridge.cfg");
	}

	if (error != 0) {
		log_debug("Error %d when loading PSMoveFreepieBridge.cfg", error);
		return false;
	}

	int hmdDeviceID = 0, contDeviceID = 0, deviceID = 0;
	hmdDeviceCount = 1;
	contDeviceCount = 1;

	PSMTrackingColorType bulbColor = PSMTrackingColorType_MaxColorTypes;
	bulbColors[0] = bulbColor;

	char buf[256], param[128], svalue[128];
	int param_read_count = 0;
	float fValue = 0.0f;

	while (fgets(buf, 256, file) != NULL) {
		line++;
		// Skip comments and blank lines
		if (buf[0] == ';' || buf[0] == '#')
			continue;
		if (strlen(buf) == 0)
			continue;

		if (sscanf_s(buf, "%s = %s", param, 128, svalue, 128) > 0) {
			fValue = (float)atof(svalue);

			if (_stricmp(param, "server_IP") == 0) {
				strcpy_s(g_sServerIP, 128, svalue);
			}
			else if (_stricmp(param, "server_port") == 0) {
				strcpy_s(g_sServerPort, 64, svalue);
			}
			else if (_stricmp(param, "HMD_ID") == 0) {
				hmdDeviceID = (int)fValue;
				hmdDeviceIDs[0] = hmdDeviceID;
				hmdDeviceCount = hmdDeviceID > -1 ? 1 : 0;
			}
			else if (_stricmp(param, "controller_ID") == 0) {
				contDeviceID = (int)fValue;
				contDeviceIDs[0] = contDeviceID;
				contDeviceCount = contDeviceID > -1 ? 1 : 0;
			}
			else if (_stricmp(param, "FreePIE_input_slot") == 0) {
				g_iHMDFreePIESlotIn = (int)fValue;
			}
			else if (_stricmp(param, "FreePIE_output_slot") == 0) {
				g_iHMDFreePIESlotOut = (int)fValue;
			}
			else if (_stricmp(param, "controller_output_slot") == 0) {
				g_iContFreePIESlotOut = (int)fValue;
				contTriggerAxis = -1;
			}
			else if (_stricmp(param, "debug_mode") == 0) {
				g_bDebug = (int)fValue;
			}
		}
	} // while ... read file
	fclose(file);

	// Print out the information read from the config file
	log_debug("Server IP: %s", g_sServerIP);
	log_debug("Server port: %s", g_sServerPort);
	if (hmdDeviceCount > 0) {
		log_debug("HMD ID: %d", hmdDeviceID);
		log_debug("FreePIE yaw/pitch/roll input slot %d", g_iHMDFreePIESlotIn);
		log_debug("FreePIE rotational + positional output slot %d", g_iHMDFreePIESlotOut);
	}
	
	if (contDeviceCount > 0) {
		log_debug("Controller ID: %d", contDeviceID);
		log_debug("FreePIE controller positional output slot %d", g_iContFreePIESlotOut);
	}
	
	log_debug("Debug mode: %d", g_bDebug);
	return true;
}

int main(int argc, char** argv)
{
	//eDeviceType deviceType = _deviceTypeController;
	int32_t hmdDeviceCount = 0, contDeviceCount;
	int hmdDeviceIDs[4], contDeviceIDs[4];
	int32_t freepieIndices[4] = { 0, 1, 2, 3 };
    int32_t contTriggerAxis = -1;
	PSMTrackingColorType bulbColors[4] = { 
		PSMTrackingColorType_MaxColorTypes, PSMTrackingColorType_MaxColorTypes, 
		PSMTrackingColorType_MaxColorTypes, PSMTrackingColorType_MaxColorTypes 
	};
	bool bRun = true;
	bool bExitWithPSMoveService = false;

	//if (argc == 1) {
		//prompt_arguments(deviceType, deviceCount, deviceIDs, bulbColors, triggerAxis);
		//prompt_arguments(hmdDeviceCount, contDeviceCount, hmdDeviceIDs, contDeviceIDs,
		//	bulbColors, contTriggerAxis);
	bRun = LoadParams(hmdDeviceCount, contDeviceCount, hmdDeviceIDs, contDeviceIDs,
		bulbColors, contTriggerAxis);

	if (!bRun) {
		std::cout << "Error loading config file, aborting" << std::endl;
	}
	/*}
	else {
		if (!parse_arguments(argc, argv, deviceType, deviceCount, deviceIDs, bulbColors, triggerAxis, bExitWithPSMoveService)) {
			std::cout << "Command line arguments are not valid." << std::endl;
			bRun = false;;
		}
	}*/

	if (bRun) {
		FreepieMoveClient* client = new FreepieMoveClient();
		//client->run(deviceType, deviceCount, deviceIDs, bulbColors, freepieIndices, deviceCount < 2, triggerAxis);
		client->run(hmdDeviceCount, contDeviceCount,
			hmdDeviceIDs, contDeviceIDs,
			bulbColors, // contBulbColors,
			freepieIndices, contDeviceCount < 2, 
			contTriggerAxis);
	}

	std::cout << "PSMoveFreepieBridge has ended" << std::endl;

	if (!bExitWithPSMoveService) {
		std::cin.ignore(INT_MAX);
	}

	return 0;
}