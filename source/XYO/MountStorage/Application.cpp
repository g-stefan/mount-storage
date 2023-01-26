// Mount Storage
// Copyright (c) 2023 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/MountStorage/Dependency.hpp>

#include <initguid.h>
#include <virtdisk.h>

#include <XYO/MountStorage/Application.hpp>
#include <XYO/MountStorage/Copyright.hpp>
#include <XYO/MountStorage/License.hpp>
#include <XYO/MountStorage/Version.hpp>

namespace XYO::MountStorage {

	Application::Application() {
		serviceName = "Mount Storage";
	};

	void Application::showUsage() {
		printf("Mount-Storage - Automatically mount VHD/VHDX on windows start-up\n");
		showVersion();
		printf("%s\n\n", MountStorage::Copyright::copyright());

		printf("%s",
		       "options:\n"
		       "    --usage             this info\n"
		       "    --license           show license\n"
		       "    --version           show version\n"
		       "    --install           install service\n"
		       "    --uninstall         uninstall service\n"
		       "    --stop              stop service\n"
		       "    --start             start service\n"
		       "    --run               run as current user\n"
		       "    --service           internal, run as service\n");
		printf("\n");
	};

	void Application::showLicense() {
		printf("%s", MountStorage::License::license());
	};

	void Application::showVersion() {
		printf("version %s build %s [%s]\n", MountStorage::Version::version(), MountStorage::Version::build(), MountStorage::Version::datetime());
	};

	int Application::main(int cmdN, char *cmdS[]) {
		int i;
		char *opt;

		if (cmdN < 2) {
			showUsage();
			return 1;
		};

		for (i = 1; i < cmdN; ++i) {
			if (strncmp(cmdS[i], "--", 2) == 0) {
				opt = &cmdS[i][2];
				if (strcmp(opt, "usage") == 0) {
					showLicense();
					if (cmdN == 2) {
						return 0;
					};
				};
				if (strcmp(opt, "license") == 0) {
					showLicense();
					if (cmdN == 2) {
						return 0;
					};
				};
				if (strcmp(opt, "version") == 0) {
					showLicense();
					if (cmdN == 2) {
						return 0;
					};
				};
				if (strcmp(opt, "install") == 0) {
					install();
					String cmd;

					// Make User Profile Loader Service dependable on our service
					// Our service should be started before user profile is loaded
					cmd = "sc config ProfSvc depend= \"RpcSs/";
					cmd << serviceName;
					cmd << "\"";

					XYO::System::Shell::executeHidden(cmd);
					return 0;
				};
				if (strcmp(opt, "uninstall") == 0) {
					String cmd;

					// Restore User Profile Loader Service dependency
					cmd = "sc config ProfSvc depend= RpcSs";
					XYO::System::Shell::executeHidden(cmd);

					uninstall();
					return 0;
				};
				continue;
			};
		};

		return Service::main(cmdN, cmdS);
	};

	String Application::getDateTime() {
		DateTime dateTime;
		char buffer[64];

		sprintf(buffer, "[ %04d-%02d-%02d %02d:%02d:%02d ]",
		        dateTime.getYear(),
		        dateTime.getMonth(),
		        dateTime.getDay(),
		        dateTime.getHour(),
		        dateTime.getMinute(),
		        dateTime.getSecond());

		return buffer;
	};

