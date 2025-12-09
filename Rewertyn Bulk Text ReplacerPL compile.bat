rem Zmiana strony kodowej na UTF-8
chcp 65001 > nul
g++ "Rewertyn Bulk Text ReplacerPL v1.0.cpp" -o "Rewertyn Bulk Text ReplacerPL v1.0.exe" -std=c++17 -mwindows -lcomdlg32 -lshell32 -lole32 -luuid -lgdi32 -static -municode -DUNICODE -D_UNICODE

pause