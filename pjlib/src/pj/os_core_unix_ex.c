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

// ADDED BY MAXSHDR START
#include <sys/stat.h> 
#include <sys/types.h>

PJ_DEF(void) pj_get_temporary_dir(char* temp_dir, unsigned int size)
{
    char buf[256];

    if (12 < size) {
        temp_dir[0] = '/';
        temp_dir[1] = 't';
        temp_dir[2] = 'm';
        temp_dir[3] = 'p';
        temp_dir[4] = '/';
        
        temp_dir[5] = 'p';
        temp_dir[6] = 'j';
        temp_dir[7] = 'a';
        temp_dir[8] = 'p';
        temp_dir[9] = 'p';
        temp_dir[10] = '/';
        temp_dir[11] = 0;

        mkdir(temp_dir, 755);

    }
    else {
        temp_dir[0] = 0;
    }
}

PJ_DEF(void) pj_log_memory_status()
{
    PJ_LOG(1, ("os_core_unix.c | pj_log_memory_status", "failed!"));
}
// ADDED BY MAXSHDR END
