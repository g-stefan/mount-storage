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

	struct VHDXProblem {
			enum {
				None = 0,
				ErrorOpen,
				ErrorRead,
				ErrorMount,
				AlreadyMounted,
				Ok
			};
	};

	bool Application::isTask;

	Application::Application() {
		serviceName = "Mount Storage";
		isTask = false;
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
		       "    --task              run from task\n"
		       "    --service           internal, run as service\n");
		printf("\n");
	};

	void Application::showLicense() {
		printf("%s", MountStorage::License::license().c_str());
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
				if (strcmp(opt, "task") == 0) {
					isTask = true;
					cmdRun();
					return 0;
				};
				if (strcmp(opt, "install") == 0) {
					cmdInstall();
					String cmd;

					// Make User Profile Loader Service dependable on our service
					// Our service should be started before user profile is loaded
					cmd = "sc config ProfSvc depend= \"RpcSs/";
					cmd << serviceName;
					cmd << "\"";

					XYO::System::Shell::executeHidden(cmd);

					// Create Task to run on boot from Windows 11 fast boot (Shutdown)
					TDynamicArray<TDynamicArray<String>> replace;
					String appPath = XYO::System::Shell::getExecutablePath();
					replace[0][0] = "${APPLICATION_PATH}";
					replace[0][1] = appPath;
					String fileIn = appPath;
					fileIn += "\\mount-storage.task.template.xml";
					String fileOut = appPath;
					fileOut += "\\mount-storage.task.xml";
					if (XYO::System::Shell::fileReplaceTextUTF8(fileIn, fileOut, replace, 16238, UTFStreamMode::UTF16)) {
						String cmd;
						cmd = "schtasks.exe /Create /XML \"";
						cmd += fileOut;
						cmd += "\" /TN \"Mount Storage\"";

						XYO::System::Shell::executeHidden(cmd);
						return 0;
					};
					return 1;
				};
				if (strcmp(opt, "uninstall") == 0) {
					String cmd;

					// Remov Task
					cmd = "schtasks.exe /Delete /TN \"Mount Storage\" /F";
					XYO::System::Shell::executeHidden(cmd);

					// Restore User Profile Loader Service dependency
					cmd = "sc config ProfSvc depend= RpcSs";
					XYO::System::Shell::executeHidden(cmd);

					cmdUninstall();
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
			return VHDXProblem::ErrorOpen;
		};
		retV = AttachVirtualDisk(vhdxHandle, nullptr, ATTACH_VIRTUAL_DISK_FLAG_PERMANENT_LIFETIME, 0, nullptr, nullptr);
		if (retV != ERROR_SUCCESS) {
			CloseHandle(vhdxHandle);
			return VHDXProblem::ErrorMount;
		};

		CloseHandle(vhdxHandle);
		return VHDXProblem::Ok;
	};

	bool Application::getMountedVHDX(TDynamicArray<String> &fileList) {
		DWORD status;
		ULONG bufferSize;
		LPWSTR buffer;
		ULONG bufferIndex;

		// prealocate for at least 16 entries
		bufferSize = (MAX_PATH + 1) * 16 * sizeof(uint16_t);
		buffer = reinterpret_cast<LPWSTR>(new uint8_t[bufferSize]());
		status = GetAllAttachedVirtualDiskPhysicalPaths(&bufferSize, buffer);
		if (status == ERROR_INSUFFICIENT_BUFFER) {
			delete[] reinterpret_cast<uint8_t *>(buffer);
			buffer = reinterpret_cast<LPWSTR>(new uint8_t[bufferSize]());
			status = GetAllAttachedVirtualDiskPhysicalPaths(&bufferSize, buffer);
			if (status != ERROR_SUCCESS) {
				delete[] reinterpret_cast<uint8_t *>(buffer);
				return false;
			};
		};

		fileList.empty();
		for (bufferIndex = 0; bufferIndex < (bufferSize / sizeof(utf16)); ++bufferIndex) {
			if (buffer[bufferIndex] == 0) {
				break;
			};
			fileList.push(UTF::utf8FromUTF16(reinterpret_cast<utf16 *>(&buffer[bufferIndex])));
			bufferIndex += StringUTF16Core::length(reinterpret_cast<utf16 *>(&buffer[bufferIndex]));
		};

		return true;
	};

	void Application::serviceWork() {
		char executablePath[MAX_PATH];
		GetModuleFileName(NULL, executablePath, MAX_PATH);
		String cfgFilename = String(executablePath).replace(".exe", ".cfg");
		String logFilename = String(executablePath).replace(".exe", ".log");

		TDynamicArray<String> vhdList;
		TDynamicArray<String> vhdMountedList;
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
			Stream::writeLn(logFile, info);
			logFile.flush();
			logFile.close();
			return;
		};

		if (!cfgFile.openRead(cfgFilename)) {
			info = getDateTime();
			info += " Error: config file not found";
			Stream::writeLn(logFile, info);
			logFile.flush();
			logFile.close();
			return;
		};

		String line;
		while (Stream::readLn(cfgFile, line, 4096)) {
			line = line.trimAscii();
			if (line.length() == 0) {
				continue;
			};
			if (line[0] == '#') {
				continue;
			};
			vhdList.push(line);
		};

		cfgFile.close();

		//

		int index;
		char buffer[64 * 1024];
		File vhdFile;
		int tryCount;
		int tryCountLn;
		bool isOk;

		for (index = 0; index < vhdList.length(); ++index) {
			vhdProblem[index] = VHDXProblem::None;
		};

		// Check if already mounted
		if (getMountedVHDX(vhdMountedList)) {
			int scan;
			for (scan = 0; scan < vhdMountedList.length(); ++scan) {
				for (index = 0; index < vhdList.length(); ++index) {
					if (vhdList[index] == vhdMountedList[scan]) {
						vhdProblem[index] = VHDXProblem::AlreadyMounted;
						break;
					};
				};
			};
		};

		// Check vhd/vhdx for reading

		tryCountLn = 3;
		if (isTask) {
			tryCountLn = 1;
		};

		isOk = false;

		for (tryCount = 0; tryCount < tryCountLn; ++tryCount) {

			for (index = 0; index < vhdList.length(); ++index) {
				if (vhdProblem[index] == VHDXProblem::AlreadyMounted) {
					continue;
				};
				if (vhdProblem[index] == VHDXProblem::Ok) {
					continue;
				};

				if (!vhdFile.openRead(vhdList[index])) {
					vhdProblem[index] = VHDXProblem::ErrorOpen;
					continue;
				};

				if (!(vhdFile.read(buffer, 64 * 1024) == 64 * 1024)) {
					vhdProblem[index] = VHDXProblem::ErrorRead;
					continue;
				};

				vhdFile.close();

				vhdProblem[index] = VHDXProblem::Ok;
			};

			isOk = true;
			for (index = 0; index < vhdList.length(); ++index) {
				if (vhdProblem[index] != VHDXProblem::Ok) {
					if (vhdProblem[index] == VHDXProblem::AlreadyMounted) {
						continue;
					};
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

		if ((!isOk) && (!isTask)) {
			datetime = getDateTime();

			for (index = 0; index < vhdList.length(); ++index) {

				if (vhdProblem[index] == VHDXProblem::ErrorOpen) {
					info = datetime;
					info += " Error: unable to open \"";
					info += vhdList[index];
					info += "\"";
					Stream::writeLn(logFile, info);
				};

				if (vhdProblem[index] == VHDXProblem::ErrorRead) {
					info = datetime;
					info += " Error: unable to read \"";
					info += vhdList[index];
					info += "\"";
					Stream::writeLn(logFile, info);
				};
			};

			logFile.flush();
			logFile.close();
			return;
		};

		// Mount vhd/vhdx

		for (index = 0; index < vhdList.length(); ++index) {
			if (vhdProblem[index] == VHDXProblem::Ok) {
				vhdProblem[index] = mountVHDX(vhdList[index]);
			};
		};

		isOk = true;
		for (index = 0; index < vhdList.length(); ++index) {
			if (vhdProblem[index] != VHDXProblem::Ok) {
				if (vhdProblem[index] == VHDXProblem::AlreadyMounted) {
					continue;
				};
				isOk = false;
				break;
			};
		};

		datetime = getDateTime();

		if (isOk) {
			bool alreadyMounted = true;
			for (index = 0; index < vhdList.length(); ++index) {
				if (vhdProblem[index] != VHDXProblem::AlreadyMounted) {
					alreadyMounted = false;
					break;
				};
			};
			if (alreadyMounted) {
				logFile.close();
				return;
			};

			info = datetime;
			info += " Mounted successfully";
			Stream::writeLn(logFile, info);

			logFile.flush();
			logFile.close();

			//
			// Wait 30 seconds, possible multiple runs if is ending too fast, before User Profile Service
			//
			int count = 0;
			while (!serviceStopEvent.peek()) {
				++count;
				if (count >= 30) {
					break;
				};
				Thread::sleep(1000);
			};
			return;
		};

		for (index = 0; index < vhdList.length(); ++index) {

			if (vhdProblem[index] == VHDXProblem::ErrorOpen) {
				info = datetime;
				info += " Error: unable to open \"";
				info += vhdList[index];
				info += "\"";
				Stream::writeLn(logFile, info);
			};

			if (vhdProblem[index] == VHDXProblem::ErrorMount) {
				info = datetime;
				info += " Error: unable to mount \"";
				info += vhdList[index];
				info += "\"";
				Stream::writeLn(logFile, info);
			};
		};

		logFile.close();
	};
};

#ifndef XYO_MOUNTSTORAGE_LIBRARY
XYO_APPLICATION_MAIN(XYO::MountStorage::Application);
#endif
