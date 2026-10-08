// Mount Storage
// Copyright (c) 2023-2026 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/MountStorage/Dependency.hpp>

#include <initguid.h>
#include <virtdisk.h>

#include <vector>

#include <XYO/MountStorage/Application.hpp>
#include <XYO/MountStorage/Copyright.hpp>
#include <XYO/MountStorage/License.hpp>
#include <XYO/MountStorage/Version.hpp>

namespace XYO::MountStorage {

	// Bytes read from every disk file before mount, to know the volume
	// holding it is available (unlocked)
	static const size_t checkSize_ = 64 * 1024;

	bool Application::isTask;

	Application::Application() {
		serviceName = "Mount Storage";
		isTask = false;
		waitAfterMount = false;
	};

	void Application::initMemory() {
		String::initMemory();
		StringUTF16::initMemory();
		TDynamicArray<String>::initMemory();
	};

	void Application::showUsage() {
		printf("Mount-Storage - Automatically mount VHD/VHDX on windows start-up\n");
		showVersion();
		printf("%s\n\n", MountStorage::Copyright::copyright());

		printf("%s",
		       "options:\n"
		       "    --help              this info\n"
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

	Application::Command Application::parseCommand(int cmdN, char *cmdS[]) {
		int i;
		char *opt;
		Command retV = Command::None;

		for (i = 1; i < cmdN; ++i) {
			if (strncmp(cmdS[i], "--", 2) != 0) {
				continue;
			};
			opt = &cmdS[i][2];
			// --task, --install and --uninstall are processed alone, the first one wins
			if (strcmp(opt, "task") == 0) {
				return Command::Task;
			};
			if (strcmp(opt, "install") == 0) {
				return Command::Install;
			};
			if (strcmp(opt, "uninstall") == 0) {
				return Command::Uninstall;
			};
			if ((strcmp(opt, "start") == 0) ||
			    (strcmp(opt, "stop") == 0) ||
			    (strcmp(opt, "run") == 0) ||
			    (strcmp(opt, "service") == 0)) {
				retV = Command::Service;
			};
		};

		return retV;
	};

	int Application::main(int cmdN, char *cmdS[]) {
		int i;
		char *opt;
		bool hasInfo = false;

		if (cmdN < 2) {
			showUsage();
			return 1;
		};

		for (i = 1; i < cmdN; ++i) {
			if (strncmp(cmdS[i], "--", 2) != 0) {
				continue;
			};
			opt = &cmdS[i][2];
			if ((strcmp(opt, "help") == 0) || (strcmp(opt, "usage") == 0)) {
				showUsage();
				hasInfo = true;
				continue;
			};
			if (strcmp(opt, "license") == 0) {
				showLicense();
				hasInfo = true;
				continue;
			};
			if (strcmp(opt, "version") == 0) {
				showVersion();
				hasInfo = true;
				continue;
			};
		};

		switch (parseCommand(cmdN, cmdS)) {
		case Command::Task:
			isTask = true;
			return cmdRun();
		case Command::Install:
			return install();
		case Command::Uninstall:
			return uninstall();
		case Command::Service:
			return Service::main(cmdN, cmdS);
		default:
			break;
		};

		if (hasInfo) {
			return 0;
		};

		showUsage();
		return 1;
	};

	int Application::install() {
		DWORD retV;
		String cmd;

		retV = cmdInstall();
		if (retV != NO_ERROR) {
			fprintf(stderr, "%s: --install failed, error %lu\n", serviceName, retV);
			return static_cast<int>(retV);
		};

		// Make User Profile Loader Service dependable on our service
		// Our service should be started before user profile is loaded
		cmd = "sc config ProfSvc depend= \"RpcSs/";
		cmd << serviceName;
		cmd << "\"";

		if (XYO::System::Shell::executeHidden(cmd) != 0) {
			fprintf(stderr, "%s: --install failed, unable to set User Profile Service dependency\n", serviceName);
			return 1;
		};

		// Create Task to run on boot from Windows 11 fast boot (Shutdown)
		String appPath = XYO::System::Shell::getExecutablePath();
		String fileIn = appPath;
		fileIn += "\\mount-storage.task.template.xml";
		String fileOut = appPath;
		fileOut += "\\mount-storage.task.xml";
		if (!createTaskFile(appPath, fileIn, fileOut)) {
			fprintf(stderr, "%s: --install failed, unable to create task file from \"%s\"\n", serviceName, fileIn.value());
			return 1;
		};

		// /F replaces the task if already created by a previous install
		cmd = "schtasks.exe /Create /XML \"";
		cmd += fileOut;
		cmd += "\" /TN \"";
		cmd += serviceName;
		cmd += "\" /F";

		if (XYO::System::Shell::executeHidden(cmd) != 0) {
			fprintf(stderr, "%s: --install failed, unable to create task\n", serviceName);
			return 1;
		};

		return 0;
	};

	bool Application::createTaskFile(const String &appPath, const String &fileIn, const String &fileOut) {
		TDynamicArray<TDynamicArray<String>> replace;
		replace[0][0] = "#{APPLICATION_PATH}";
		replace[0][1] = xmlEscape(appPath);
		return XYO::System::Shell::fileReplaceTextUTF8(fileIn, fileOut, replace, 16384, UTFStreamMode::UTF16);
	};

	int Application::uninstall() {
		DWORD retV;
		String cmd;

		// Remove Task, may not exist
		cmd = "schtasks.exe /Delete /TN \"";
		cmd += serviceName;
		cmd += "\" /F";
		XYO::System::Shell::executeHidden(cmd);

		// Restore User Profile Loader Service dependency, before the service is removed
		cmd = "sc config ProfSvc depend= RpcSs";
		if (XYO::System::Shell::executeHidden(cmd) != 0) {
			fprintf(stderr, "%s: --uninstall failed, unable to restore User Profile Service dependency\n", serviceName);
			return 1;
		};

		retV = cmdUninstall();
		if (retV != NO_ERROR) {
			fprintf(stderr, "%s: --uninstall failed, error %lu\n", serviceName, retV);
			return static_cast<int>(retV);
		};

		return 0;
	};

	String Application::getDateTime() {
		DateTime dateTime;
		char buffer[64];

		snprintf(buffer, sizeof(buffer), "[ %04d-%02d-%02d %02d:%02d:%02d ]",
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

	void Application::parseConfig(const String &content, TDynamicArray<String> &vhdList) {
		size_t index;
		size_t lineStart;
		size_t contentLength;
		const char *text = content.value();
		String line;

		vhdList.empty();

		contentLength = content.length();
		lineStart = 0;
		// UTF-8 BOM
		if ((contentLength >= 3) &&
		    (static_cast<uint8_t>(text[0]) == 0xEF) &&
		    (static_cast<uint8_t>(text[1]) == 0xBB) &&
		    (static_cast<uint8_t>(text[2]) == 0xBF)) {
			lineStart = 3;
		};

		for (index = lineStart; index <= contentLength; ++index) {
			if ((index < contentLength) && (text[index] != '\n')) {
				continue;
			};
			line = content.substring(lineStart, index - lineStart).trimASCII();
			lineStart = index + 1;
			if (line.length() == 0) {
				continue;
			};
			if (line[0] == '#') {
				continue;
			};
			if ((line.length() >= 2) && (line[0] == '"') && (line[line.length() - 1] == '"')) {
				line = line.substring(1, line.length() - 2).trimASCII();
				if (line.length() == 0) {
					continue;
				};
			};
			vhdList.push(line);
		};
	};

	String Application::getFilenameNearExecutable(const String &executable, const char *extension) {
		String retV = XYO::System::Shell::getFileBasename(executable);
		retV += ".";
		retV += extension;
		return retV;
	};

	bool Application::isVHD(const String &filename) {
		return StringCore::compareIgnoreCaseASCII(XYO::System::Shell::getFileExtension(filename), "vhd") == 0;
	};

	static String pathForCompare_(const String &path) {
		String retV = path.trimASCII().replace("/", "\\");
		if (retV.beginWith("\\\\?\\")) {
			retV = retV.substring(4);
		};
		return retV;
	};

	bool Application::isSamePath(const String &x, const String &y) {
		return StringCore::compareIgnoreCaseASCII(pathForCompare_(x), pathForCompare_(y)) == 0;
	};

	String Application::xmlEscape(const String &text) {
		return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("\"", "&quot;").replace("'", "&apos;");
	};

	int Application::checkVHDX(const String &filename, DWORD &errorCode) {
		HANDLE hFile;
		DWORD readSize;
		size_t totalSize;
		std::vector<uint8_t> buffer(checkSize_);
		StringUTF16 filenameUTF16 = UTF::utf16FromUTF8(filename);

		errorCode = ERROR_SUCCESS;

		hFile = CreateFileW(reinterpret_cast<LPCWSTR>(filenameUTF16.value()), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (hFile == INVALID_HANDLE_VALUE) {
			errorCode = GetLastError();
			return VHDXProblem::ErrorOpen;
		};

		totalSize = 0;
		while (totalSize < checkSize_) {
			if (!ReadFile(hFile, &buffer[totalSize], static_cast<DWORD>(checkSize_ - totalSize), &readSize, nullptr)) {
				errorCode = GetLastError();
				break;
			};
			if (readSize == 0) {
				errorCode = ERROR_HANDLE_EOF;
				break;
			};
			totalSize += readSize;
		};

		CloseHandle(hFile);

		if (totalSize != checkSize_) {
			return VHDXProblem::ErrorRead;
		};

		return VHDXProblem::Ok;
	};

	int Application::mountVHDX(const String &filename, DWORD &errorCode) {
		HANDLE vhdxHandle;
		StringUTF16 filenameUTF16 = UTF::utf16FromUTF8(filename);

		VIRTUAL_STORAGE_TYPE virtualStorageTypeVHDX = {
		    VIRTUAL_STORAGE_TYPE_DEVICE_VHDX,
		    VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT};
		VIRTUAL_STORAGE_TYPE virtualStorageTypeVHD = {
		    VIRTUAL_STORAGE_TYPE_DEVICE_VHD,
		    VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT};

		errorCode = OpenVirtualDisk(isVHD(filename) ? &virtualStorageTypeVHD : &virtualStorageTypeVHDX,
		                            reinterpret_cast<PCWSTR>(filenameUTF16.value()), VIRTUAL_DISK_ACCESS_ATTACH_RW, OPEN_VIRTUAL_DISK_FLAG_NONE, nullptr, &vhdxHandle);
		if (errorCode != ERROR_SUCCESS) {
			return VHDXProblem::ErrorOpen;
		};

		errorCode = AttachVirtualDisk(vhdxHandle, nullptr, ATTACH_VIRTUAL_DISK_FLAG_PERMANENT_LIFETIME, 0, nullptr, nullptr);
		CloseHandle(vhdxHandle);
		if (errorCode != ERROR_SUCCESS) {
			return VHDXProblem::ErrorMount;
		};

		return VHDXProblem::Ok;
	};

	bool Application::getMountedVHDX(TDynamicArray<String> &fileList) {
		DWORD status;
		ULONG bufferSize;
		ULONG requiredSize;
		size_t bufferIndex;
		size_t length;
		std::vector<WCHAR> buffer;
		int retry;

		fileList.empty();

		// prealocate for at least 16 entries, retry if the list grows between calls
		bufferSize = (MAX_PATH + 1) * 16 * sizeof(WCHAR);
		status = ERROR_INSUFFICIENT_BUFFER;
		for (retry = 0; retry < 4; ++retry) {
			// one more, always zero, so the list is terminated
			buffer.assign(bufferSize / sizeof(WCHAR) + 1, 0);
			requiredSize = bufferSize;
			status = GetAllAttachedVirtualDiskPhysicalPaths(&requiredSize, buffer.data());
			if (status != ERROR_INSUFFICIENT_BUFFER) {
				break;
			};
			if (requiredSize <= bufferSize) {
				requiredSize = bufferSize * 2;
			};
			bufferSize = requiredSize;
		};
		if (status != ERROR_SUCCESS) {
			return false;
		};

		// list of strings, terminated by an empty string
		for (bufferIndex = 0; bufferIndex < buffer.size(); bufferIndex += length + 1) {
			if (buffer[bufferIndex] == 0) {
				break;
			};
			length = StringUTF16Core::length(reinterpret_cast<const utf16 *>(&buffer[bufferIndex]));
			fileList.push(UTF::utf8FromUTF16(reinterpret_cast<const utf16 *>(&buffer[bufferIndex])));
		};

		return true;
	};

	static void logLine_(File &logFile, const String &datetime, const char *message, const String &filename, DWORD errorCode) {
		String info;
		char buffer[64];

		info = datetime;
		info += " Error: ";
		info += message;
		info += " \"";
		info += filename;
		info += "\"";
		if (errorCode != ERROR_SUCCESS) {
			snprintf(buffer, sizeof(buffer), " (error %lu)", errorCode);
			info += buffer;
		};
		Stream::writeLn(logFile, info);
	};

	static void logProblems_(File &logFile, const String &datetime, TDynamicArray<String> &vhdList, TDynamicArray<int> &vhdProblem, TDynamicArray<DWORD> &vhdError) {
		size_t index;
		for (index = 0; index < vhdList.length(); ++index) {
			switch (vhdProblem[index]) {
			case VHDXProblem::ErrorOpen:
				logLine_(logFile, datetime, "unable to open", vhdList[index], vhdError[index]);
				break;
			case VHDXProblem::ErrorRead:
				logLine_(logFile, datetime, "unable to read", vhdList[index], vhdError[index]);
				break;
			case VHDXProblem::ErrorMount:
				logLine_(logFile, datetime, "unable to mount", vhdList[index], vhdError[index]);
				break;
			default:
				break;
			};
		};
	};

	bool Application::mountStorage() {
		String executable = XYO::System::Shell::getExecutable();
		String cfgFilename = getFilenameNearExecutable(executable, "cfg");
		String logFilename = getFilenameNearExecutable(executable, "log");

		TDynamicArray<String> vhdList;
		TDynamicArray<String> vhdMountedList;
		TDynamicArray<int> vhdProblem;
		TDynamicArray<DWORD> vhdError;

		File logFile;

		String datetime;
		String info;
		String content;

		if (!logFile.openAppend(logFilename)) {
			printf("Error: unable to open log file\n");
			return false;
		};

		if (!isElevated()) {
			info = getDateTime();
			info += " Error: process require elevation";
			Stream::writeLn(logFile, info);
			logFile.close();
			return false;
		};

		if (!XYO::System::Shell::fileGetContents(cfgFilename, content)) {
			info = getDateTime();
			info += " Error: config file not found";
			Stream::writeLn(logFile, info);
			logFile.close();
			return false;
		};

		parseConfig(content, vhdList);

		if (vhdList.length() == 0) {
			info = getDateTime();
			info += " Error: no vhd/vhdx in config file";
			Stream::writeLn(logFile, info);
			logFile.close();
			return false;
		};

		//

		size_t index;
		int tryCount;
		int tryCountLn;
		bool isOk;

		for (index = 0; index < vhdList.length(); ++index) {
			vhdProblem[index] = VHDXProblem::None;
			vhdError[index] = ERROR_SUCCESS;
		};

		// Check if already mounted
		if (getMountedVHDX(vhdMountedList)) {
			size_t scan;
			for (scan = 0; scan < vhdMountedList.length(); ++scan) {
				for (index = 0; index < vhdList.length(); ++index) {
					if (isSamePath(vhdList[index], vhdMountedList[scan])) {
						vhdProblem[index] = VHDXProblem::AlreadyMounted;
					};
				};
			};
		};

		// Check vhd/vhdx for reading, the volume holding it may not be available yet

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
				vhdProblem[index] = checkVHDX(vhdList[index], vhdError[index]);
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

			if (isOk) {
				break;
			};

			if (tryCount + 1 < tryCountLn) {
				reportPending();
				Thread::sleep(1000);
				reportPending();
			};
		};

		if ((!isOk) && (!isTask)) {
			logProblems_(logFile, getDateTime(), vhdList, vhdProblem, vhdError);
			logFile.close();
			return false;
		};

		// Mount vhd/vhdx

		for (index = 0; index < vhdList.length(); ++index) {
			if (vhdProblem[index] == VHDXProblem::Ok) {
				vhdProblem[index] = mountVHDX(vhdList[index], vhdError[index]);
				reportPending();
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
				return false;
			};

			info = datetime;
			info += " Mounted successfully";
			Stream::writeLn(logFile, info);
			logFile.close();
			return true;
		};

		logProblems_(logFile, datetime, vhdList, vhdProblem, vhdError);
		logFile.close();
		return false;
	};

	HRESULT Application::serviceCOMInitialize() {
		return S_OK;
	};

	void Application::serviceCOMUninitialize() {
	};

	DWORD Application::serviceInit() {
		waitAfterMount = false;
		try {
			waitAfterMount = mountStorage();
		} catch (...) {
		};
		return NO_ERROR;
	};

	void Application::serviceWork() {
		if (!waitAfterMount) {
			return;
		};
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
	};
};

#ifndef XYO_MOUNTSTORAGE_LIBRARY
XYO_APPLICATION_MAIN(XYO::MountStorage::Application);
#endif
