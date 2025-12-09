// main.cpp - część 1/4
// Nagłówki, detekcja kodowania i konwersje (UTF-8, UTF-16 LE/BE, ANSI)


//compiling:
//
//g++ "BULK API Program Zmieniajacy Tekst w Pliku.cpp" -o "BULK API Program Zmieniajacy Tekst w Pliku.exe" -std=c++17 -mwindows -lcomdlg32 -lshell32 -lole32 -luuid -lgdi32 -static -municode -DUNICODE -D_UNICODE


//MIT License

//Copyright (c) 2025 Marcin Matysek (RewertynPL)


#include <windows.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <functional>
#include <iostream>

// --- IDENTYFIKATORY KONTROLEK (używane później) ---
#define IDC_EDIT_PATH          101
#define IDC_BUTTON_BROWSE      102
#define IDC_EDIT_FILENAME      103
#define IDC_EDIT_OLD_TEXT      104
#define IDC_EDIT_NEW_TEXT      105
#define IDC_BUTTON_START       106
#define IDC_EDIT_LOG           107

// --- ZMIENNE GLOBALNE (deklaracje; przypisania w części GUI) ---
HWND hEditPath, hEditFilename, hEditOldText, hEditNewText, hEditLog, hButtonStart, hButtonBrowse;
HWND hMainWindow;

struct ThreadData { std::wstring rootPath, targetFilename, oldText, newText; };

// --- TYPU ENUM: rozpoznawane kodowania ---
enum class FileEncoding {
    UNKNOWN,
    UTF8_WITH_BOM,
    UTF8_NO_BOM,
    UTF16_LE,
    UTF16_BE,
    ANSI  // fallback (system code page)
};

// --- POMOCNICZE FUNKCJE KONWERSJI ---

// Konwersja UTF-8 (bajty) -> wstring (UTF-16) przy użyciu MultiByteToWideChar
std::wstring UTF8_to_wstring(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), NULL, 0);
    if (size_needed == 0) return std::wstring();
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// Konwersja wstring -> UTF-8 (bajty)
std::string wstring_to_UTF8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), NULL, 0, NULL, NULL);
    if (size_needed == 0) return std::string();
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Konwersja ANSI (system CP) -> wstring
std::wstring ANSI_to_wstring(const std::string& str, UINT codePage = CP_ACP) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(codePage, 0, str.data(), (int)str.size(), NULL, 0);
    if (size_needed == 0) return std::wstring();
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(codePage, 0, str.data(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

// Konwersja wstring -> ANSI (system CP)
std::string wstring_to_ANSI(const std::wstring& wstr, UINT codePage = CP_ACP) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(codePage, 0, wstr.data(), (int)wstr.size(), NULL, 0, NULL, NULL);
    if (size_needed == 0) return std::string();
    std::string str(size_needed, 0);
    WideCharToMultiByte(codePage, 0, wstr.data(), (int)wstr.size(), &str[0], size_needed, NULL, NULL);
    return str;
}

// Konwersja UTF-16 LE/BE bajty -> wstring
// Uwaga: jeżeli plik jest UTF-16 BE, trzeba odwrócić bajty par (swap)
std::wstring UTF16Bytes_to_wstring(const std::vector<char>& bytes, bool bigEndian) {
    if (bytes.empty()) return std::wstring();
    size_t byteCount = bytes.size();
    // Jeżeli liczba bajtów nieparzysta - ignorujemy ostatni bajt
    if (byteCount % 2 != 0) --byteCount;
    std::wstring result;
    result.resize(byteCount / 2);
    for (size_t i = 0, wi = 0; i + 1 < byteCount; i += 2, ++wi) {
        wchar_t ch;
        if (!bigEndian) {
            // LE: low then high
            ch = (unsigned char)bytes[i] | ((unsigned char)bytes[i + 1] << 8);
        } else {
            // BE: high then low
            ch = ((unsigned char)bytes[i] << 8) | (unsigned char)bytes[i + 1];
        }
        result[wi] = ch;
    }
    return result;
}

// Konwersja wstring -> UTF-16 LE bajty
std::vector<char> wstring_to_UTF16LE_bytes(const std::wstring& wstr, bool writeBOM) {
    std::vector<char> out;
    if (writeBOM) {
        out.push_back(static_cast<char>(0xFF));
        out.push_back(static_cast<char>(0xFE));
    }
    for (wchar_t wc : wstr) {
        char low = static_cast<char>(wc & 0xFF);
        char high = static_cast<char>((wc >> 8) & 0xFF);
        out.push_back(low);
        out.push_back(high);
    }
    return out;
}

// Konwersja wstring -> UTF-16 BE bajty
std::vector<char> wstring_to_UTF16BE_bytes(const std::wstring& wstr, bool writeBOM) {
    std::vector<char> out;
    if (writeBOM) {
        out.push_back(static_cast<char>(0xFE));
        out.push_back(static_cast<char>(0xFF));
    }
    for (wchar_t wc : wstr) {
        char high = static_cast<char>((wc >> 8) & 0xFF);
        char low = static_cast<char>(wc & 0xFF);
        out.push_back(high);
        out.push_back(low);
    }
    return out;
}

// --- FUNKCJE POMOCNICZE DO ODCZYTU PLIKU (BAJTY) ---
bool read_file_bytes(const std::filesystem::path& path, std::vector<char>& outBytes) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) return false;
    ifs.seekg(0, std::ios::end);
    std::streamoff sz = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    outBytes.resize((size_t)sz);
    if (sz > 0) ifs.read(outBytes.data(), sz);
    return true;
}

