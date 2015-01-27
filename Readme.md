# KeyboardTrap

Windows Kernel mode class filter driver (KMDF) built with the WDF framework.

* Outputs keycode for every input
* Comes with a Driver Installer application
* KeyboardTrapInstaller.exe has to be in the same path as the packaged files
* Install as admin with "KeyboardTrapInstaller.exe" install and a reboot
* Remove as admin with "KeyboardTrapInstaller.exe remove" and a reboot
* Implementation of installer/installation not polished
* Tested with windows 7 

# Installation 

On Windows 7 x64 test signatures have to be allowd in the bootloader:

	bcdedit /set TESTSIGNING ON

Then open a ADMIN shell and go to the path where "KeyboardTrapInstaller.exe", "KeyboardTrap.sys", "KeyboardTrap.inf" and "WdfCoInstaller01011.dll" are located and

## Install

	KeyboardTrapInstaller.exe
	
## Remove

	KeyboardTrapInstaller.exe remove
