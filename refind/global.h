/*
 * refit/global.h
 * Global header file
 *
 * Copyright (c) 2006-2009 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Modifications copyright (c) 2012-2014 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), a copy of which must be distributed
 * with this source code or binaries made from it.
 *
 */

#ifndef __GLOBAL_H_
#define __GLOBAL_H_

#ifdef __MAKEWITH_GNUEFI
#include <efi.h>
#include <efilib.h>
#else
#include "../include/tiano_includes.h"
#endif
#include "../EfiLib/GenericBdsLib.h"

#include "libeg.h"

#define REFIT_DEBUG (0)

// Tag classifications; used in various ways.
#define TAG_ABOUT            (1)
#define TAG_REBOOT           (2)
#define TAG_SHUTDOWN         (3)
#define TAG_TOOL             (4)
#define TAG_LOADER           (5)
#define TAG_LEGACY           (6)
#define TAG_EXIT             (7)
#define TAG_SHELL            (8)
#define TAG_GPTSYNC          (9)
#define TAG_LEGACY_UEFI      (10)
#define TAG_APPLE_RECOVERY   (11)
#define TAG_WINDOWS_RECOVERY (12)
#define TAG_MOK_TOOL         (13)
#define TAG_FIRMWARE         (14)
#define TAG_MEMTEST          (15)
#define TAG_GDISK            (16)
#define NUM_TOOLS            (17)

#define NUM_SCAN_OPTIONS 10

#define DEFAULT_ICONS_DIR L"icons"

// OS bit codes; used in GlobalConfig.GraphicsOn
#define GRAPHICS_FOR_OSX        1
#define GRAPHICS_FOR_LINUX      2
#define GRAPHICS_FOR_ELILO      4
#define GRAPHICS_FOR_GRUB       8
#define GRAPHICS_FOR_WINDOWS   16

// Type of legacy (BIOS) boot support detected
#define LEGACY_TYPE_NONE 0
#define LEGACY_TYPE_MAC  1
#define LEGACY_TYPE_UEFI 2

#ifdef __MAKEWITH_GNUEFI
//
// define BBS Device Types
//
#define BBS_FLOPPY        0x01
#define BBS_HARDDISK      0x02
#define BBS_CDROM         0x03
#define BBS_PCMCIA        0x04
#define BBS_USB           0x05
#define BBS_EMBED_NETWORK 0x06
#define BBS_BEV_DEVICE    0x80
#define BBS_UNKNOWN       0xff
#endif

// BIOS Boot Specification (BBS) device types, as returned in DevicePath->Type field
#define DEVICE_TYPE_HW         0x01
#define DEVICE_TYPE_ACPI       0x02 /* returned by UEFI boot loader on USB */
#define DEVICE_TYPE_MESSAGING  0x03
#define DEVICE_TYPE_MEDIA      0x04 /* returned by EFI boot loaders on hard disk */
#define DEVICE_TYPE_BIOS       0x05 /* returned by legacy (BIOS) boot loaders */
#define DEVICE_TYPE_END        0x75 /* end of path */

// Filesystem type identifiers. Not all are yet used....
#define FS_TYPE_UNKNOWN        0
#define FS_TYPE_FAT            1
#define FS_TYPE_EXT2           2
#define FS_TYPE_EXT3           3
#define FS_TYPE_EXT4           4
#define FS_TYPE_HFSPLUS        5
#define FS_TYPE_REISERFS       6
#define FS_TYPE_BTRFS          7
#define FS_TYPE_ISO9660        8

// How to scale banner images
#define BANNER_NOSCALE         0
#define BANNER_FILLSCREEN      1

// Sizes of the default icons; badges are 1/4 the big icon size
#define DEFAULT_SMALL_ICON_SIZE 48
#define DEFAULT_BIG_ICON_SIZE   128

// Codes for types of icon sizes; used for indexing into GlobalConfig.IconSizes[]
#define ICON_SIZE_BADGE 0
#define ICON_SIZE_SMALL 1
#define ICON_SIZE_BIG   2

// Names of binaries that can manage MOKs....
#define MOK_NAMES               L"MokManager.efi,HashTool.efi,HashTool-signed.efi"
// Directories to search for these MOK-managing programs. Note that SelfDir is
// searched in addition to these locations....
#define MOK_LOCATIONS           L"\\,EFI\\tools,EFI\\fedora,EFI\\redhat,EFI\\ubuntu,EFI\\suse,EFI\\opensuse,EFI\\altlinux"
// Directories to search for memtest86....
#define MEMTEST_LOCATIONS       L"EFI\\tools,EFI\\tools\\memtest86,EFI\\tools\\memtest,EFI\\memtest86,EFI\\memtest"
// Files that may be Windows recovery files
#define WINDOWS_RECOVERY_FILES  L"EFI\\Microsoft\\Boot\\LrsBootmgr.efi"

#define NULL_GUID_VALUE { 0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} };
#define REFIND_GUID_VALUE { 0x36D08FA7, 0xCF0B, 0x42F5, {0x8F, 0x14, 0x68, 0xDF, 0x73, 0xED, 0x37, 0x40} };

//
// global definitions
//

// global types

typedef struct {
   UINT8 Flags;
   UINT8 StartCHS1;
   UINT8 StartCHS2;
   UINT8 StartCHS3;
   UINT8 Type;
   UINT8 EndCHS1;
   UINT8 EndCHS2;
   UINT8 EndCHS3;
   UINT32 StartLBA;
   UINT32 Size;
} MBR_PARTITION_INFO;

