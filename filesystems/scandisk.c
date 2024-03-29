/*
 * scandisk.c
 * Scanning disk for btrfs multi-devices
 * by Samuel Liao
 *
 * Copyright (c) 2013 Tencent, Inc.
 */
#include "fsw_efi.h"
#ifdef __MAKEWITH_GNUEFI
#include "edk2/DriverBinding.h"
#include "edk2/ComponentName.h"
#endif
#include "../include/refit_call_wrapper.h"

extern struct fsw_host_table   fsw_efi_host_table;
static void dummy_volume_free(struct fsw_volume *vol) { }
static struct fsw_fstype_table   dummy_fstype = {
    { FSW_STRING_TYPE_UTF8, 4, 4, "dummy" },
    sizeof(struct fsw_volume),
    sizeof(struct fsw_dnode),

    NULL, //volume_mount,
    dummy_volume_free, //volume_free,
    NULL, //volume_stat,
    NULL, //dnode_fill,
    NULL, //dnode_free,
    NULL, //dnode_stat,
    NULL, //get_extent,
    NULL, //dir_lookup,
    NULL, //dir_read,
    NULL, //readlink,
};

static struct fsw_volume *create_dummy_volume(EFI_DISK_IO *diskio, UINT32 mediaid)
{
    fsw_status_t err;
    struct fsw_volume *vol;
    FSW_VOLUME_DATA *Volume;

    err = fsw_alloc_zero(sizeof(struct fsw_volume), (void **)&vol);
    if(err)
        return NULL;
    err = fsw_alloc_zero(sizeof(FSW_VOLUME_DATA), (void **)&Volume);
    if(err) {
        fsw_free(vol);
        return NULL;
    }
    /* fstype_table->volume_free for fsw_unmount */
    vol->fstype_table = &dummy_fstype;
    /* host_data needded to fsw_block_get()/fsw_efi_read_block() */
    Volume->DiskIo = diskio;
    Volume->MediaId = mediaid;

    vol->host_data = Volume;
    vol->host_table = &fsw_efi_host_table;
    return vol;
}

static struct fsw_volume *clone_dummy_volume(struct fsw_volume *vol)
{
    FSW_VOLUME_DATA *Volume = (FSW_VOLUME_DATA *)vol->host_data;
    return create_dummy_volume(Volume->DiskIo, Volume->MediaId);
}

static void free_dummy_volume(struct fsw_volume *vol)
{
    fsw_free(vol->host_data);
    fsw_unmount(vol);
}

static int scan_disks(int (*hook)(struct fsw_volume *, struct fsw_volume *), struct fsw_volume *master)
{
    EFI_STATUS  Status;
    EFI_HANDLE *Handles;
    UINTN       HandleCount = 0;
    UINTN       i;
    UINTN       scanned = 0;

    // Driver hangs if compiled with GNU-EFI unless there's a Print() statement somewhere.
    // I'm still trying to track that down; in the meantime, work around it....
#if defined(__MAKEWITH_GNUEFI)
    Print(L" ");
#endif
    DPRINT(L"Scanning disks\n");
    Status = refit_call5_wrapper(BS->LocateHandleBuffer, ByProtocol, &PROTO_NAME(DiskIoProtocol), NULL, &HandleCount, &Handles);
    if (Status == EFI_NOT_FOUND)
        return -1;  // no filesystems. strange, but true...
    for (i = 0; i < HandleCount; i++) {
        EFI_DISK_IO *diskio;
        EFI_BLOCK_IO *blockio;
        Status = refit_call3_wrapper(BS->HandleProtocol, Handles[i], &PROTO_NAME(DiskIoProtocol), (VOID **) &diskio);
        if (Status != 0)
            continue;
        Status = refit_call3_wrapper(BS->HandleProtocol, Handles[i], &PROTO_NAME(BlockIoProtocol), (VOID **) &blockio);
        if (Status != 0)
            continue;
        struct fsw_volume *vol = create_dummy_volume(diskio, blockio->Media->MediaId);
        if(vol) {
            DPRINT(L"Checking disk %d\n", i);
            if(hook(master, vol) == FSW_SUCCESS)
                scanned++;
            free_dummy_volume(vol);
        }
    }
    return scanned;
}

