// Mount Storage
// Copyright (c) 2023 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/MountStorage/Copyright.hpp>
#include <XYO/MountStorage/Copyright.rh>

namespace XYO::MountStorage::Copyright {

	static const char *copyright_ = XYO_MOUNTSTORAGE_COPYRIGHT;
	static const char *publisher_ = XYO_MOUNTSTORAGE_PUBLISHER;
	static const char *company_ = XYO_MOUNTSTORAGE_COMPANY;
	static const char *contact_ = XYO_MOUNTSTORAGE_CONTACT;

	std::string copyright() {
		return copyright_;
	};

	std::string publisher() {
		return publisher_;
	};

	std::string company() {
		return company_;
	};

	std::string contact() {
		return contact_;
	};

};
