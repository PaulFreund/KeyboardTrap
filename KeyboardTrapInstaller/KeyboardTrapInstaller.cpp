#include <DriverSpecs.h>
#include <windows.h>
#include <tchar.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <SetupAPI.h>
#include <wdfinstaller.h>

typedef std::basic_string<TCHAR> tstring;
#if UNICODE
	#define tcout std::wcout
#else
	#define tcout std::cout
#endif

#define NAME_DRIVER _T("KeyboardTrap")
#define NAME_COINSTALLER _T("WdfCoinstaller01011.dll")
#define NAME_SYS _T("KeyboardTrap.sys")
#define NAME_INF _T("KeyboardTrap.inf")
#define NAME_INF_WDF_INSTALL L"KeyboardTrap_Install.Wdf"
#define PATH_SYS_TARGET _T("C:\\Windows\\system32\\drivers\\KeyboardTrap.sys")

#define NAME_UPPERFILTERS _T("UpperFilters")
#define PATH_KEYBOARDCLASS _T("System\\CurrentControlSet\\Control\\Class\\{4D36E96B-E325-11CE-BFC1-08002BE10318}")

DEFINE_GUID(CLASS_KEYBOARD, 0x4D36E96B, 0xE325, 0x11CE, 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18);

const tstring PathSeparator = _T("\\");
const TCHAR PathSeparatorChar = _T('\\');
const tstring LibrarySuffix = _T(".dll");

inline bool endsWith(const tstring& heystack, const tstring& needle) {
	if(heystack.length() < needle.length()) { return false; }
	return (heystack.substr(heystack.length() - needle.length(), needle.length()) == needle);
}

std::vector<tstring> fileDirectoryContents(const tstring& path) {

	if(path.empty()) { return {}; }
	tstring findPath = path;
	if(!endsWith(findPath, PathSeparator)) { findPath += PathSeparator; }
	findPath.append("*");

	WIN32_FIND_DATA findData;
	auto handle = FindFirstFile(findPath.c_str(), &findData);
	if(handle == INVALID_HANDLE_VALUE) { return {}; }

	std::vector<tstring> result;
	tstring current;
	do {
		current = tstring(findData.cFileName);
		if(!current.empty() && current != _T(".") && current != _T("..")) { result.push_back(path + PathSeparator + current); }
	}
	while(FindNextFile(handle, &findData) != 0);

	return std::move(result);
}

void OutputMessage(const tstring& message) {
	tcout << message << std::endl;
}

void* libraryLoad(const tstring& path) {
	if(path.empty()) { return nullptr; }
	return LoadLibrary(path.c_str());
}

void libraryUnload(void* handle) {
	if(handle == nullptr) { return; }
	FreeLibrary((HMODULE)handle);
}

void* libraryGetSymbol(void* handle, const tstring& name) {
	return (void*)GetProcAddress((HMODULE)handle, name.c_str());
}

bool libraryHasSymbol(void* handle, const tstring& name) {
	return (libraryGetSymbol(handle, name) != nullptr);
}

std::vector<tstring> RegGetFilterList() {
	DWORD regBufferSize = 2048;
	TCHAR regBuffer[2048];
	RegGetValue(HKEY_LOCAL_MACHINE, PATH_KEYBOARDCLASS, NAME_UPPERFILTERS, RRF_RT_REG_MULTI_SZ, NULL, regBuffer, &regBufferSize);

	TCHAR* start = &regBuffer[0];
	std::vector<tstring> filterList;
	for(size_t idx = 0; idx < (regBufferSize - 1); idx++) {
		if(regBuffer[idx] == '\0') {
			filterList.push_back(tstring(start, &regBuffer[idx]));
			start = &regBuffer[idx + 1];
		}
	}

	return filterList;
}

void RegSetFilterList(std::vector<tstring> list) {
	std::vector<BYTE> data;
	for(auto&& filter : list) {
		data.insert(end(data), begin(filter), end(filter));
		data.push_back(_T('\0'));
	}
	data.push_back(_T('\0'));

	HKEY key;
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, PATH_KEYBOARDCLASS, NULL, KEY_WRITE, &key);
	RegSetValueEx(key, NAME_UPPERFILTERS, NULL, REG_MULTI_SZ, data.data(), (DWORD)data.size());
}

struct WdfCoInstallerFunctions {
	PFN_WDFPOSTDEVICEINSTALL WdfPostDeviceInstall;
	PFN_WDFPOSTDEVICEREMOVE WdfPostDeviceRemove;
	PFN_WDFPREDEVICEINSTALL WdfPreDeviceInstall;
	PFN_WDFPREDEVICEINSTALLEX WdfPreDeviceInstallEx;
	PFN_WDFPREDEVICEREMOVE WdfPreDeviceRemove;
};

