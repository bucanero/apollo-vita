/*
  VitaShell
  Copyright (C) 2015-2018, TheFloW

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __VITASHELL_KERNEL_H__
#define __VITASHELL_KERNEL_H__

/*
  SceAppMgr mount IDs:
  0x64: ux0:picture
  0x65: ur0:user/00/psnfriend
  0x66: ur0:user/00/psnmsg
  0x69: ux0:music
  0x6E: ux0:appmeta
  0xC8: ur0:temp/sqlite
  0xCD: ux0:cache
  0x12E: ur0:user/00/trophy/data/sce_trop
  0x12F: ur0:user/00/trophy/data
  0x3E8: ux0:app, vs0:app, gro0:app
  0x3E9: ux0:patch
  0x3EB: ?
  0x3EA: ux0:addcont
  0x3EC: ux0:theme
  0x3ED: ux0:user/00/savedata
  0x3EE: ur0:user/00/savedata
  0x3EF: vs0:sys/external
  0x3F0: vs0:data/external
*/

typedef struct {
  int id;
  const char *process_titleid;
  const char *path;
  const char *desired_mount_point;
  const void *klicensee;
  char *mount_point;
} ShellMountIdArgs;

int shellKernelIsUx0Redirected(const char *blkdev, const char *blkdev2);
int shellKernelRedirectUx0(const char *blkdev, const char *blkdev2);
int shellKernelMountById(ShellMountIdArgs *args);
int shellKernelGetRifVitaKey(const void *license_buf, void *klicensee);

#endif