// --- SPRAWDZANIE CZY CIĄG BAJTÓW JEST POPRAWNYM UTF-8 (heurystyka) ---
bool is_valid_utf8(const std::vector<char>& bytes) {
    size_t i = 0;
    const size_t n = bytes.size();
    while (i < n) {
        unsigned char c = static_cast<unsigned char>(bytes[i]);
        if (c <= 0x7F) {
            i++;
            continue;
        } else if ((c >> 5) == 0x6) { // 110xxxxx  2-bytes
            if (i + 1 >= n) return false;
            unsigned char c1 = static_cast<unsigned char>(bytes[i + 1]);
            if ((c1 >> 6) != 0x2) return false;
            i += 2;
        } else if ((c >> 4) == 0xE) { // 1110xxxx 3-bytes
            if (i + 2 >= n) return false;
            unsigned char c1 = static_cast<unsigned char>(bytes[i + 1]);
            unsigned char c2 = static_cast<unsigned char>(bytes[i + 2]);
            if ((c1 >> 6) != 0x2 || (c2 >> 6) != 0x2) return false;
            i += 3;
        } else if ((c >> 3) == 0x1E) { // 11110xxx 4-bytes
            if (i + 3 >= n) return false;
            unsigned char c1 = static_cast<unsigned char>(bytes[i + 1]);
            unsigned char c2 = static_cast<unsigned char>(bytes[i + 2]);
            unsigned char c3 = static_cast<unsigned char>(bytes[i + 3]);
            if ((c1 >> 6) != 0x2 || (c2 >> 6) != 0x2 || (c3 >> 6) != 0x2) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

// --- DETEKCJA KODOWANIA PLIKU NA PODSTAWIE BAJTÓW ---
FileEncoding detect_file_encoding(const std::vector<char>& bytes) {
    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        return FileEncoding::UTF8_WITH_BOM;
    }
    if (bytes.size() >= 2) {
        unsigned char b0 = static_cast<unsigned char>(bytes[0]);
        unsigned char b1 = static_cast<unsigned char>(bytes[1]);
        if (b0 == 0xFF && b1 == 0xFE) return FileEncoding::UTF16_LE;
        if (b0 == 0xFE && b1 == 0xFF) return FileEncoding::UTF16_BE;
    }
    // heurystyka: jeśli wygląda na poprawny UTF-8 -> UTF8_NO_BOM
    if (!bytes.empty() && is_valid_utf8(bytes)) return FileEncoding::UTF8_NO_BOM;
    // fallback
    return FileEncoding::ANSI;
}

// --- KONWERSJA BAJTÓW PLIKU -> std::wstring, zachowując informację o kodowaniu ---
/*
    outEncoding - wykryte kodowanie
    outHasBOM - informacja czy plik miał BOM (dotyczy UTF-8 i UTF-16)
*/
std::wstring bytes_to_wstring_and_detect(const std::vector<char>& bytes, FileEncoding& outEncoding, bool& outHasBOM) {
    outHasBOM = false;
    outEncoding = detect_file_encoding(bytes);
    if (outEncoding == FileEncoding::UTF8_WITH_BOM) {
        outHasBOM = true;
        // pomijamy pierwsze 3 bajty BOM
        std::string payload(bytes.begin() + 3, bytes.end());
        return UTF8_to_wstring(payload);
    } else if (outEncoding == FileEncoding::UTF8_NO_BOM) {
        std::string payload(bytes.begin(), bytes.end());
        return UTF8_to_wstring(payload);
    } else if (outEncoding == FileEncoding::UTF16_LE) {
        // sprawdzamy BOM (FF FE) - jeśli jest to pomijamy, jeśli nie, to interpretujemy całość jako LE
        size_t offset = 0;
        if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xFE) {
            outHasBOM = true;
            offset = 2;
        }
        std::vector<char> payload(bytes.begin() + offset, bytes.end());
        return UTF16Bytes_to_wstring(payload, false /*bigEndian*/);
    } else if (outEncoding == FileEncoding::UTF16_BE) {
        size_t offset = 0;
        if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFE && static_cast<unsigned char>(bytes[1]) == 0xFF) {
            outHasBOM = true;
            offset = 2;
        }
        std::vector<char> payload(bytes.begin() + offset, bytes.end());
        return UTF16Bytes_to_wstring(payload, true /*bigEndian*/);
    } else { // ANSI
        std::string payload(bytes.begin(), bytes.end());
        // używamy CP_ACP (systemowego) jako domyślnego; można zmienić na 1250 jeśli potrzeba
        return ANSI_to_wstring(payload, CP_ACP);
    }
}

