// Created by Grigore Stefan <g_stefan@yahoo.com>
// Public domain (Unlicense) <http://unlicense.org>
// SPDX-FileCopyrightText: 2023-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: Unlicense

// Disk checks that do not need administrator rights: Application::checkVHDX
// (missing, short, readable, non ASCII name), mountVHDX of a missing or
// invalid file, and getMountedVHDX.

#include <stdio.h>
#include <string.h>

#include <stdexcept>
#include <vector>

#include <XYO/MountStorage/Application.hpp>

using String = XYO::Encoding::String;
using StringUTF16 = XYO::Encoding::StringUTF16;
using Application = XYO::MountStorage::Application;
using VHDXProblem = XYO::MountStorage::VHDXProblem;
template <typename T>
using TDynamicArray = XYO::DataStructures::TDynamicArray<T>;
namespace UTF = XYO::Encoding::UTF;

static void check(bool value, const char *expression, int line) {
	if (!value) {
		char message[1024];
		snprintf(message, sizeof(message), "line %d: %s", line, expression);
		throw std::runtime_error(message);
	};
};

#define CHECK(x) check((x), #x, __LINE__)

// Create a file of `size` bytes, the name is UTF-8.
static bool createFile(const String &fileName, size_t size) {
	StringUTF16 fileNameUTF16 = UTF::utf16FromUTF8(fileName);
	HANDLE hFile = CreateFileW(reinterpret_cast<LPCWSTR>(fileNameUTF16.value()), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE) {
		return false;
	};
	std::vector<uint8_t> buffer(size, 0x5A);
	DWORD written = 0;
	bool retV = true;
	if (size > 0) {
		retV = WriteFile(hFile, buffer.data(), static_cast<DWORD>(size), &written, nullptr) && (written == size);
	};
	CloseHandle(hFile);
	return retV;
};

static void removeFile(const String &fileName) {
	StringUTF16 fileNameUTF16 = UTF::utf16FromUTF8(fileName);
	DeleteFileW(reinterpret_cast<LPCWSTR>(fileNameUTF16.value()));
};

static void testCheckVHDX() {
	DWORD errorCode;

	// Missing file
	removeFile("test.missing.vhdx");
	CHECK(Application::checkVHDX("test.missing.vhdx", errorCode) == VHDXProblem::ErrorOpen);
	CHECK(errorCode == ERROR_FILE_NOT_FOUND);

	// Shorter than 64 KB
	CHECK(createFile("test.short.vhdx", 1000));
	CHECK(Application::checkVHDX("test.short.vhdx", errorCode) == VHDXProblem::ErrorRead);
	CHECK(errorCode != ERROR_SUCCESS);
	removeFile("test.short.vhdx");

	// Empty
	CHECK(createFile("test.empty.vhdx", 0));
	CHECK(Application::checkVHDX("test.empty.vhdx", errorCode) == VHDXProblem::ErrorRead);
	removeFile("test.empty.vhdx");

	// Exactly 64 KB and bigger
	CHECK(createFile("test.64k.vhdx", 64 * 1024));
	CHECK(Application::checkVHDX("test.64k.vhdx", errorCode) == VHDXProblem::Ok);
	CHECK(errorCode == ERROR_SUCCESS);
	removeFile("test.64k.vhdx");

	CHECK(createFile("test.big.vhdx", 200 * 1024));
	CHECK(Application::checkVHDX("test.big.vhdx", errorCode) == VHDXProblem::Ok);
	removeFile("test.big.vhdx");

	// Non ASCII name, the path in the config file is UTF-8
	String unicodeName = "test.\xC8\x98" "ase.\xE6\x96\x87.vhdx";
	CHECK(createFile(unicodeName, 64 * 1024));
	CHECK(Application::checkVHDX(unicodeName, errorCode) == VHDXProblem::Ok);
	removeFile(unicodeName);
	CHECK(Application::checkVHDX(unicodeName, errorCode) == VHDXProblem::ErrorOpen);

	// The file is closed after the check, it can be removed
	CHECK(createFile("test.closed.vhdx", 1000));
	CHECK(Application::checkVHDX("test.closed.vhdx", errorCode) == VHDXProblem::ErrorRead);
	removeFile("test.closed.vhdx");
	CHECK(Application::checkVHDX("test.closed.vhdx", errorCode) == VHDXProblem::ErrorOpen);
};

