/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2014        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "diskio.h"		/* FatFs lower layer API */
#include "image.h"
#include "nand.h"
#include "sdmmc.h"

#define PART_TYPE(pdrv) (DriveInfo[pdrv].type)
#define PART_SUBTYPE(pdrv) (DriveInfo[pdrv].subtype)

#define TYPE_NONE       0
#define TYPE_SYSNAND    NAND_SYSNAND
#define TYPE_EMUNAND    NAND_EMUNAND
#define TYPE_IMGNAND    NAND_IMGNAND
#define TYPE_SDCARD     (1UL<<4)
#define TYPE_IMAGE      (1UL<<5)
#define TYPE_RAMDRV     (1UL<<6)

#define SUBTYPE_CTRN    0
#define SUBTYPE_CTRN_N  1
#define SUBTYPE_CTRN_NO 2
#define SUBTYPE_TWLN    3
#define SUBTYPE_TWLP    4
#define SUBTYPE_NONE    5

typedef struct {
    BYTE  type;
    BYTE  subtype;
} FATpartition;

typedef struct {
    DWORD offset;
    DWORD size;
    BYTE  keyslot;
} SubtypeDesc;

FATpartition DriveInfo[5] = {
    { TYPE_SDCARD,  SUBTYPE_NONE },     // 0 - SDCARD
    { TYPE_SYSNAND, SUBTYPE_CTRN },     // 1 - SYSNAND CTRNAND
    { TYPE_SYSNAND, SUBTYPE_TWLN },     // 2 - SYSNAND TWLN
    { TYPE_SYSNAND, SUBTYPE_TWLP },     // 3 - SYSNAND TWLP
    { TYPE_IMAGE,   SUBTYPE_NONE }     // 4 - IMAGE
};

SubtypeDesc SubTypes[7] = {
    { 0x05C980, 0x17AE80, 0x04 },       // O3DS CTRNAND
    { 0x05C980, 0x20F680, 0x05 },       // N3DS CTRNAND
    { 0x05C980, 0x20F680, 0x04 },       // N3DS CTRNAND (downgraded)
    { 0x000097, 0x047DA9, 0x03 },       // TWLN
    { 0x04808D, 0x0105B3, 0x03 }        // TWLP
};

static BYTE nand_type_sys = 0;



/*-----------------------------------------------------------------------*/
/* Get Drive Subtype helper                                              */
/*-----------------------------------------------------------------------*/

static inline SubtypeDesc* get_subtype_desc(
    __attribute__((unused))
    BYTE pdrv		/* Physical drive number to identify the drive */
)
{
    BYTE subtype = PART_SUBTYPE(pdrv);
    
    if (subtype == SUBTYPE_NONE) {
        return NULL;
    } else if (subtype == SUBTYPE_CTRN) {
        if (nand_type_sys != NAND_TYPE_O3DS)
            subtype = (nand_type_sys == NAND_TYPE_N3DS) ? SUBTYPE_CTRN_N : SUBTYPE_CTRN_NO;
    }
    
    return &(SubTypes[subtype]);
}



/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
    __attribute__((unused))
    BYTE pdrv		/* Physical drive number to identify the drive */
)
{
    return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Initialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
    __attribute__((unused))
    BYTE pdrv				/* Physical drive number to identify the drive */
)
{
    if (pdrv == 0) { // a mounted SD card is the preriquisite for everything else
        if (sdmmc_sdcard_init() != 0) return STA_NOINIT|STA_NODISK;
    } else if (pdrv < 4) {
        nand_type_sys = CheckNandType();
        if (!nand_type_sys) return STA_NOINIT|STA_NODISK;
    } else if (pdrv == 4) {
        UINT mount_state = GetMountState();
        if (!mount_state) return STA_NOINIT|STA_NODISK;
    }
    return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
    __attribute__((unused))
    BYTE pdrv,		/* Physical drive number to identify the drive */
    BYTE *buff,		/* Data buffer to store read data */
    DWORD sector,	/* Sector address in LBA */
    UINT count		/* Number of sectors to read */
)
{   
    BYTE type = PART_TYPE(pdrv);
    
    if (type == TYPE_NONE) {
        return RES_PARERR;
    } else if (type == TYPE_SDCARD) {
        if (sdmmc_sdcard_readsectors(sector, count, buff))
            return RES_PARERR;
    } else if (type == TYPE_IMAGE) {
        if (ReadImageSectors(buff, sector, count))
            return RES_PARERR;
    } else {
        SubtypeDesc* subtype = get_subtype_desc(pdrv);
        BYTE keyslot = subtype->keyslot;
        DWORD isector = subtype->offset + sector;
        
        if (ReadNandSectors(buff, isector, count, keyslot))
            return RES_PARERR;
    }

    return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
DRESULT disk_write (
    __attribute__((unused))
    BYTE pdrv,			/* Physical drive number to identify the drive */
    const BYTE *buff,	/* Data to be written */
    DWORD sector,		/* Sector address in LBA */
    UINT count			/* Number of sectors to write */
)
{
    BYTE type = PART_TYPE(pdrv);
    
    if (type == TYPE_NONE) {
        return RES_PARERR;
    } else if (type == TYPE_SDCARD) {
        if (sdmmc_sdcard_writesectors(sector, count, (BYTE *)buff))
            return RES_PARERR;
    } else if (type == TYPE_IMAGE) {
        if (WriteImageSectors(buff, sector, count))
            return RES_PARERR;
    } else {
        SubtypeDesc* subtype = get_subtype_desc(pdrv);
        BYTE keyslot = subtype->keyslot;
        DWORD isector = subtype->offset + sector;
        
        if (WriteNandSectors(buff, isector, count, keyslot))
            return RES_PARERR; // unstubbed!
    }

    return RES_OK;
}
#endif



/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL
DRESULT disk_ioctl (
    __attribute__((unused))
    BYTE pdrv,		/* Physical drive number (0..) */
    __attribute__((unused))
    BYTE cmd,		/* Control code */
    __attribute__((unused))
    void *buff		/* Buffer to send/receive control data */
)
{
    BYTE type = PART_TYPE(pdrv);
    
    switch (cmd) {
        case GET_SECTOR_SIZE:
            *((DWORD*) buff) = 0x200;
            return RES_OK;
        case GET_SECTOR_COUNT:
            if (type == TYPE_SDCARD) { // SD card
                *((DWORD*) buff) = getMMCDevice(1)->total_size;
            } else if (type == TYPE_IMAGE) { // FAT image
                *((DWORD*) buff) = GetMountSize() / 0x200;
            } else if (type != TYPE_NONE) { // NAND
                *((DWORD*) buff) = get_subtype_desc(pdrv)->size;
            }
            return RES_OK;
        case GET_BLOCK_SIZE:
            *((DWORD*) buff) = (type == TYPE_IMAGE) ? 0x1 : 0x2000;
            return RES_OK;
        case CTRL_SYNC:
            if (type == TYPE_IMAGE)
                SyncImage();
            // nothing else to do here - sdmmc.c handles the rest
            return RES_OK;
    }
    
    return RES_PARERR;
}
#endif