// --- FUNKCJA ZAPISU: zapisuje w tej samej formie (encoding) co podano ---
bool write_wstring_to_file_with_encoding(const std::filesystem::path& path,
                                         const std::wstring& content,
                                         FileEncoding encoding,
                                         bool writeBOM,
                                         UINT ansiCodePage = CP_ACP)
{
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) return false;

    if (encoding == FileEncoding::UTF8_WITH_BOM || encoding == FileEncoding::UTF8_NO_BOM) {
        // UTF-8: zdecydujemy czy zapisać BOM
        if (writeBOM) {
            ofs.put('\xEF'); ofs.put('\xBB'); ofs.put('\xBF');
        }
        std::string bytes = wstring_to_UTF8(content);
        if (!bytes.empty()) ofs.write(bytes.data(), (std::streamsize)bytes.size());
        return true;
    } else if (encoding == FileEncoding::UTF16_LE) {
        std::vector<char> bytes = wstring_to_UTF16LE_bytes(content, writeBOM);
        if (!bytes.empty()) ofs.write(bytes.data(), (std::streamsize)bytes.size());
        return true;
    } else if (encoding == FileEncoding::UTF16_BE) {
        std::vector<char> bytes = wstring_to_UTF16BE_bytes(content, writeBOM);
        if (!bytes.empty()) ofs.write(bytes.data(), (std::streamsize)bytes.size());
        return true;
    } else { // ANSI
        std::string bytes = wstring_to_ANSI(content, ansiCodePage);
        if (!bytes.empty()) ofs.write(bytes.data(), (std::streamsize)bytes.size());
        return true;
    }
}

// main.cpp - część 2/4
// Logika find/replace, wątek, backup, normalizacja końców linii

// --- POMOCNICZE LOGOWANIE DO EDITA (UI) ---
// Funkcja pomocnicza, która wysyła komunikat do okna głównego.
// Odbiorca (WindowProc) powinien obsłużyć WM_APP+1, zwalniając przekazaną std::wstring.
void PostLogMessage(const std::wstring& msg) {
    if (hMainWindow) {
        // kopiujemy na stertę i wysyłamy wskaźnik — UI zwolni pamięć.
        std::wstring* p = new std::wstring(msg);
        PostMessageW(hMainWindow, WM_APP + 1, 0, (LPARAM)p);
    }
}

// Lokalna funkcja do formatowanego logu
void LogFmt(const wchar_t* fmt, ...) {
    wchar_t buf[1024];
    va_list args;
    va_start(args, fmt);
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, args);
    va_end(args);
    PostLogMessage(buf);
}

// --- POMOCNICZE: ODPYCHANIE KOŃCÓWEK LINI (CRLF <-> LF) ---