static void testMountVHDX() {
	DWORD errorCode;

	// Missing file: not opened, nothing attached
	removeFile("test.missing.vhdx");
	CHECK(Application::mountVHDX("test.missing.vhdx", errorCode) == VHDXProblem::ErrorOpen);
	CHECK(errorCode != ERROR_SUCCESS);

	// Not a virtual disk (or no rights to attach): not opened
	CHECK(createFile("test.invalid.vhdx", 64 * 1024));
	CHECK(Application::mountVHDX("test.invalid.vhdx", errorCode) == VHDXProblem::ErrorOpen);
	CHECK(errorCode != ERROR_SUCCESS);
	removeFile("test.invalid.vhdx");

	CHECK(createFile("test.invalid.vhd", 64 * 1024));
	CHECK(Application::mountVHDX("test.invalid.vhd", errorCode) == VHDXProblem::ErrorOpen);
	CHECK(errorCode != ERROR_SUCCESS);
	removeFile("test.invalid.vhd");
};

static void testGetMountedVHDX() {
	TDynamicArray<String> fileList;
	fileList.push("not a mounted disk");

	// The list is replaced, not appended to
	if (Application::getMountedVHDX(fileList)) {
		size_t index;
		for (index = 0; index < fileList.length(); ++index) {
			CHECK(fileList[index] != "not a mounted disk");
			CHECK(fileList[index].length() > 0);
			printf("Mounted: %s\r\n", fileList[index].value());
		};
	};
};

static void testCreateTaskFile() {
	namespace Shell = XYO::System::Shell;
	using UTFStreamMode = XYO::Encoding::UTFStreamMode;
	// fabricare test runs the tests in output/test
	const char *templateFile = "../../source/XYO/MountStorage/Application.Task.Template.xml";
	const char *taskFile = "test.task.xml";
	String content;
	size_t index;

	CHECK(Application::createTaskFile("C:\\Tom & Jerry\\Storage", templateFile, taskFile));
	CHECK(Shell::fileGetContentsUTF8(taskFile, content, UTFStreamMode::UTF16));
	CHECK(content.length() > 0);
	CHECK(!content.indexOf("#{APPLICATION_PATH}", 0, index));
	CHECK(content.indexOf("<Command>C:\\Tom &amp; Jerry\\Storage\\mount-storage.exe</Command>", 0, index));
	CHECK(content.indexOf("<Arguments>--task</Arguments>", 0, index));

	// Written as UTF-16 LE with BOM, as the XML declaration says
	XYO::System::Buffer buffer;
	CHECK(Shell::fileGetContents(taskFile, buffer));
	CHECK(buffer.length >= 2);
	CHECK(buffer.buffer[0] == 0xFF);
	CHECK(buffer.buffer[1] == 0xFE);

	Shell::remove(taskFile);

	// Missing template
	CHECK(!Application::createTaskFile("C:\\Storage", "test.missing.template.xml", taskFile));
	Shell::remove(taskFile);
};

static void test() {
	Application::initMemory();

	testCheckVHDX();
	testMountVHDX();
	testGetMountedVHDX();
	testCreateTaskFile();

	printf("Done.\r\n");
};

int main(int cmdN, char *cmdS[]) {

	try {
		test();
		return 0;
	} catch (const std::exception &e) {
		printf("* Error: %s\n", e.what());
	} catch (...) {
		printf("* Error: Unknown\n");
	};

	return 1;
};