	bool Application::isElevated() {
		HANDLE hProcess = nullptr;
		TOKEN_ELEVATION tokenElevation;
		DWORD dwSize;

		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hProcess)) {
			return false;
		};

		if (!GetTokenInformation(hProcess, TokenElevation, &tokenElevation, sizeof(tokenElevation), &dwSize)) {
			CloseHandle(hProcess);
			return false;
		};

		CloseHandle(hProcess);
		return tokenElevation.TokenIsElevated;
	};

	int Application::mountVHDX(const char *filename) {
		DWORD retV;
		HANDLE vhdxHandle;
		wchar_t wfilename[4096];

		VIRTUAL_STORAGE_TYPE virtualStorageTypeVHDX = {
		    VIRTUAL_STORAGE_TYPE_DEVICE_VHDX,
		    VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT};
		VIRTUAL_STORAGE_TYPE virtualStorageTypeVHD = {
		    VIRTUAL_STORAGE_TYPE_DEVICE_VHD,
		    VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT};

		bool isVHDX = true;
		char *extension = strrchr(const_cast<char *>(filename), '.');
		if (extension != NULL) {
			if (strcmp(extension, ".vhdx") == 0) {
				isVHDX = true;
			} else {
				isVHDX = false;
			};
		};

		memset(wfilename, 0, sizeof(wfilename));
		mbstowcs(wfilename, filename, strlen(filename));

		retV = OpenVirtualDisk(isVHDX ? &virtualStorageTypeVHDX : &virtualStorageTypeVHD,
		                       (PCWSTR)wfilename, VIRTUAL_DISK_ACCESS_ATTACH_RW, OPEN_VIRTUAL_DISK_FLAG_NONE, nullptr, &vhdxHandle);

		if (retV != ERROR_SUCCESS) {
			return 1;
		};
		retV = AttachVirtualDisk(vhdxHandle, nullptr, ATTACH_VIRTUAL_DISK_FLAG_PERMANENT_LIFETIME, 0, nullptr, nullptr);
		if (retV != ERROR_SUCCESS) {
			CloseHandle(vhdxHandle);
			return 2;
		};

		CloseHandle(vhdxHandle);
		return 3;
	};

	void Application::serviceWork() {
		char executablePath[MAX_PATH];
		GetModuleFileName(NULL, executablePath, MAX_PATH);
		String cfgFilename = String::replace(executablePath, ".exe", ".cfg");
		String logFilename = String::replace(executablePath, ".exe", ".log");

		TDynamicArray<String> vhdList;
		TDynamicArray<int> vhdProblem;

		File cfgFile;
		File logFile;

		String datetime;
		String info;

		if (!logFile.openAppend(logFilename)) {
			printf("Error: unable to open log file\n");
			return;
		};

		if (!isElevated()) {
			info = getDateTime();
			info += " Error: process require elevation";
			StreamX::writeLn(logFile, info);
			logFile.flush();
			logFile.close();
			return;
		};

		if (!cfgFile.openRead(cfgFilename)) {
			info = getDateTime();
			info += " Error: config file not found";
			StreamX::writeLn(logFile, info);
			logFile.flush();
			logFile.close();
			return;
		};

		String line;
		while (StreamX::readLn(cfgFile, line, 4096)) {
			line = String::trimAscii(line);
			if (line.length() == 0) {
				continue;
			};
			if (line[0] == '#') {
				continue;
			};
			vhdList.push(line);
		};

		cfgFile.close();

		// Check vhd/vhdx for reading

		int index;
		char buffer[64 * 1024];
		File vhdFile;
		int tryCount;
		bool isOk;

		for (index = 0; index < vhdList.length(); ++index) {
			vhdProblem[index] = 0;
		};

		for (tryCount = 0; tryCount < 3; ++tryCount) {

			for (index = 0; index < vhdList.length(); ++index) {
				if (vhdProblem[index] == 3) {
					continue;
				};

				if (!vhdFile.openRead(vhdList[index])) {
					vhdProblem[index] = 1;
					continue;
				};

				if (!(vhdFile.read(buffer, 64 * 1024) == 64 * 1024)) {
					vhdProblem[index] = 2;
					continue;
				};

				vhdFile.close();

				vhdProblem[index] = 3;
			};

			isOk = true;
			for (index = 0; index < vhdList.length(); ++index) {
				if (vhdProblem[index] != 3) {
					isOk = false;
					break;
				};
			};

			if (!isOk) {
				Sleep(1000);
				continue;
			}

			break;
		};

		if (!isOk) {
			datetime = getDateTime();

			for (index = 0; index < vhdList.length(); ++index) {

				if (vhdProblem[index] == 1) {
					info = datetime;
					info += " Error: unable to open \"";
					info += vhdList[index];
					info += "\"";
					StreamX::writeLn(logFile, info);
				};

				if (vhdProblem[index] == 2) {
					info = datetime;
					info += " Error: unable to read \"";
					info += vhdList[index];
					info += "\"";
					StreamX::writeLn(logFile, info);
				};
			};

			logFile.flush();
			logFile.close();
			return;
		};

		// Mount vhd/vhdx

		for (index = 0; index < vhdList.length(); ++index) {
			vhdProblem[index] = mountVHDX(vhdList[index]);
		};

		isOk = true;
		for (index = 0; index < vhdList.length(); ++index) {
			if (vhdProblem[index] != 3) {
				isOk = false;
				break;
			};
		};

		datetime = getDateTime();

		if (isOk) {

			info = datetime;
			info += " Mounted successfully";			
			StreamX::writeLn(logFile, info);

			logFile.flush();
			logFile.close();

			//
			// Wait 30 seconds, possible multiple runs if is ending too fast, before User Profile Service
			//
			int count=0;
			while (!serviceStopEvent.peek()) {
				++count;
				if(count>=30) {
					break;
				};
				Thread::sleep(1000);
			};
			return;
		};		

		for (index = 0; index < vhdList.length(); ++index) {

			if (vhdProblem[index] == 1) {
				info = datetime;
				info += " Error: unable to open \"";
				info += vhdList[index];
				info += "\"";
				StreamX::writeLn(logFile, info);
			};

			if (vhdProblem[index] == 2) {
				info = datetime;
				info += " Error: unable to mount \"";
				info += vhdList[index];
				info += "\"";
				StreamX::writeLn(logFile, info);
			};
		};

		logFile.close();
	};
};

#ifndef XYO_MOUNTSTORAGE_LIBRARY
XYO_APPLICATION_MAIN(XYO::MountStorage::Application);
#endif