// Normalizuj wszystkie CRLF -> LF
void normalize_CRLF_to_LF(std::wstring& s) {
    size_t pos = 0;
    while ((pos = s.find(L"\r\n", pos)) != std::wstring::npos) {
        s.replace(pos, 2, L"\n");
    }
}

// Przywróć LF -> CRLF (ale nie duplikuj istniejących CR)
void normalize_LF_to_CRLF(std::wstring& s) {
    size_t pos = 0;
    while ((pos = s.find(L"\n", pos)) != std::wstring::npos) {
        if (pos == 0 || s[pos - 1] != L'\r') {
            s.replace(pos, 1, L"\r\n");
            pos += 2;
        } else pos += 1;
    }
}

// --- LOGIKA DLA JEDNEGO PLIKU ---
// Zwraca liczbę dokonanych zamian, -1 przy błędzie
long long process_single_file(const std::filesystem::path& filepath, const ThreadData* data) {
    try {
        std::vector<char> rawBytes;
        if (!read_file_bytes(filepath, rawBytes)) {
            LogFmt(L" -> ERROR: Could not read file: %s", filepath.wstring().c_str());
            return -1;
        }

        FileEncoding detectedEncoding;
        bool hadBOM = false;
        std::wstring content = bytes_to_wstring_and_detect(rawBytes, detectedEncoding, hadBOM);
        
        // Normalizujemy plik do LF, aby wyszukiwanie wieloliniowe się powiodło
        normalize_CRLF_to_LF(content);

        if (data->oldText.empty()) {
            return 0;
        }

        size_t pos = 0;
        long long count = 0;
        while ((pos = content.find(data->oldText, pos)) != std::wstring::npos) {
            content.replace(pos, data->oldText.length(), data->newText);
            pos += data->newText.length();
            ++count;
        }

        if (count == 0) {
            return 0;
        }
        
        normalize_LF_to_CRLF(content);

        try {
            std::filesystem::path bak = filepath;
            bak += L".bak";
            std::error_code ec;
            std::filesystem::copy_file(filepath, bak, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                LogFmt(L" -> Warning: Backup file not created: %s", ec.message().c_str());
            } else {
                LogFmt(L" -> Backup created: %s", bak.wstring().c_str());
            }
        } catch (const std::exception& e) {
            LogFmt(L" -> Warning: Exception during backup creation: %S", e.what());
        }

        bool writeBOM = hadBOM;
        FileEncoding targetEncoding = detectedEncoding;
        
        bool ok = write_wstring_to_file_with_encoding(filepath, content, targetEncoding, writeBOM, CP_ACP);
        if (!ok) {
            LogFmt(L" -> ERROR: Failed to write to file: %s", filepath.wstring().c_str());
            return -1;
        }

        return count;
    } catch (const std::exception& e) {
        std::string what = e.what();
        std::wstring wwhat(what.begin(), what.end());
        LogFmt(L" -> Exception: %s", wwhat.c_str());
        return -1;
    }
}

// --- GŁÓWNA LOGIKA PRZEGLĄDANIA FOLDERU I ZASTĘPOWANIA ---
void findAndReplaceLogic(ThreadData* data) {
    try {
        long long totalReplacements = 0;
        int filesProcessed = 0;

        std::filesystem::path rootPath(data->rootPath);
        if (!std::filesystem::exists(rootPath) || !std::filesystem::is_directory(rootPath)) {
            PostLogMessage(L"ERROR: Path does not exist or is not a folder: " + data->rootPath);
            return;
        }
        
        bool wildcard = false;
        std::wstring patternExt;
        if (!data->targetFilename.empty() && data->targetFilename[0] == L'*' && data->targetFilename.size() > 1 && data->targetFilename[1] == L'.') {
            wildcard = true;
            patternExt = data->targetFilename.substr(1);
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(rootPath)) {
            if (!entry.is_regular_file()) continue;

            bool fileMatch = false;
            if (wildcard) {
                if (entry.path().extension() == patternExt) fileMatch = true;
            } else {
                if (entry.path().filename() == data->targetFilename) fileMatch = true;
            }

            if (!fileMatch) continue;

            ++filesProcessed;
            PostLogMessage(L"Processing: " + entry.path().wstring());

            long long replaced = process_single_file(entry.path(), data);
            if (replaced < 0) {
                PostLogMessage(L" -> Error during processing.");
            } else if (replaced == 0) {
                PostLogMessage(L" -> Text not found.");
            } else {
                PostLogMessage(L" -> Replaced: " + std::to_wstring(replaced) + L" occurrences.");
                totalReplacements += replaced;
            }
        }

        PostLogMessage(L"\n--- Summary ---");
        PostLogMessage(L"Files processed: " + std::to_wstring(filesProcessed));
        PostLogMessage(L"Total replacements: " + std::to_wstring(totalReplacements));
    } catch (const std::exception& e) {
        std::string what = e.what();
        std::wstring wwhat(what.begin(), what.end());
        PostLogMessage(L"Critical error: " + wwhat);
    }
}

