# Torsper

Welcome to the Torsper source code!

**Torsper** is a system which provides distributed storage, data sharing etc. based on TOR

This project focused on bypassing censorship and maintaining anonymity of all Torsper members

---

Before you build any of those apps you need to install some additional packages. Recommended installation method is vcpkg.

- Full boost static lib
- curl:x64-windows-static
- ftxui:x64-windows-static

You must place the Tor executable and its data directory in the Tor folder located next to the built .exe files.

Also you need preinstalled Visual Studio compiler and use x64 Native Tools Command Prompt for VS 2022 for building

## Build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DVCPKG_TARGET_TRIPLET=x64-windows-static

cmake --build build --config Release
```

---

## Features (in progress)

- Data sharing and dividing algorithms 
    
- RSA integration for P2P connection between hosts

___
## 📜 License

This project is licensed under the **GNU GPLv3**.  
See LICENSE.txt for details.
