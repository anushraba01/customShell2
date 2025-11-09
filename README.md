🐚 Custom Shell in C++

📘 Overview

This project is a **custom command-line shell** implemented in **C++**, designed to mimic the behavior of standard UNIX/Linux shells.
It demonstrates core concepts like **process management**, **I/O redirection**, and **inter-process communication**, built from scratch with a modular and extensible architecture.

---

🎯 Objective

To build a **lightweight yet functional shell** that can:

* Execute commands through the Linux system
* Manage foreground and background processes
* Support piping (`|`) and file redirection (`<`, `>`, `>>`)
* Perform job control (list, resume, and bring jobs to foreground/background)

---

🏗️ Day-Wise Development Plan

#Day 1 – Input Parsing & Planning

* Designed the core shell architecture.
* Implemented **command parsing** and **tokenization** using string streams.
* Handled whitespace and multi-argument commands.

#Day 2 – Basic Command Execution

* Added **command execution** via `fork()` and `execvp()`.
* Implemented error handling for unknown or failed commands.

#Day 3 – Process Management

* Added support for **foreground** and **background** processes (`&`).
* Managed process IDs and maintained a job table.

#Day 4 – Piping & Redirection

* Implemented **piping between multiple commands**.
* Added **input/output redirection** using `<`, `>`, and `>>`.
* Properly handled **file descriptors** and cleanup.

#Day 5 – Job Control

* Implemented `jobs` to list background processes.
* Added `fg` and `bg` commands to control running jobs.

---

⚙️ Features

✅ Execute built-in Linux commands
✅ Foreground and background process execution
✅ Multi-level piping (`cmd1 | cmd2 | cmd3`)
✅ File redirection (`<`, `>`, `>>`)
✅ Job management (`jobs`, `fg`, `bg`)
✅ Error handling with clear feedback

---

🧠 Key Concepts Used

* **System Calls:** `fork()`, `execvp()`, `waitpid()`, `pipe()`, `dup2()`, `open()`, `close()`
* **Process Control & Signals:** Manage running jobs efficiently
* **I/O Streams & Descriptors:** For redirection and communication
* **Command Parsing & Tokenization:** Splitting user input effectively

---

🧩 Example Usage

```bash
$ ./myshell
myshell> ls -l
myshell> cat input.txt | grep "error" > output.txt
myshell> sleep 10 &
myshell> jobs
[1] 12345 sleep 10
myshell> fg 1
```

---

🛠️ How to Compile and Run

#For Linux / WSL

```bash
g++ -std=c++17 -O2 -Wall custom_shell.cpp -o myshell
./myshell
```

#For Windows (via MinGW or WSL)

1. Install **MinGW** or **WSL**.
2. Add `g++` to your system PATH.
3. Compile and run using:
bash
   g++ -std=c++17 -O2 -Wall custom_shell.cpp -o myshell
   ./myshell
   


---

📁 Directory Structure

📂 CustomShell
 ├── custom_shell.cpp       # Main source code
 ├── README.md              # Project documentation
 └── Makefile (optional)    # Build automation file


---

🚀 Future Enhancements

* Add **command history** and **auto-completion**
* Implement **built-in commands** like `cd`, `exit`, `clear`
* Handle **signal interrupts** (`Ctrl+C`, `Ctrl+Z`) gracefully
* Introduce **colorized output** for better UI feedback
* Support **shell scripting and command chaining**

---

👨‍💻 Author
Anushraba Pati
🎓 B.Tech Student | 💻 Embedded Systems & AI Enthusiast | ⚙️ Linux Developer

> Passionate about systems programming, Linux internals, and exploring intelligent automation through C++, IoT, and AI.

