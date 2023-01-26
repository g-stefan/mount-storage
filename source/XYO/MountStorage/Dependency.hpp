// Mount Storage
// Copyright (c) 2023 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_MOUNTSTORAGE_DEPENDENCY_HPP
#define XYO_MOUNTSTORAGE_DEPENDENCY_HPP

#ifndef XYO_WINSERVICE_HPP
#	include <XYO/WinService.hpp>
#endif

#ifndef XYO_CRYPTOGRAPHY_HPP
#	include <XYO/Cryptography.hpp>
#endif

namespace XYO::MountStorage {
	using namespace XYO::WinService;
};

#endif