// --- WĄTEK ROBOCZY ---
DWORD WINAPI SearchAndReplaceThread(LPVOID lpParam) {
    ThreadData* data = static_cast<ThreadData*>(lpParam);

    PostLogMessage(L"--- Starting processing ---");
    findAndReplaceLogic(data);
    PostLogMessage(L"--- Processing finished ---");

    // ZMIANA: Wyślij wiadomość do głównego wątku, aby odblokować UI
    PostMessageW(hMainWindow, WM_APP + 2, 0, 0);

    delete data;
    return 0;
}

// --- DIALOG WYBORU FOLDERU (używany w GUI) ---
void HandleBrowseFolder() {
    BROWSEINFOW bi = { 0 };
    bi.hwndOwner = hMainWindow;
    bi.lpszTitle = L"Select a folder"; // ZMIANA: Tłumaczenie
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) SetWindowTextW(hEditPath, path);
        CoTaskMemFree(pidl);
    }
}

// --- Użyteczna funkcja pomocnicza do odczytu tekstu z kontrolki EDIT ---
std::wstring GetEditText(HWND hEdit) {
    int len = GetWindowTextLengthW(hEdit) + 1;
    std::vector<wchar_t> buf(len);
    GetWindowTextW(hEdit, buf.data(), len);
    return std::wstring(buf.data());
}

// main.cpp — część 3/4
// GUI, WindowProc, logowanie, tworzenie kontrolek

// --- Tworzenie kontrolek UI ---
void CreateControls(HWND hwnd) {
    // ZMIANA: Tłumaczenia etykiet i przycisków
    CreateWindowW(L"STATIC", L"Folder:", WS_VISIBLE | WS_CHILD,
        10, 10, 80, 20, hwnd, nullptr, nullptr, nullptr);
    hEditPath = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        100, 10, 360, 22, hwnd, (HMENU)IDC_EDIT_PATH, nullptr, nullptr);
    hButtonBrowse = CreateWindowW(L"BUTTON", L"Browse", WS_VISIBLE | WS_CHILD,
        470, 10, 100, 22, hwnd, (HMENU)IDC_BUTTON_BROWSE, nullptr, nullptr);

    CreateWindowW(L"STATIC", L"Filename / *.ext:", WS_VISIBLE | WS_CHILD,
        10, 45, 150, 20, hwnd, nullptr, nullptr, nullptr);
    hEditFilename = CreateWindowW(L"EDIT", L"*.*", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        170, 45, 200, 22, hwnd, (HMENU)IDC_EDIT_FILENAME, nullptr, nullptr);

    CreateWindowW(L"STATIC", L"Text to find:", WS_VISIBLE | WS_CHILD,
        10, 80, 150, 20, hwnd, nullptr, nullptr, nullptr);
    hEditOldText = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER |
        ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_MULTILINE,
        170, 80, 400, 60, hwnd, (HMENU)IDC_EDIT_OLD_TEXT, nullptr, nullptr);

    CreateWindowW(L"STATIC", L"Replacement text:", WS_VISIBLE | WS_CHILD,
        10, 150, 150, 20, hwnd, nullptr, nullptr, nullptr);
    hEditNewText = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER |
        ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_MULTILINE,
        170, 150, 400, 60, hwnd, (HMENU)IDC_EDIT_NEW_TEXT, nullptr, nullptr);

    CreateWindowW(L"STATIC", L"Log:", WS_VISIBLE | WS_CHILD,
        10, 220, 40, 20, hwnd, nullptr, nullptr, nullptr);
    hEditLog = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER |
        ES_AUTOVSCROLL | ES_MULTILINE | ES_READONLY,
        10, 245, 560, 260, hwnd, (HMENU)IDC_EDIT_LOG, nullptr, nullptr);

    hButtonStart = CreateWindowW(L"BUTTON", L"Start", WS_VISIBLE | WS_CHILD,
        10, 515, 80, 30, hwnd, (HMENU)IDC_BUTTON_START, nullptr, nullptr);
}