typedef struct {
   EFI_DEVICE_PATH     *DevicePath;
   EFI_HANDLE          DeviceHandle;
   EFI_FILE            *RootDir;
   CHAR16              *VolName;
   CHAR16              *PartName;
   EFI_GUID            VolUuid;
   EFI_GUID            PartGuid;
   UINTN               VolNumber;
   EG_IMAGE            *VolIconImage;
   EG_IMAGE            *VolBadgeImage;
   UINTN               DiskKind;
   BOOLEAN             IsAppleLegacy;
   BOOLEAN             HasBootCode;
   CHAR16              *OSIconName;
   CHAR16              *OSName;
   BOOLEAN             IsMbrPartition;
   UINTN               MbrPartitionIndex;
   EFI_BLOCK_IO        *BlockIO;
   UINT64              BlockIOOffset;
   EFI_BLOCK_IO        *WholeDiskBlockIO;
   EFI_DEVICE_PATH     *WholeDiskDevicePath;
   MBR_PARTITION_INFO  *MbrPartitionTable;
   BOOLEAN             IsReadable;
   UINT32              FSType;
} REFIT_VOLUME;

typedef struct _refit_menu_entry {
   CHAR16      *Title;
   UINTN       Tag;
   UINTN       Row;
   CHAR16      ShortcutDigit;
   CHAR16      ShortcutLetter;
   EG_IMAGE    *Image;
   EG_IMAGE    *BadgeImage;
   struct _refit_menu_screen *SubScreen;
} REFIT_MENU_ENTRY;

typedef struct _refit_menu_screen {
   CHAR16      *Title;
   EG_IMAGE    *TitleImage;
   UINTN       InfoLineCount;
   CHAR16      **InfoLines;
   UINTN       EntryCount;     // total number of entries registered
   REFIT_MENU_ENTRY **Entries;
   UINTN       TimeoutSeconds;
   CHAR16      *TimeoutText;
   CHAR16      *Hint1;
   CHAR16      *Hint2;
} REFIT_MENU_SCREEN;

typedef struct {
   REFIT_MENU_ENTRY me;
   CHAR16           *Title;
   CHAR16           *LoaderPath;
   CHAR16           *VolName;
   EFI_DEVICE_PATH  *DevicePath;
   BOOLEAN          UseGraphicsMode;
   BOOLEAN          Enabled;
   CHAR16           *LoadOptions;
   CHAR16           *InitrdPath; // Linux stub loader only
   CHAR8            OSType;
} LOADER_ENTRY;

typedef struct {
   REFIT_MENU_ENTRY  me;
   REFIT_VOLUME      *Volume;
   BDS_COMMON_OPTION *BdsOption;
   CHAR16            *LoadOptions;
   BOOLEAN           Enabled;
} LEGACY_ENTRY;

typedef struct {
   BOOLEAN     TextOnly;
   BOOLEAN     ScanAllLinux;
   BOOLEAN     DeepLegacyScan;
   UINTN       RequestedScreenWidth;
   UINTN       RequestedScreenHeight;
   UINTN       BannerBottomEdge;
   UINTN       RequestedTextMode;
   UINTN       Timeout;
   UINTN       HideUIFlags;
   UINTN       MaxTags;     // max. number of OS entries to show simultaneously in graphics mode
   UINTN       GraphicsFor;
   UINTN       LegacyType;
   UINTN       ScanDelay;
   UINTN       ScreensaverTime;
   UINTN       IconSizes[3];
   UINTN       BannerScale;
   CHAR16      *BannerFileName;
   EG_IMAGE    *ScreenBackground;
   CHAR16      *ConfigFilename;
   CHAR16      *SelectionSmallFileName;
   CHAR16      *SelectionBigFileName;
   CHAR16      *DefaultSelection;
   CHAR16      *AlsoScan;
   CHAR16      *DontScanVolumes;
   CHAR16      *DontScanDirs;
   CHAR16      *DontScanFiles;
   CHAR16      *WindowsRecoveryFiles;
   CHAR16      *DriverDirs;
   CHAR16      *IconsDir;
   UINTN       ShowTools[NUM_TOOLS];
   CHAR8       ScanFor[NUM_SCAN_OPTIONS]; // codes of types of loaders for which to scan
} REFIT_CONFIG;

// Global variables

extern EFI_HANDLE       SelfImageHandle;
extern EFI_LOADED_IMAGE *SelfLoadedImage;
extern EFI_FILE         *SelfRootDir;
extern EFI_FILE         *SelfDir;
extern CHAR16           *SelfDirPath;

extern REFIT_VOLUME     *SelfVolume;
extern REFIT_VOLUME     **Volumes;
extern UINTN            VolumesCount;

extern REFIT_CONFIG     GlobalConfig;

extern EFI_GUID gEfiLegacyBootProtocolGuid;
extern EFI_GUID gEfiGlobalVariableGuid;

LOADER_ENTRY *InitializeLoaderEntry(IN LOADER_ENTRY *Entry);
REFIT_MENU_SCREEN *InitializeSubScreen(IN LOADER_ENTRY *Entry);
VOID GenerateSubScreen(LOADER_ENTRY *Entry, IN REFIT_VOLUME *Volume);
LOADER_ENTRY * MakeGenericLoaderEntry(VOID);
LOADER_ENTRY * AddLoaderEntry(IN CHAR16 *LoaderPath, IN CHAR16 *LoaderTitle, IN REFIT_VOLUME *Volume);
VOID SetLoaderDefaults(LOADER_ENTRY *Entry, CHAR16 *LoaderPath, IN REFIT_VOLUME *Volume);
LOADER_ENTRY * AddPreparedLoaderEntry(LOADER_ENTRY *Entry);

#endif

/* EOF */