int __cdecl _tmain(int argc, _TCHAR* argv[]) {
	bool install = true;
	if(argc == 2 && tstring(argv[1]) == tstring(_T("remove"))) { install = false; }
	
	TCHAR dirBuff[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, dirBuff);
	tstring dir(dirBuff);
	tstring infPath = dir + tstring(_T("\\")) + tstring(NAME_INF);
	tstring sysPath = dir + tstring(_T("\\")) + tstring(NAME_SYS);
	tstring coInstallerPath = dir + tstring(_T("\\")) + tstring(NAME_COINSTALLER);

	std::wstring infPathW(begin(infPath), end(infPath));

	void* hCI = LoadLibrary(coInstallerPath.c_str());

	WdfCoInstallerFunctions ci;
	ci.WdfPostDeviceInstall = (PFN_WDFPOSTDEVICEINSTALL)libraryGetSymbol(hCI, _T("WdfPostDeviceInstall"));
	ci.WdfPostDeviceRemove = (PFN_WDFPOSTDEVICEREMOVE)libraryGetSymbol(hCI, _T("WdfPostDeviceRemove"));
	ci.WdfPreDeviceInstall = (PFN_WDFPREDEVICEINSTALL)libraryGetSymbol(hCI, _T("WdfPreDeviceInstall"));
	ci.WdfPreDeviceInstallEx = (PFN_WDFPREDEVICEINSTALLEX)libraryGetSymbol(hCI, _T("WdfPreDeviceInstallEx"));
	ci.WdfPreDeviceRemove = (PFN_WDFPREDEVICEREMOVE)libraryGetSymbol(hCI, _T("WdfPreDeviceRemove"));

	if(ci.WdfPostDeviceInstall == nullptr || ci.WdfPostDeviceRemove == nullptr || ci.WdfPreDeviceInstall == nullptr || ci.WdfPreDeviceInstallEx == nullptr || ci.WdfPreDeviceRemove == nullptr) {
		OutputMessage(_T("Could not load coInstaller")); return -1;
	}


	SC_HANDLE manager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if(!manager) { OutputMessage(_T("Could not open service manager, you need administration priviliges")); goto lblUnload; }


	// Install
	if(install) {
		std::cout << _T("[Installing]") << std::endl;

		WDF_COINSTALLER_INSTALL_OPTIONS clientOptions;
		WDF_COINSTALLER_INSTALL_OPTIONS_INIT(&clientOptions);

		DWORD err = ci.WdfPreDeviceInstallEx(infPathW.c_str(), NAME_INF_WDF_INSTALL, &clientOptions);
		if(err == ERROR_SUCCESS_REBOOT_REQUIRED) { OutputMessage(_T("System needs to be rebooted")); }
		else if(err != ERROR_SUCCESS) { OutputMessage(_T("Installing WDF section failed")); goto lblCloseManager; }

		// Copy file
		CopyFile(sysPath.c_str(), PATH_SYS_TARGET, FALSE);

		// Add inf class
		auto list = RegGetFilterList();
		bool writeRegistry = true;
		for(size_t idx = 0, cnt = list.size(); idx < cnt; idx++) {
			if(list[idx] == _T("kbdclass")) { list.insert(begin(list) + idx, NAME_DRIVER); }
			else if(list[idx] == NAME_DRIVER) { writeRegistry = false; }
		}
		if(writeRegistry) { RegSetFilterList(list); }

		// Install service
		SC_HANDLE service = CreateService(manager, NAME_DRIVER, NAME_DRIVER, SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, PATH_SYS_TARGET, NULL, NULL, NULL, NULL, NULL);
		if(service == NULL) {
			err = GetLastError();
			if(err == ERROR_SERVICE_EXISTS) { OutputMessage(_T("Service already installed")); }
			else { OutputMessage(_T("Installing service failed")); goto lblCloseManager; }
		}

		CloseServiceHandle(service);
		ci.WdfPostDeviceInstall(infPathW.c_str(), NAME_INF_WDF_INSTALL);
	}
	// Remove
	else {
		DWORD err = ci.WdfPreDeviceRemove(infPathW.c_str(), NAME_INF_WDF_INSTALL);

		if(err != ERROR_SUCCESS) { OutputMessage(_T("WDF remove failed")); goto lblCloseManager; }

		// Remove service
		SC_HANDLE service = OpenService(manager, NAME_DRIVER, SERVICE_ALL_ACCESS);
		if(service != NULL) {
			if(!DeleteService(service)) { OutputMessage(_T("Deleting service failed")); }
			CloseServiceHandle(service);
		}
		else { OutputMessage(_T("Opening service for removal failed")); }

		// Remove sys file
		DeleteFile(PATH_SYS_TARGET);

		// Remove inf class
		auto list = RegGetFilterList();
		for(auto it = list.begin(); it != end(list); ++it) {
			if(*it == NAME_DRIVER) { list.erase(it); }
		}
		RegSetFilterList(list);

		ci.WdfPostDeviceRemove(infPathW.c_str(), NAME_INF_WDF_INSTALL);
	}

lblCloseManager:
	CloseServiceHandle(manager);

lblUnload:
	libraryUnload(hCI);

	OutputMessage(_T("Press key to continue..."));
	std::cin.get();
	return 0;
}