kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~$ git clone https://github.com/OpenRA/OpenRA.git

kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~$ sudo apt update

kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~$ sudo apt install 


kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~$ dotnet --version
6.0.136
kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~$ sudo apt install -y libsdl2-2.0-0 libsdl2-dev

kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~$ cd OpenRA

kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~/OpenRA$ cd
kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~$ sudo apt install make

kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~$ cd OpenRA
kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~/OpenRA$ make
Compiling in Release mode...

Welcome to .NET 6.0!
---------------------
SDK Version: 6.0.136


kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~/OpenRA$ cd
kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~$ sudo apt install -y dotnet-sdk-8.0

kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~$ dotnet --version
8.0.116
kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~$ cd OpenRA
kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~/OpenRA$ make
Compiling in Release mode...

Welcome to .NET 8.0!
---------------------
SDK Version: 8.0.116


kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~/OpenRA$ 
kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~/OpenRA$ nano /home/kkongnyang2/OpenRA/OpenRA.Game/Map/Map.cs
kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~/OpenRA$ nano /home/kkongnyang2/OpenRA/OpenRA.Game/Map/Map.cs
kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~/OpenRA$ nano /home/kkongnyang2/OpenRA/OpenRA.Game/Map/Map.cs
kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~/OpenRA$ make
Compiling in Release mode...
  Determining projects to restore...
  All projects are up-to-date for restore.
  OpenRA.Game -> /home/kkongnyang2/OpenRA/bin/OpenRA.Game.dll
  OpenRA.Server -> /home/kkongnyang2/OpenRA/bin/OpenRA.Server.dll
  OpenRA.Utility -> /home/kkongnyang2/OpenRA/bin/OpenRA.Utility.dll
  OpenRA.Platforms.Default -> /home/kkongnyang2/OpenRA/bin/OpenRA.Platforms.Default.dll
  OpenRA.Launcher -> /home/kkongnyang2/OpenRA/bin/OpenRA.dll
  OpenRA.Mods.Common -> /home/kkongnyang2/OpenRA/bin/OpenRA.Mods.Common.dll
  OpenRA.Mods.D2k -> /home/kkongnyang2/OpenRA/bin/OpenRA.Mods.D2k.dll
  OpenRA.Test -> /home/kkongnyang2/OpenRA/bin/OpenRA.Test.dll
  OpenRA.Mods.Cnc -> /home/kkongnyang2/OpenRA/bin/OpenRA.Mods.Cnc.dll

Build succeeded.
    0 Warning(s)
    0 Error(s)

Time Elapsed 00:00:16.55
Downloading IP2Location GeoIP database.
kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~/OpenRA$ ./launch-game.sh
Platform is Linux (X64)
Engine version is {DEV_VERSION}
Runtime: .NET CLR 8.0.16
Using SDL 2 with OpenGL (Modern) renderer
Desktop resolution: 1920x1080
No custom resolution provided, using desktop resolution
Using resolution: 1920x1080
Using window scale 1.00
OpenGL renderer: Mesa Intel(R) UHD Graphics (CML GT2)
OpenGL version: 4.6 (Core Profile) Mesa 23.2.1-1ubuntu3.1~22.04.3
Using default sound device
Internal mods:
	d2k-content ({DEV_VERSION})
	ra-content ({DEV_VERSION})
	ra ({DEV_VERSION})
	all ({DEV_VERSION})
	ts-content ({DEV_VERSION})
	cnc-content ({DEV_VERSION})
	ts ({DEV_VERSION})
	d2k ({DEV_VERSION})
	cnc ({DEV_VERSION})
External mods:
	ra-{DEV_VERSION} ({DEV_VERSION})
Loading mod: ra
Loading mod: ra-content
kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~/OpenRA$ ./launch-game.sh
Platform is Linux (X64)
Engine version is {DEV_VERSION}
Runtime: .NET CLR 8.0.16
Using SDL 2 with OpenGL (Modern) renderer
Desktop resolution: 1920x1080
No custom resolution provided, using desktop resolution
Using resolution: 1920x1080
Using window scale 1.00
OpenGL renderer: Mesa Intel(R) UHD Graphics (CML GT2)
OpenGL version: 4.6 (Core Profile) Mesa 23.2.1-1ubuntu3.1~22.04.3
Using default sound device
Internal mods:
	d2k-content ({DEV_VERSION})
	ra-content ({DEV_VERSION})
	ra ({DEV_VERSION})
	all ({DEV_VERSION})
	ts-content ({DEV_VERSION})
	cnc-content ({DEV_VERSION})
	ts ({DEV_VERSION})
	d2k ({DEV_VERSION})
	cnc ({DEV_VERSION})
External mods:
	ra-{DEV_VERSION} ({DEV_VERSION})
Loading mod: ra
Loading mod: ra-content
kkongnyang2@kkongnyang2-930XCJ-931XCJ-930XCR:~/OpenRA$ ./launch-game.sh
Platform is Linux (X64)
Engine version is {DEV_VERSION}
Runtime: .NET CLR 8.0.16
Using SDL 2 with OpenGL (Modern) renderer
Desktop resolution: 1920x1080
No custom resolution provided, using desktop resolution
Using resolution: 1920x1080
Using window scale 1.00
OpenGL renderer: Mesa Intel(R) UHD Graphics (CML GT2)
OpenGL version: 4.6 (Core Profile) Mesa 23.2.1-1ubuntu3.1~22.04.3
Using default sound device
Internal mods:
	d2k-content ({DEV_VERSION})
	ra-content ({DEV_VERSION})
	ra ({DEV_VERSION})
	all ({DEV_VERSION})
	ts-content ({DEV_VERSION})
	cnc-content ({DEV_VERSION})
	ts ({DEV_VERSION})
	d2k ({DEV_VERSION})
	cnc ({DEV_VERSION})
External mods:
	ra-{DEV_VERSION} ({DEV_VERSION})
Loading mod: ra
Loading mod: ra-content
Loading mod: ra
[2025-06-06T20:27:49] Game started.
