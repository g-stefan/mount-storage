// Created by Grigore Stefan <g_stefan@yahoo.com>
// Public domain (Unlicense) <http://unlicense.org>
// SPDX-FileCopyrightText: 2023-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: Unlicense

// Helpers without I/O: Application::parseCommand, parseConfig,
// getFilenameNearExecutable, isVHD, isSamePath and xmlEscape.

#include <stdio.h>
#include <string.h>

#include <stdexcept>
#include <vector>

#include <XYO/MountStorage/Application.hpp>

using String = XYO::Encoding::String;
using Application = XYO::MountStorage::Application;
template <typename T>
using TDynamicArray = XYO::DataStructures::TDynamicArray<T>;

static void check(bool value, const char *expression, int line) {
	if (!value) {
		char message[1024];
		snprintf(message, sizeof(message), "line %d: %s", line, expression);
		throw std::runtime_error(message);
	};
};

#define CHECK(x) check((x), #x, __LINE__)

// A program name is prepended, as the real command line has one.
static Application::Command command(const std::vector<const char *> &argv) {
	std::vector<char *> cmdS;
	cmdS.push_back((char *)"mount-storage");
	for (const char *argument : argv) {
		cmdS.push_back((char *)argument);
	};
	return Application::parseCommand((int)cmdS.size(), cmdS.data());
};

static void testParseCommand() {
	CHECK(command({}) == Application::Command::None);
	CHECK(command({"--help"}) == Application::Command::None);
	CHECK(command({"--version", "--license", "--usage"}) == Application::Command::None);
	CHECK(command({"--unknown", "install", "-install"}) == Application::Command::None);

	CHECK(command({"--task"}) == Application::Command::Task);
	CHECK(command({"--install"}) == Application::Command::Install);
	CHECK(command({"--uninstall"}) == Application::Command::Uninstall);
	CHECK(command({"--start"}) == Application::Command::Service);
	CHECK(command({"--stop"}) == Application::Command::Service);
	CHECK(command({"--run"}) == Application::Command::Service);
	CHECK(command({"--service"}) == Application::Command::Service);

	// --task / --install / --uninstall win over the service commands, the first one wins
	CHECK(command({"--stop", "--install"}) == Application::Command::Install);
	CHECK(command({"--start", "--uninstall", "--install"}) == Application::Command::Uninstall);
	CHECK(command({"--version", "--task", "--install"}) == Application::Command::Task);
};

static void testParseConfig() {
	TDynamicArray<String> vhdList;

	// Comments, empty lines, CRLF and surrounding whitespace
	Application::parseConfig(
	    "#\r\n"
	    "# List of vhd/vhdx to mount, full path\r\n"
	    "#\r\n"
	    "C:\\Storage\\Storage-X-1.vhdx\r\n"
	    "\r\n"
	    "   \t\r\n"
	    "  D:\\Storage\\Storage-X-2.vhdx  \r\n",
	    vhdList);
	CHECK(vhdList.length() == 2);
	CHECK(vhdList[0] == "C:\\Storage\\Storage-X-1.vhdx");
	CHECK(vhdList[1] == "D:\\Storage\\Storage-X-2.vhdx");

	// LF only, last line without end of line
	Application::parseConfig("C:\\A.vhdx\nD:\\B.vhd", vhdList);
	CHECK(vhdList.length() == 2);
	CHECK(vhdList[0] == "C:\\A.vhdx");
	CHECK(vhdList[1] == "D:\\B.vhd");

	// UTF-8 BOM (Notepad) is not part of the first path
	Application::parseConfig("\xEF\xBB\xBF"
	                         "C:\\A.vhdx\r\n",
	                         vhdList);
	CHECK(vhdList.length() == 1);
	CHECK(vhdList[0] == "C:\\A.vhdx");

	// UTF-8 BOM followed by a comment
	Application::parseConfig("\xEF\xBB\xBF# comment\r\nC:\\A.vhdx\r\n", vhdList);
	CHECK(vhdList.length() == 1);
	CHECK(vhdList[0] == "C:\\A.vhdx");

	// Paths with spaces, quoted or not
	Application::parseConfig("\"C:\\My Storage\\A.vhdx\"\r\nC:\\My Storage\\B.vhdx\r\n\"\"\r\n", vhdList);
	CHECK(vhdList.length() == 2);
	CHECK(vhdList[0] == "C:\\My Storage\\A.vhdx");
	CHECK(vhdList[1] == "C:\\My Storage\\B.vhdx");

	// A # inside a path is kept, only a leading # starts a comment
	Application::parseConfig("C:\\Disk#1\\A.vhdx\r\n", vhdList);
	CHECK(vhdList.length() == 1);
	CHECK(vhdList[0] == "C:\\Disk#1\\A.vhdx");

	// Non ASCII (UTF-8) path is kept as is
	Application::parseConfig("C:\\Stocare\\\xC8\x98" "ase.vhdx\r\n", vhdList);
	CHECK(vhdList.length() == 1);
	CHECK(vhdList[0] == "C:\\Stocare\\\xC8\x98" "ase.vhdx");

	// Nothing to mount, the previous content is cleared
	Application::parseConfig("", vhdList);
	CHECK(vhdList.length() == 0);
	Application::parseConfig("# only comments\r\n\r\n", vhdList);
	CHECK(vhdList.length() == 0);
};