// Dopisywanie logu do EDIT
void AppendToLog(const std::wstring& s) {
    int len = GetWindowTextLengthW(hEditLog);
    SendMessageW(hEditLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(hEditLog, EM_REPLACESEL, 0, (LPARAM)s.c_str());
}

// Blokada UI podczas pracy
void SetUIEnabled(BOOL enabled) {
    EnableWindow(hEditPath, enabled);
    EnableWindow(hButtonBrowse, enabled);
    EnableWindow(hEditFilename, enabled);
    EnableWindow(hEditOldText, enabled);
    EnableWindow(hEditNewText, enabled);
    EnableWindow(hButtonStart, enabled);
}

// --- WindowProc ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateControls(hwnd);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BUTTON_BROWSE) {
            HandleBrowseFolder();
        }
        else if (LOWORD(wParam) == IDC_BUTTON_START) {
            std::wstring path = GetEditText(hEditPath);
            std::wstring filename = GetEditText(hEditFilename);
            std::wstring oldText = GetEditText(hEditOldText);
            std::wstring newText = GetEditText(hEditNewText);

            normalize_CRLF_to_LF(oldText);
            normalize_CRLF_to_LF(newText);
            
            // ZMIANA: Tłumaczenie komunikatów
            if (path.empty()) {
                MessageBoxW(hwnd, L"Please provide a folder.", L"Error", MB_ICONERROR);
                return 0;
            }
            if (filename.empty()) {
                MessageBoxW(hwnd, L"Please provide a filename or pattern (*.txt).", L"Error", MB_ICONERROR);
                return 0;
            }
            if (oldText.empty()) {
                MessageBoxW(hwnd, L"Please provide the text to find.", L"Error", MB_ICONERROR);
                return 0;
            }
            
            // NOWA FUNKCJA: Sprawdzenie, czy tekst zamienny kończy się nową linią
            if (!newText.empty() && newText.back() == L'\n') {
                MessageBoxW(hwnd, L"The replacement text cannot end with a trailing new line.", L"Warning", MB_ICONWARNING);
                return 0;
            }

            SetWindowTextW(hEditLog, L"");
            SetUIEnabled(FALSE);

            ThreadData* data = new ThreadData{ path, filename, oldText, newText };
            
            HANDLE hThread = CreateThread(nullptr, 0, SearchAndReplaceThread, data, 0, nullptr);
            if (hThread) {
                 CloseHandle(hThread); // Zamknij uchwyt, jeśli go nie potrzebujesz
            }
        }
        return 0;

    case WM_APP + 1: {
        std::wstring* txt = reinterpret_cast<std::wstring*>(lParam);
        if (txt) {
            AppendToLog(*txt + L"\r\n");
            delete txt;
        }
        return 0;
    }

    // ZMIANA: Obsługa wiadomości o zakończeniu wątku
    case WM_APP + 2: 
        SetUIEnabled(TRUE); // Odblokuj UI
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// --- Tworzenie i rejestracja klasy okna ---
bool InitMainWindow(HINSTANCE hInstance, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"FindReplaceApp";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassW(&wc)) return false;

    // ZMIANA: Tłumaczenie tytułu okna
    hMainWindow = CreateWindowExW(
        0, CLASS_NAME, L"Bulk Text Replacer",
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 600,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hMainWindow) return false;

    ShowWindow(hMainWindow, nCmdShow);
    UpdateWindow(hMainWindow);
    return true;
}

// main.cpp — część 4/4
// WinMain, pętla zdarzeń, start programu

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow
) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    if (!InitMainWindow(hInstance, nCmdShow)) {
        // ZMIANA: Tłumaczenie
        MessageBoxW(nullptr, L"Failed to create window!", L"Critical Error", MB_ICONERROR);
        return 1;
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return 0;
}

/*
================================================================
                    KONIEC PLIKU MAIN.CPP
================================================================
*/
