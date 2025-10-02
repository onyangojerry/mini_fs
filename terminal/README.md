# MiniFS (C++17)

A lightweight **in-memory file system shell** written in modern C++17.  
It simulates basic terminal commands (`mkdir`, `ls`, `touch`, `cat`, etc.) without touching your real filesystem — all files and directories live in memory until the program exits.

---

## Features

- **Directories and files**  
  - Create (`mkdir`, `touch`)  
  - Navigate (`cd`, `pwd`)  
  - List contents (`ls`)  

- **File operations**  
  - Edit file content (`edit`)  
  - View file content (`cat`)  
  - Delete files/directories (`rm`, `rmr`)  

- **Executable simulation**  
  - Toggle executable flag (`chmodx`)  
  - Run files (`exec`)  

- **References (links)**  
  - Link files to other files or directories (`refadd`)  
  - View references (`refls`)  

- **Interactive shell**  
  - Type `help` at any time to see all supported commands  

---

## Build & Run

### Requirements
- A C++17 compiler (tested with **g++ 9+**, **clang 10+**)
- Linux, macOS, or WSL/MinGW on Windows

### Build
```bash
g++ -std=gnu++17 -O2 -Wall -Wextra mini_fs.cpp -o mini_fs
