#include "stdafx.h"
#include "FreepieMoveClient.h"
#include <stdarg.h>

extern int g_iFreePIESlotOut;
extern int g_iFreePIESlotIn;
extern float g_fMultiplier;
extern bool g_bDebug;

void log_debug(const char *format, ...)
{
	static char buf[300];
	static char out[300];

#ifdef DEBUG_TO_FILE
	if (g_DebugFile == NULL) {
		try {
			errno_t error = fopen_s(&g_DebugFile, "./cockpitlook.log", "wt");
		}
		catch (...) {
			OutputDebugString("[DBG] [FreePIEBridge] Could not open cockpitlook.log");
		}
	}
#endif

	va_list args;
	va_start(args, format);

	vsprintf_s(buf, 300, format, args);
	sprintf_s(out, 300, "[DBG] [FreePIEBridge] %s\n", buf);
	printf(out);
#ifdef DEBUG_TO_FILE
	if (g_DebugFile != NULL) {
		fprintf(g_DebugFile, "%s\n", buf);
		fflush(g_DebugFile);
	}
#endif

	va_end(args);
}

void prompt_arguments(eDeviceType &deviceType, int32_t &deviceCount, int* deviceIDs, PSMTrackingColorType* bulbColors, int32_t &triggerAxis) {
	int rawDeviceType = 1;
	// In this version we only care about tracking the HMD:
	deviceType  = _deviceTypeHMD;
	deviceCount = 1;

	std::cout << "PSMove FreePIE Bridge LITE" << std::endl;
	int deviceID = 0;

	std::cout << "1) HMD " << std::endl;
	std::cout << "2) Controllers " << std::endl;
	std::cout << "Which device type do you want to track: ";
	std::cin >> rawDeviceType;

	if (rawDeviceType == (int)_deviceTypeHMD)
	{
		deviceType = _deviceTypeHMD;
		deviceCount = 1;
	}
	else
	{
		deviceType = _deviceTypeController;
		deviceCount = 1;

		//std::cout << "How many controllers do you want to track (1-4)? Note that more than 1 disables raw sensor data access and 4 disables button access: ";
		//std::cin >> deviceCount;
	}

	std::cout << "Enter the ID of the device: ";
	std::cin >> deviceID;

	PSMTrackingColorType bulbColor = PSMTrackingColorType_MaxColorTypes;

	deviceIDs[0] = deviceID;
	bulbColors[0] = bulbColor;

	// For HMDs we'll read yaw/pitch/roll from the input FreePIE slot and write combined 6dof to the output slot
	if (deviceType == _deviceTypeHMD) 
	{
		std::cout << "Enter the FreePIE input slot (to read yaw/pitch/roll): ";
		std::cin >> g_iFreePIESlotIn;
	}
	// For Controllers, we won't read yaw/pitch/roll, just output positional data to the output slot
	std::cout << "Enter the FreePIE output slot: ";
	std::cin >> g_iFreePIESlotOut;

	//std::cout << "Enter the mutiplier: ";
	//std::cin >> g_fMultiplier;

	if (deviceType == _deviceTypeController)
	{
		std::cout << "For virtual controllers, which axis corresponds to the trigger (-1 for default): ";
		std::cin >> triggerAxis;
	}

	std::cout << "Debug Mode (1 = true, 0 = false): ";
	std::cin >> g_bDebug;
}

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

int main(int argc, char** argv)
{
	eDeviceType deviceType= _deviceTypeController;
	int32_t deviceCount = 0;
	int deviceIDs[4];
	int32_t freepieIndices[4] = { 0, 1, 2, 3 };
    int32_t triggerAxis= -1;
	PSMTrackingColorType bulbColors[4] = { 
		PSMTrackingColorType_MaxColorTypes, PSMTrackingColorType_MaxColorTypes, 
		PSMTrackingColorType_MaxColorTypes, PSMTrackingColorType_MaxColorTypes 
	};
	bool bRun = true;
	bool bExitWithPSMoveService = false;

	if (argc == 1) {
		prompt_arguments(deviceType, deviceCount, deviceIDs, bulbColors, triggerAxis);
	}
	else {
		if (!parse_arguments(argc, argv, deviceType, deviceCount, deviceIDs, bulbColors, triggerAxis, bExitWithPSMoveService)) {
			std::cout << "Command line arguments are not valid." << std::endl;
			bRun = false;;
		}
	}

	if (bRun) {
		FreepieMoveClient* client = new FreepieMoveClient();
		client->run(deviceType, deviceCount, deviceIDs, bulbColors, freepieIndices, deviceCount < 2, triggerAxis);
	}

	std::cout << "PSMoveFreepieBridge has ended" << std::endl;

	if (!bExitWithPSMoveService) {
		std::cin.ignore(INT_MAX);
	}

	return 0;
}