// Mount Storage
// Copyright (c) 2023-2024 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023-2024 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_MOUNTSTORAGE_APPLICATION_HPP
#define XYO_MOUNTSTORAGE_APPLICATION_HPP

#ifndef XYO_MOUNTSTORAGE_DEPENDENCY_HPP
#	include <XYO/MountStorage/Dependency.hpp>
#endif

namespace XYO::MountStorage {

	class Application : public virtual Service {
			XYO_DISALLOW_COPY_ASSIGN_MOVE(Application);

		public:
			static bool isTask;
			Application();

			void showUsage();
			void showLicense();
			void showVersion();

			int main(int cmdN, char *cmdS[]);

			void serviceWork();
			String getDateTime();
			bool isElevated();
			int mountVHDX(const char *filename);
			bool getMountedVHDX(TDynamicArray<String> &fileList);
	};

};

#endif
