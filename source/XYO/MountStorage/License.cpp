// Mount Storage
// Copyright (c) 2023-2024 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023-2024 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/MountStorage/License.hpp>
#include <XYO/MountStorage/Copyright.hpp>

namespace XYO::MountStorage::License {

	std::string license() {
		std::string retV;
		retV += ManagedMemory::License::licenseMITHeader();
		retV += Copyright::copyright();
		retV += "\r\n";
		retV += ManagedMemory::License::licenseMITContent();
		return retV;
	};

	std::string shortLicense() {
		std::string retV;
		retV += Copyright::copyright();
		retV += "\r\n";
		retV += ManagedMemory::License::licenseMITShort();
		return retV;
	};

};
