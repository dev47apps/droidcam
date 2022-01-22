/* DroidCam & DroidCamX (C) 2010-2021
 * https://github.com/dev47apps
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "usbmuxd.h"

#include "common.h"
#include "settings.h"

int CheckAdbDevices(int port) {
	char buf[256];
	FILE* pipe;
	int rc = system("adb start-server");
	if (WEXITSTATUS(rc) != 0){
		rc = ERROR_LOADING_DEVICES;
		goto EXIT;
	}

	pipe = popen("adb devices", "r");
	if (!pipe) {
		rc = ERROR_LOADING_DEVICES;
		goto EXIT;
	}

	rc = ERROR_NO_DEVICES;

	while (!feof(pipe)) {
		if (fgets(buf, sizeof(buf), pipe) == NULL) break;
		dbgprint("Got line: %s", buf);
		if (strstr(buf, "List of") != NULL){
			continue;
		}
		if (strstr(buf, "offline") != NULL){
			rc = ERROR_DEVICE_OFFLINE;
			break;
		}
		if (strstr(buf, "unauthorized") != NULL){
			rc = ERROR_DEVICE_NOTAUTH;
			break;
		}
		if (strstr(buf, "device") != NULL && strstr(buf, "??") == NULL){
			rc = NO_ERROR;
			break;
		}
	}
	pclose(pipe);

EXIT:
	dbgprint("CheckAdbDevices rc=%d\n", rc);

	if (rc == NO_ERROR) {
		snprintf(buf, sizeof(buf), "adb forward tcp:%d tcp:%d", port, port);
		rc = system(buf);
		if (WEXITSTATUS(rc) != 0){
			rc = ERROR_ADDING_FORWARD;
			MSG_ERROR("Error adding adb forward!");
		}
	}
	else if (rc == ERROR_NO_DEVICES) {
		MSG_ERROR("No devices detected.\n"
			"Reconnect device and try running `adb devices` in Terminal.");
	}
	else if (rc == ERROR_DEVICE_OFFLINE) {
		if (system("adb kill-server") < 0){}
		MSG_ERROR("Device is offline. Try re-attaching device.");
	}
	else if (rc == ERROR_DEVICE_NOTAUTH) {
		if (system("adb kill-server") < 0){}
		MSG_ERROR("Device is in unauthorized state.");
	}
	else {
		MSG_ERROR("Error loading devices.\n"
			"Make sure adb is installed and try running `adb devices` in Terminal.");
	}

	return rc;
}

int ever_worked = 0;
int error_shown = 0;

int CheckiOSDevices(int port) {
	usbmuxd_device_info_t *deviceList = NULL;
	const int deviceCount = usbmuxd_get_device_list(&deviceList);
	dbgprint("CheckiOSDevices: found %d devices\n", deviceCount);

	if (deviceCount < 0) {
        if (!error_shown && !ever_worked) {
	    	MSG_ERROR("Error loading devices.\n"
    			"Make sure usbmuxd service is installed and running.");
            error_shown = 0;
        }
		return ERROR_LOADING_DEVICES;
	}
	if (deviceCount == 0) {
		MSG_ERROR("No devices detected.\n"
			"Make sure usbmuxd service running and this computer is trusted.");
		usbmuxd_device_list_free(&deviceList);
		return ERROR_NO_DEVICES;
	}

	const int sfd = usbmuxd_connect(deviceList[0].handle, port);
	if (sfd <= 0) {
		MSG_ERROR("Error getting a connection.\n"
			"Make sure DroidCam app is open,\nor try re-attaching device.");
		usbmuxd_device_list_free(&deviceList);
		return ERROR_ADDING_FORWARD;
	}

    ever_worked = 1;
	// remove the NONBLOCK flag
	int flags = fcntl(sfd, F_GETFL, NULL);
	flags &= ~O_NONBLOCK;
	fcntl(sfd, F_SETFL, flags);

	usbmuxd_device_list_free(&deviceList);
	return sfd;
}
