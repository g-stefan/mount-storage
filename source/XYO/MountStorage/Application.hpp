// Mount Storage
// Copyright (c) 2023-2026 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_MOUNTSTORAGE_APPLICATION_HPP
#define XYO_MOUNTSTORAGE_APPLICATION_HPP

#ifndef XYO_MOUNTSTORAGE_DEPENDENCY_HPP
#	include <XYO/MountStorage/Dependency.hpp>
#endif

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

	class Application : public virtual Service {
			XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE(Application);

		public:
			// The command selected on the command line, see parseCommand
			enum class Command {
				None,
				Task,
				Install,
				Uninstall,
				Service
			};

			static bool isTask;
			Application();

			void showUsage();
			void showLicense();
			void showVersion();

			int main(int cmdN, char *cmdS[]);

			// Service, User Profile Service dependency and startup task
			int install();
			int uninstall();
			// Task definition from the UTF-16 template, #{APPLICATION_PATH}
			// replaced with appPath (XML escaped)
			static bool createTaskFile(const String &appPath, const String &fileIn, const String &fileOut);

			// Mount the disks before the service is reported running, so that
			// the services depending on this one (User Profile Service) start
			// only after the disks are available. Always NO_ERROR: a failed
			// mount must not stop the dependent services from starting.
			DWORD serviceInit();
			void serviceWork();
			// COM is not used, and a COM failure must not fail the service start
			HRESULT serviceCOMInitialize();
			void serviceCOMUninitialize();

			// Returns true if at least one disk was mounted by this run and all
			// disks are now mounted
			bool mountStorage();

			static void initMemory();

			// First of --task, --install, --uninstall or a xyo-win-service
			// command (--start, --stop, --run, --service), Command::None if
			// there is none
			static Command parseCommand(int cmdN, char *cmdS[]);

			// Disk list from the content of the config file: one full path per
			// line, empty lines and lines starting with # are skipped,
			// surrounding whitespace and double quotes are removed, an UTF-8
			// BOM is ignored
			static void parseConfig(const String &content, TDynamicArray<String> &vhdList);

			// <executable path without extension>.<extension>
			static String getFilenameNearExecutable(const String &executable, const char *extension);

			// true for a .vhd file (case insensitive), false for .vhdx and any other
			static bool isVHD(const String &filename);

			// Compare two paths: case insensitive, / and \ are the same,
			// a \\?\ prefix is ignored
			static bool isSamePath(const String &x, const String &y);

			// Escape & < > " ' for use inside XML text
			static String xmlEscape(const String &text);

			static String getDateTime();
			static bool isElevated();

			// Check the file can be opened and its first 64 KB read,
			// returns VHDXProblem::ErrorOpen / ErrorRead / Ok
			static int checkVHDX(const String &filename, DWORD &errorCode);
			// Attach with permanent lifetime, returns VHDXProblem::ErrorOpen / ErrorMount / Ok
			static int mountVHDX(const String &filename, DWORD &errorCode);
			static bool getMountedVHDX(TDynamicArray<String> &fileList);

		protected:
			bool waitAfterMount;
	};

};

#endif