static void testFilenameNearExecutable() {
	CHECK(Application::getFilenameNearExecutable("C:\\Storage\\mount-storage.exe", "cfg") == "C:\\Storage\\mount-storage.cfg");
	CHECK(Application::getFilenameNearExecutable("C:\\Storage\\mount-storage.exe", "log") == "C:\\Storage\\mount-storage.log");
	// Only the extension of the file name is replaced, not a ".exe" in the path
	CHECK(Application::getFilenameNearExecutable("C:\\Tools.exe\\mount-storage.exe", "cfg") == "C:\\Tools.exe\\mount-storage.cfg");
	CHECK(Application::getFilenameNearExecutable("C:\\Storage\\mount-storage.EXE", "cfg") == "C:\\Storage\\mount-storage.cfg");
	CHECK(Application::getFilenameNearExecutable("C:\\Storage\\my.mount-storage.exe", "cfg") == "C:\\Storage\\my.mount-storage.cfg");
};

static void testIsVHD() {
	CHECK(Application::isVHD("C:\\Storage\\A.vhd"));
	CHECK(Application::isVHD("C:\\Storage\\A.VHD"));
	CHECK(Application::isVHD("C:\\Storage\\A.Vhd"));
	CHECK(!Application::isVHD("C:\\Storage\\A.vhdx"));
	CHECK(!Application::isVHD("C:\\Storage\\A.VHDX"));
	CHECK(!Application::isVHD("C:\\Storage\\A"));
	// The extension of the file, not of a folder
	CHECK(!Application::isVHD("C:\\Disk.vhd\\A"));
	CHECK(!Application::isVHD("C:\\Disk.vhd\\A.vhdx"));
};

static void testIsSamePath() {
	CHECK(Application::isSamePath("C:\\Storage\\A.vhdx", "C:\\Storage\\A.vhdx"));
	CHECK(Application::isSamePath("c:\\storage\\a.VHDX", "C:\\Storage\\A.vhdx"));
	CHECK(Application::isSamePath("C:/Storage/A.vhdx", "C:\\Storage\\A.vhdx"));
	CHECK(Application::isSamePath("\\\\?\\C:\\Storage\\A.vhdx", "C:\\Storage\\A.vhdx"));
	CHECK(Application::isSamePath("C:\\Storage\\A.vhdx", "\\\\?\\c:\\storage\\a.vhdx"));
	CHECK(!Application::isSamePath("C:\\Storage\\A.vhdx", "D:\\Storage\\A.vhdx"));
	CHECK(!Application::isSamePath("C:\\Storage\\A.vhdx", "C:\\Storage\\A.vhd"));
	CHECK(!Application::isSamePath("C:\\Storage\\A.vhdx", "C:\\Storage\\A1.vhdx"));
};

static void testXmlEscape() {
	CHECK(Application::xmlEscape("C:\\Storage") == "C:\\Storage");
	CHECK(Application::xmlEscape("C:\\Tom & Jerry") == "C:\\Tom &amp; Jerry");
	CHECK(Application::xmlEscape("<a>\"'") == "&lt;a&gt;&quot;&apos;");
	CHECK(Application::xmlEscape("&amp;") == "&amp;amp;");
};

static void test() {
	Application::initMemory();

	testParseCommand();
	testParseConfig();
	testFilenameNearExecutable();
	testIsVHD();
	testIsSamePath();
	testXmlEscape();

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
