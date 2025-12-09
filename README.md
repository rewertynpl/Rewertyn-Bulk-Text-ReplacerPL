# Rewertyn-Bulk-Text-ReplacerPL
Copyright (c) 2025 Marcin Matysek (RewertynPL) under MIT License

I. Project Design Assumptions (Business & Functional Level)
1. Project Name and Purpose
Project Name: Rewertyn Bulk Text ReplacerPL

Main Goal/Mission: To provide a robust, fast, and multi-encoding aware desktop utility for performing recursive find-and-replace operations on text files within complex directory structures. This project solves the common administrative and development task of bulk editing configuration files, source code headers, or data files in a structured manner, ensuring that the original file encoding (UTF-8, UTF-16, ANSI) and line endings (CRLF/LF) are faithfully preserved upon saving.

2. Description of Functionality
Key Modules and Functions:

GUI Module (Win32): Provides a native Windows interface for user interaction, including input fields for the root directory, target filename pattern, old text, and new text.

File System Traversal Module: Uses std::filesystem::recursive_directory_iterator to traverse subdirectories starting from the root path, applying filters based on the user-defined filename or wildcard pattern (e.g., *.* or *.txt).

Encoding Detection & Conversion Module: This is the core specialized module. It reads file content as raw bytes and accurately detects the original encoding (UTF8 w/ or w/o BOM, UTF16 LE/BE, or ANSI). It includes robust functions (UTF8_to_wstring, UTF16Bytes_to_wstring, etc.) to convert file bytes to the internal UTF-16 (std::wstring) format for processing.

Text Processing & Normalization Module: Performs the actual find-and-replace operation. It temporarily normalizes line endings (CRLF to LF) before search/replace to ensure multiline searches succeed, and then restores the original line endings (LF to CRLF) before writing.

File I/O & Backup Module: Before saving changes, it creates a backup copy (.bak extension) of the original file. It then uses the detected encoding to write the modified UTF-16 content back to the file system, preserving the original BOM presence and encoding type.

Logging Module: Provides detailed, asynchronous logging (PostLogMessage) to the main window's log area, tracking processed files, replacement counts, and errors.

Usage Scenarios (Happy Paths):

Configuration Update: A user needs to change a copyright year (2024 to 2025) across thousands of configuration files (config.ini) scattered throughout a large project folder. They enter the root path, config.ini, 2024, and 2025, and click Start. The system recursively finds all matching files, performs the replacement, and logs the total count, ensuring all files are backed up and their original encoding is maintained.

Code Refactoring (Headers): A developer must globally replace a legacy function call (old_func) with a new one (new_func) only in files with the extension *.h. The system processes all files matching the wildcard, handling potential Unicode characters and multi-byte encoding correctly.

3. Non-Functional Assumptions (Quality Requirements)
Performance: The processing logic (file I/O and text replacement) runs on a separate Worker Thread (SearchAndReplaceThread). This architecture is critical for responsiveness, preventing the single-threaded Windows GUI from freezing during prolonged file system operations.

Compatibility & Robustness: The key requirement is encoding and path fidelity. The application must flawlessly handle multiple Unicode and legacy encodings, supporting wide character paths (Unicode/UTF-16) via the Win32 API (wWinMain, SetWindowTextW, etc.).

Usability & UX: The UI locks during processing (SetUIEnabled(FALSE)) to prevent user interaction that could corrupt the running thread's data, ensuring process integrity. Clear error messages and a full log provide transparency.

Security & Integrity: A mandatory backup feature (.bak file creation) ensures that the user can revert changes if an unintended replacement occurs, minimizing data loss risk.

II. Technical Specification (Architectural Level)
1. Technology Stack
Programming Language: C++17

Main Frameworks & Libraries:

Win32 API: Used for all GUI components, thread management (CreateThread), path selection (SHBrowseForFolderW), and low-level character encoding conversions (MultiByteToWideChar).

Standard Library: Heavily relies on C++17 features, particularly std::filesystem for directory iteration and file path management.

C Standard Library: Used for formatted logging (_vsnwprintf_s).

Database: None. Data persistence is handled directly via the file system.

2. System Architecture (High Level)
Architecture Schema: Monolithic Desktop Application (Win32) with Decoupled Worker Thread.

The application is a single executable built using the C++ standard library and the native Windows API.

It follows the Event-Driven Architecture principle inherent to Windows GUI programming.

The business logic is isolated in a separate thread to maintain the responsiveness of the main UI thread.

Data Flow:

Input Collection: The Main Thread (WindowProc) collects user parameters (path, texts) into a ThreadData struct upon pressing Start.

Asynchronous Execution: The Main Thread creates and detaches a Worker Thread (SearchAndReplaceThread), passing the ThreadData struct pointer.

File Processing: The Worker Thread iterates over the file system (recursive_directory_iterator). For each matching file: a. Reads raw bytes. b. Detects encoding/BOM (detect_file_encoding). c. Converts bytes to internal std::wstring (UTF-16). d. Performs std::wstring::find/replace. e. Creates backup. f. Converts modified std::wstring back to the original encoding/BOM format. g. Writes to disk.

Feedback & Finalization: The Worker Thread sends custom Windows Messages (WM_APP + 1 for logging, WM_APP + 2 for completion) back to the Main Thread. The Main Thread processes these messages to update the GUI and finally re-enables the UI controls.

3. Code Structure and Conventions
Key Components: The entire source is contained within a single file, logically divided by comments (1/4 to 4/4).

Part 1 (Headers/Conversions): Contains all the complex low-level encoding and byte manipulation functions (e.g., detect_file_encoding, wstring_to_UTF8).

Part 2 (Logic/Threading): Contains process_single_file (the core business logic) and SearchAndReplaceThread (the concurrency wrapper).

Part 3 (GUI): Contains WindowProc, CreateControls, and helper functions for UI state management.

Part 4 (Entry Point): Contains wWinMain and the message loop.

Design Patterns:

Event Message Bus: The core interaction between threads is achieved using the Windows Message Queue as an asynchronous message bus (PostMessageW).

Global State Transfer: The ThreadData struct acts as a transfer object to safely pass input parameters from the main thread's state to the new worker thread.

Strategy Pattern (Implicit): The write_wstring_to_file_with_encoding function acts as a dispatcher, selecting the correct writing strategy based on the FileEncoding enum.

4. Data Model (Entities)
Main Entities (In-Memory/Global):

FileEncoding (Enum): Defines the critical state of an input file (e.g., UTF8_WITH_BOM, UTF16_LE, ANSI).

ThreadData (Struct): The configuration object defining the operation (rootPath, targetFilename, oldText, newText).

Raw Bytes (std::vector<char>): Intermediate representation of file content used for encoding detection.

Wide String (std::wstring): The universal, in-memory processing representation of the file content (UTF-16).

Relationships: No relational database schema exists. The relationship is based on the File System hierarchy: Root Path recursively maps to Target Files.

5. Implementation Details
State Management: The application's runtime parameters are stored in global HWND variables (UI controls) and transferred to the worker thread via a heap-allocated ThreadData struct, which is explicitly deleted by the worker thread upon completion to prevent memory leaks.

External Integrations: None. The application is entirely self-contained, relying only on the host Windows Operating System for GUI, filesystem, and character conversion services.

Error Handling:

Critical Errors (UI): Non-recoverable user errors (e.g., missing path) trigger MessageBoxW.

Process Errors (Worker): Failures (e.g., file read/write exceptions, backup errors) are caught within try-catch blocks in process_single_file and relayed to the user via the robust Logging Module for later review.
