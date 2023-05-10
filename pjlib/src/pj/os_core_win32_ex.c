/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pj/os.h>
#include <pj/log.h>
#include <pj/assert.h>
#include <pj/errno.h>

#include <windows.h>
// ADDED BY MAXSHDR START
#define _WIN32_WINNT    0x0602
#include <psapi.h>

PJ_DEF(void) pj_get_temporary_dir(char* temp_dir, unsigned int size)
{
    char buf[256];

    DWORD res = GetTempPathA(sizeof(buf), buf);
    if (res != 0 && (res + 7 < size)) {
        strncpy(temp_dir, buf, res);
        *(temp_dir + res) = 'p';
        *(temp_dir + res + 1) = 'j';
        *(temp_dir + res + 2) = 'a';
        *(temp_dir + res + 3) = 'p';
        *(temp_dir + res + 4) = 'p';
        *(temp_dir + res + 5) = '\\';
        *(temp_dir + res + 6) = 0;

        DWORD dwAttrib = GetFileAttributes(temp_dir);

        if (!(dwAttrib != INVALID_FILE_ATTRIBUTES &&
            (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))) {
            CreateDirectoryA(temp_dir, NULL);
        }
    }
    else {
        temp_dir[0] = 0;
    }
}

PJ_DEF(void) pj_log_memory_status()
{
    PROCESS_MEMORY_COUNTERS_EX proc_memory;

    if (GetProcessMemoryInfo(GetCurrentProcess(), &proc_memory, sizeof(proc_memory))) {

        PJ_LOG(3, ("os_core_win32.c | pj_log_memory_status", "MemInfo:\n"
                   "PeakWorkingSetSize: %u | WorkingSetSize: %u | QuotaPeakPagedPoolUsage: %u | QuotaPagedPoolUsage: %u\n"
                   "QuotaPeakNonPagedPoolUsage: %u | QuotaNonPagedPoolUsage: %u | PagefileUsage: %u | PeakPagefileUsage: %u\n"
                   "PrivateUsage: %u\n",
            proc_memory.PeakWorkingSetSize, proc_memory.WorkingSetSize, proc_memory.QuotaPeakPagedPoolUsage, proc_memory.QuotaPagedPoolUsage,
            proc_memory.QuotaPeakNonPagedPoolUsage, proc_memory.QuotaNonPagedPoolUsage, proc_memory.PagefileUsage, proc_memory.PeakPagefileUsage,
            proc_memory.PrivateUsage));
    }
    else {
        PJ_LOG(1, ("os_core_win32.c | pj_log_memory_status", "GetProcessMemoryInfo failed!"));
    }
}
// ADDED BY MAXSHDR END
