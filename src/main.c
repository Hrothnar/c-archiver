/*
 * backup_with_minizip_split.c
 *
 * Backup utility for Windows with optional split behavior:
 * - Resolves .lnk shortcuts via COM
 * - Recursively collects files, preserves structure, metadata, Unicode filenames
 * - Creates a single ZIP or separate ZIPs per link via --split
 * - Excludes hidden, system files and desktop.ini
 * - Displays console progress with fixed-width percentages
 *
 * Requirements:
 *  - Windows OS
 *  - GCC (MinGW-w64)
 *  - minizip-ng library with zstd, bcrypt
 *  - Link: -lminizip-ng -lzstd -lbcrypt -luuid -lshell32 -lshlwapi -lcomdlg32 -lole32 -loleaut32
 *
 * Build:
 * gcc -std=c23 -o backup_with_minizip_split.exe main.c \
 *   -Iinclude -Llib \
 *   -lminizip-ng -lzstd -lbcrypt -luuid -lshell32 \
 *   -lshlwapi -lcomdlg32 -lole32 -loleaut32
 *
 * Usage:
 *  Single archive: backup_with_minizip_split.exe <source_folder> <output_zip>
 *  Split archives:  backup_with_minizip_split.exe --split <source_folder> <output_dir>
 */

 #include <windows.h>
 #include <shlobj.h>
 #include <shobjidl.h>
 #include <objbase.h>
 #include <shellapi.h>
 #include <stdio.h>
 #include <stdbool.h>
 #include <stdlib.h>
 #include <locale.h>
 
 #include "mz.h"
 #include "mz_strm.h"
 #include "mz_strm_buf.h"
 #include "mz_strm_os.h"
 #include "mz_zip.h"
 #include "mz_zip_rw.h"
 
 #define PATH_MAX_LEN 1024
 
 typedef struct FileEntry {
     wchar_t full[PATH_MAX_LEN];
     wchar_t rel[PATH_MAX_LEN];
 } FileEntry;
 
 // Recursively collect file paths under base_dir, excluding hidden/system and desktop.ini
 static void collect_entries(const wchar_t *base_dir, const wchar_t *curr_dir,
                              FileEntry **arr, int *cnt, int *cap) {
     WIN32_FIND_DATAW ffd;
     HANDLE hFind;
     wchar_t pattern[PATH_MAX_LEN];
     wsprintfW(pattern, L"%s\\*", curr_dir);
     hFind = FindFirstFileW(pattern, &ffd);
     if (hFind == INVALID_HANDLE_VALUE) return;
     do {
         // Skip . and ..
         if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0)
             continue;
         // Skip hidden and system
         if (ffd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))
             continue;
         // Skip desktop.ini
         if (_wcsicmp(ffd.cFileName, L"desktop.ini") == 0)
             continue;
         wchar_t full_path[PATH_MAX_LEN];
         wsprintfW(full_path, L"%s\\%s", curr_dir, ffd.cFileName);
         // compute relative path
         size_t base_len = wcslen(base_dir);
         wchar_t rel_part[PATH_MAX_LEN];
         if (wcsncmp(full_path, base_dir, base_len) == 0 && full_path[base_len] == L'\\')
             wcscpy(rel_part, full_path + base_len + 1);
         else
             wcscpy(rel_part, ffd.cFileName);
         // resize array if needed
         if (*cnt >= *cap) {
             *cap *= 2;
             *arr = realloc(*arr, (*cap) * sizeof(FileEntry));
         }
         wcscpy((*arr)[*cnt].full, full_path);
         wcscpy((*arr)[*cnt].rel, rel_part);
         (*cnt)++;
         if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
             collect_entries(base_dir, full_path, arr, cnt, cap);
     } while (FindNextFileW(hFind, &ffd));
     FindClose(hFind);
 }
 
 // Resolve .lnk shortcut to target path
 static BOOL resolve_link(LPCWSTR link_path, LPWSTR out_path, size_t max_len) {
     IShellLinkW *psl = NULL;
     IPersistFile *ppf = NULL;
     HRESULT hr = CoInitialize(NULL);
     if (FAILED(hr)) return FALSE;
     hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                           &IID_IShellLinkW, (void**)&psl);
     if (SUCCEEDED(hr)) hr = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void**)&ppf);
     if (SUCCEEDED(hr)) hr = ppf->lpVtbl->Load(ppf, link_path, STGM_READ);
     if (SUCCEEDED(hr)) hr = psl->lpVtbl->GetPath(psl, out_path, (int)max_len, NULL, SLGP_RAWPATH);
     if (ppf) ppf->lpVtbl->Release(ppf);
     if (psl) psl->lpVtbl->Release(psl);
     CoUninitialize();
     return SUCCEEDED(hr);
 }
 
 // Write entries to a ZIP file, excluding hidden/system and desktop.ini in archive step as well
 static void zip_entries(const wchar_t *zip_path_w, FileEntry *entries, int count) {
     char zipPath[PATH_MAX_LEN];
     WideCharToMultiByte(CP_UTF8, 0, zip_path_w, -1, zipPath, PATH_MAX_LEN, NULL, NULL);
     void *zip = mz_zip_writer_create();
     if (mz_zip_writer_open_file(zip, zipPath, 0, 0) != MZ_OK) {
         fwprintf(stderr, L"Cannot open %s\n", zip_path_w);
         mz_zip_writer_delete(&zip);
         return;
     }
     for (int i = 0; i < count; i++) {
         DWORD attr = GetFileAttributesW(entries[i].full);
         // Skip hidden/system
         if (attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))
             continue;
         // Skip desktop.ini by name
         {
             const wchar_t *fname = wcsrchr(entries[i].full, L'\\');
             if (fname && _wcsicmp(fname + 1, L"desktop.ini") == 0)
                 continue;
         }
         char fullUtf[PATH_MAX_LEN], relUtf[PATH_MAX_LEN];
         WideCharToMultiByte(CP_UTF8, 0, entries[i].full, -1, fullUtf, PATH_MAX_LEN, NULL, NULL);
         WideCharToMultiByte(CP_UTF8, 0, entries[i].rel, -1, relUtf, PATH_MAX_LEN, NULL, NULL);
         int pct = (i * 100) / count;
         wprintf(L"[%3d%%] %s\r", pct, entries[i].rel);
         if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
             mz_zip_writer_add_file(zip, fullUtf, relUtf);
         }
     }
     wprintf(L"\nDone: %d items -> %s\n", count, zip_path_w);
     mz_zip_writer_close(zip);
     mz_zip_writer_delete(&zip);
 }
 
 int wmain(int argc, wchar_t *argv[]) {
     setlocale(LC_ALL, "");
     bool split = false;
     int arg = 1;
     if (argc > 1 && wcscmp(argv[1], L"--split") == 0) {
         split = true;
         arg++;
     }
     if ((!split && argc != 3) || (split && argc != 4)) {
         fwprintf(stderr, L"Usage: %s [--split] <source_folder> <output_%s>\n",
                 argv[0], split ? L"directory" : L"zip");
         return 1;
     }
     LPCWSTR source_folder = argv[arg];
     LPCWSTR output = argv[arg + 1];
 
     if (split) {
         // Ensure output directory exists
         if (!CreateDirectoryW(output, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
             fwprintf(stderr, L"Cannot create or access output dir %s\n", output);
             return 1;
         }
         // Enumerate .lnk files
         WIN32_FIND_DATAW ffd;
         wchar_t pattern[PATH_MAX_LEN];
         wsprintfW(pattern, L"%s\\*.lnk", source_folder);
         HANDLE hFind = FindFirstFileW(pattern, &ffd);
         if (hFind == INVALID_HANDLE_VALUE) {
             wprintf(L"No .lnk files found in %s\n", source_folder);
             return 1;
         }
         do {
             wchar_t link_path[PATH_MAX_LEN]; wsprintfW(link_path, L"%s\\%s", source_folder, ffd.cFileName);
             wchar_t target_dir[PATH_MAX_LEN]; if (!resolve_link(link_path, target_dir, PATH_MAX_LEN)) continue;
             wchar_t link_name[PATH_MAX_LEN]; wcscpy(link_name, ffd.cFileName);
             wchar_t *dot = wcsrchr(link_name, L'.'); if (dot) *dot = L'\0';
             const wchar_t suf[] = L" - Ярлык"; size_t ln = wcslen(link_name), sl = wcslen(suf);
             if (ln>sl && wcscmp(link_name+ln-sl, suf)==0) link_name[ln-sl]=L'\0';
             FileEntry *temp = malloc(16*sizeof(FileEntry)); int cnt=0, cap=16;
             collect_entries(target_dir, target_dir, &temp, &cnt, &cap);
             wchar_t zip_path[PATH_MAX_LEN]; wsprintfW(zip_path, L"%s\\%s.zip", output, link_name);
             zip_entries(zip_path, temp, cnt);
             free(temp);
         } while (FindNextFileW(hFind, &ffd));
         FindClose(hFind);
     } else {
         FileEntry *entries = malloc(16 * sizeof(FileEntry)); int count=0, cap=16;
         WIN32_FIND_DATAW ffd;
         wchar_t link_pattern[PATH_MAX_LEN]; wsprintfW(link_pattern, L"%s\\*.lnk", source_folder);
         HANDLE hFind = FindFirstFileW(link_pattern, &ffd);
         if (hFind != INVALID_HANDLE_VALUE) {
             do {
                 wchar_t link_path[PATH_MAX_LEN]; wsprintfW(link_path, L"%s\\%s", source_folder, ffd.cFileName);
                 wchar_t target_dir[PATH_MAX_LEN]; if (!resolve_link(link_path, target_dir, PATH_MAX_LEN)) continue;
                 wchar_t link_name[PATH_MAX_LEN]; wcscpy(link_name, ffd.cFileName);
                 wchar_t *dot = wcsrchr(link_name, L'.'); if (dot) *dot = L'\0';
                 const wchar_t suf[] = L" - Ярлык"; size_t ln=wcslen(link_name), sl=wcslen(suf);
                 if (ln>sl && wcscmp(link_name+ln-sl,suf)==0) link_name[ln-sl]=L'\0';
                 FileEntry *temp = malloc(16*sizeof(FileEntry)); int tcount=0, tcap=16;
                 collect_entries(target_dir, target_dir, &temp, &tcount, &tcap);
                 for (int j=0; j<tcount; j++) {
                     if (count>=cap) { cap*=2; entries=realloc(entries, cap*sizeof(FileEntry)); }
                     wcscpy(entries[count].full, temp[j].full);
                     wsprintfW(entries[count].rel, L"%s\\%s", link_name, temp[j].rel);
                     count++;
                 }
                 free(temp);
             } while (FindNextFileW(hFind, &ffd));
             FindClose(hFind);
         }
         if (count == 0) { wprintf(L"No files to archive.\n"); return 1; }
         zip_entries(output, entries, count);
         free(entries);
     }
     return 0;
 }
 
 #if !defined(__MINGW64_VERSION_MAJOR__)
 int main(int argc, char **argv) {
     int wargc;
     wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
     int res = wmain(wargc, wargv);
     LocalFree(wargv);
     return res;
 }
 #endif