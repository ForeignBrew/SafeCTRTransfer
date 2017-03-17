#include "nand.h"
#include "aes.h"
#include "sha.h"
#include "sdmmc.h"
#include "qff.h"

static u8 slot0x05KeyY[0x10] = { 0x00 }; // need to load this from FIRM0 / external file
static const u8 slot0x05KeyY_sha256[0x20] = { // hash for slot0x05KeyY (16 byte)
    0x98, 0x24, 0x27, 0x14, 0x22, 0xB0, 0x6B, 0xF2, 0x10, 0x96, 0x9C, 0x36, 0x42, 0x53, 0x7C, 0x86,
    0x62, 0x22, 0x5C, 0xFD, 0x6F, 0xAE, 0x9B, 0x0A, 0x85, 0xA5, 0xCE, 0x21, 0xAA, 0xB6, 0xC8, 0x4D
};

static const u8 slot0x11Key95_sha256[0x20] = { // slot0x11Key95 hash (first 16 byte of sector0x96)
    0xBA, 0xC1, 0x40, 0x9C, 0x6E, 0xE4, 0x1F, 0x04, 0xAA, 0xC4, 0xE2, 0x09, 0x5C, 0xE9, 0x4F, 0x78, 
    0x6C, 0x78, 0x5F, 0xAC, 0xEC, 0x7E, 0xC0, 0x11, 0x26, 0x9D, 0x4E, 0x47, 0xB3, 0x64, 0xC4, 0xA5
};
    
static const u8 nand_magic_n3ds[0x60] = { // NCSD NAND header N3DS magic
    0x4E, 0x43, 0x53, 0x44, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x04, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x01, 0x02, 0x02, 0x02, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x05, 0x00, 0x00, 0x88, 0x05, 0x00, 0x80, 0x01, 0x00, 0x00,
    0x80, 0x89, 0x05, 0x00, 0x00, 0x20, 0x00, 0x00, 0x80, 0xA9, 0x05, 0x00, 0x00, 0x20, 0x00, 0x00,
    0x80, 0xC9, 0x05, 0x00, 0x80, 0xF6, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const u8 nand_magic_o3ds[0x60] = { // NCSD NAND header O3DS magic
    0x4E, 0x43, 0x53, 0x44, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x04, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x01, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x05, 0x00, 0x00, 0x88, 0x05, 0x00, 0x80, 0x01, 0x00, 0x00,
    0x80, 0x89, 0x05, 0x00, 0x00, 0x20, 0x00, 0x00, 0x80, 0xA9, 0x05, 0x00, 0x00, 0x20, 0x00, 0x00,
    0x80, 0xC9, 0x05, 0x00, 0x80, 0xAE, 0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const u8 twl_mbr[0x42] = { // encrypted version inside the NCSD NAND header (@0x1BE)
    0x00, 0x04, 0x18, 0x00, 0x06, 0x01, 0xA0, 0x3F, 0x97, 0x00, 0x00, 0x00, 0xA9, 0x7D, 0x04, 0x00,
    0x00, 0x04, 0x8E, 0x40, 0x06, 0x01, 0xA0, 0xC3, 0x8D, 0x80, 0x04, 0x00, 0xB3, 0x05, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x55, 0xAA
};

static u8 CtrNandCtr[16];
static u8 TwlNandCtr[16];
static u8 OtpSha256[32] = { 0 };


u32 LoadKeyYFromP9(u8* key, const u8* keyhash, u32 offset, u32 keyslot)
{
    static u32 offsetA9l = 0x066A00; // fixed offset, this only has to work for FIRM90 / FIRM81
    u8 ctr0x15[16] __attribute__((aligned(32)));
    u8 keyY0x15[16] __attribute__((aligned(32)));
    u8 keyY[16] __attribute__((aligned(32)));
    u8 header[0x200];
    
    // check arm9loaderhax
    if (!CheckA9lh() || (offset < (offsetA9l + 0x0800))) return 1;
    
    // section 2 (arm9loader) header of FIRM
    // this is @0x066A00 in FIRM90 & FIRM81
    ReadNandBytes(header, (SECTOR_FIRM0 * 0x200) + offsetA9l, 0x200, 0x06);
    memcpy(keyY0x15, header + 0x10, 0x10); // 0x15 keyY
    memcpy(ctr0x15, header + 0x20, 0x10); // 0x15 counter
    
    // read and decrypt the encrypted keyY
    ReadNandBytes(keyY, (SECTOR_FIRM0 * 0x200) + offset, 0x10, 0x06);
    setup_aeskeyY(0x15, keyY0x15);
    use_aeskey(0x15);
    ctr_decrypt_byte(keyY, keyY, 0x10, offset - (offsetA9l + 0x800), AES_CNT_CTRNAND_MODE, ctr0x15);
    if (key) memcpy(key, keyY, 0x10);
    
    // check the key
    u8 shasum[0x32];
    sha_quick(shasum, keyY, 16, SHA256_MODE);
    if (memcmp(shasum, keyhash, 32) == 0) {
        setup_aeskeyY(keyslot, keyY);
        use_aeskey(keyslot);
        return 0;
    }
    
    return 1;
}

bool InitNandCrypto(void)
{   
    // part #0: KeyX / KeyY for secret sector 0x96
    // on a9lh this MUST be run before accessing the SHA register in any other way
    if (CheckA9lh()) { // for a9lh
        // store the current SHA256 from register
        memcpy(OtpSha256, (void*) REG_SHAHASH, 32);
    }
        
    // part #1: Get NAND CID, set up TWL/CTR counter
    u32 NandCid[4];
    u8 shasum[32];
    
    sdmmc_sdcard_init();
    sdmmc_get_cid(1, NandCid);
    sha_quick(shasum, (u8*) NandCid, 16, SHA256_MODE);
    memcpy(CtrNandCtr, shasum, 16);
    sha_quick(shasum, (u8*) NandCid, 16, SHA1_MODE);
    for(u32 i = 0; i < 16; i++) // little endian and reversed order
        TwlNandCtr[i] = shasum[15-i];
    
    // part #2: TWL KEY
    // see: https://www.3dbrew.org/wiki/Memory_layout#ARM9_ITCM
    if (CheckA9lh()) { // only for a9lh
        u32* TwlCustId = (u32*) (0x01FFB808);
        u8 TwlKeyX[16] __attribute__((aligned(32)));
        u8 TwlKeyY[16] __attribute__((aligned(32)));
        
        // thanks b1l1s & Normmatt
        // see source from https://gbatemp.net/threads/release-twltool-dsi-downgrading-save-injection-etc-multitool.393488/
        const char* nintendo = "NINTENDO";
        u32 TwlKeyXW0 = (TwlCustId[0] ^ 0xB358A6AF) | 0x80000000;
        u32 TwlKeyXW3 = TwlCustId[1] ^ 0x08C267B7;
        memcpy(TwlKeyX +  4, nintendo, 8);
        memcpy(TwlKeyX +  0, &TwlKeyXW0, 4);
        memcpy(TwlKeyX + 12, &TwlKeyXW3, 4);
        
        // see: https://www.3dbrew.org/wiki/Memory_layout#ARM9_ITCM
        u32 TwlKeyYW3 = 0xE1A00005;
        memcpy(TwlKeyY, (u8*) 0x01FFD3C8, 12);
        memcpy(TwlKeyY + 12, &TwlKeyYW3, 4);
        
        setup_aeskeyX(0x03, TwlKeyX);
        setup_aeskeyY(0x03, TwlKeyY);
        use_aeskey(0x03);
    }
    
    // part #3: CTRNAND N3DS KEY
    // thanks AuroraWright and Gelex for advice on this
    // see: https://github.com/AuroraWright/Luma3DS/blob/master/source/crypto.c#L347
    if (CheckA9lh()) { // only for a9lh
        // keyY 0x05 is encrypted @0x0EB014 in the FIRM90
        // keyY 0x05 is encrypted @0x0EB24C in the FIRM81
        if ((LoadKeyYFromP9(slot0x05KeyY, slot0x05KeyY_sha256, 0x0EB014, 0x05) != 0) &&
            (LoadKeyYFromP9(slot0x05KeyY, slot0x05KeyY_sha256, 0x0EB24C, 0x05) != 0)) {};
    }
    
    return true;
}

bool CheckSlot0x05Crypto(void)
{
    // step #1 - check the slot0x05KeyY SHA-256
    u8 shasum[32];
    sha_quick(shasum, slot0x05KeyY, 16, SHA256_MODE);
    if (memcmp(shasum, slot0x05KeyY_sha256, 32) == 0)
        return true;
    
    // step #2 - check actual CTRNAND magic
    const u8 magic[8] = {0xE9, 0x00, 0x00, 0x43, 0x54, 0x52, 0x20, 0x20}; 
    const u32 sector = 0x05CAD7;
    u8 buffer[0x200];
    ReadNandSectors(buffer, sector, 1, 0x05);
    if (memcmp(buffer, magic, 8) == 0)
        return true;
    
    // failed if we arrive here
    return false;
}

bool CheckSector0x96Crypto(void)
{
    u8 buffer[0x200];
    ReadNandSectors(buffer, 0x96, 1, 0x11);
    return (sha_cmp(slot0x11Key95_sha256, buffer, 16, SHA256_MODE) == 0);
}

bool CheckFirmCrypto(void)
{
    // check the FIRM magic
    const u8 magic[8] = {'F', 'I', 'R', 'M'}; 
    const u32 sectors[] = { SECTOR_FIRM0, SECTOR_FIRM1 };
    u8 buffer[0x200];
    for (u32 i = 0; i < sizeof(sectors) / sizeof(u32); i++) {
        ReadNandSectors(buffer, sectors[i], 1, 0x06);
        if (memcmp(buffer, magic, sizeof(magic)) != 0) return false;
    }
    
    // success if we arrive here
    return true;
}

bool CheckA9lh(void)
{
    return ((*(vu32*) 0x101401C0) == 0);
}

void CryptNand(void* buffer, u32 sector, u32 count, u32 keyslot)
{
    u32 mode = (sector >= SECTOR_TWL + SIZE_TWL) ? AES_CNT_CTRNAND_MODE : AES_CNT_TWLNAND_MODE;
    u8 ctr[16] __attribute__((aligned(32)));
    u32 blocks = count * (0x200 / 0x10);
    
    // copy NAND CTR and increment it
    memcpy(ctr, (sector >= SECTOR_TWL + SIZE_TWL) ? CtrNandCtr : TwlNandCtr, 16);
    add_ctr(ctr, sector * (0x200 / 0x10));
    
    // decrypt the data
    use_aeskey(keyslot);
    ctr_decrypt(buffer, buffer, blocks, mode, ctr);
}

void CryptSector0x96(void* buffer, bool encrypt)
{
    u32 mode = encrypt ? AES_CNT_ECB_ENCRYPT_MODE : AES_CNT_ECB_DECRYPT_MODE;
    
    // setup the key
    setup_aeskeyX(0x11, OtpSha256);
    setup_aeskeyY(0x11, OtpSha256 + 16);
    
    // decrypt the sector
    use_aeskey(0x11);
    ecb_decrypt(buffer, buffer, 0x200 / AES_BLOCK_SIZE, mode);
}

int ReadNandBytes(void* buffer, u32 offset, u32 count, u32 keyslot)
{
    u8* buffer8 = (u8*) buffer;
    if (!(offset % 0x200) && !(count % 0x200)) { // aligned data -> simple case 
        // simple wrapper function for ReadNandSectors(...)
        return ReadNandSectors(buffer8, offset / 0x200, count / 0x200, keyslot);
    } else { // misaligned data -> -___-
        u8 l_buffer[0x200];
        int errorcode = 0;
        if (offset % 0x200) { // handle misaligned offset
            u32 offset_fix = 0x200 - (offset % 0x200);
            errorcode = ReadNandSectors(l_buffer, offset / 0x200, 1, keyslot);
            if (errorcode != 0) return errorcode;
            memcpy(buffer8, l_buffer + 0x200 - offset_fix, min(offset_fix, count));
            if (count <= offset_fix) return 0;
            offset += offset_fix;
            buffer8 += offset_fix;
            count -= offset_fix;
        } // offset is now aligned and part of the data is read
        if (count >= 0x200) { // otherwise this is misaligned and will be handled below
            errorcode = ReadNandSectors(buffer8, offset / 0x200, count / 0x200, keyslot);
            if (errorcode != 0) return errorcode;
        }
        if (count % 0x200) { // handle misaligned count
            u32 count_fix = count % 0x200;
            errorcode = ReadNandSectors(l_buffer, (offset + count) / 0x200, 1, keyslot);
            if (errorcode != 0) return errorcode;
            memcpy(buffer8 + count - count_fix, l_buffer, count_fix);
        }
        return errorcode;
    }
}

int WriteNandBytes(const void* buffer, u32 offset, u32 count, u32 keyslot)
{
    u8* buffer8 = (u8*) buffer;
    if (!(offset % 0x200) && !(count % 0x200)) { // aligned data -> simple case 
        // simple wrapper function for WriteNandSectors(...)
        return WriteNandSectors(buffer8, offset / 0x200, count / 0x200, keyslot);
    } else { // misaligned data -> -___-
        u8 l_buffer[0x200];
        int errorcode = 0;
        if (offset % 0x200) { // handle misaligned offset
            u32 offset_fix = 0x200 - (offset % 0x200);
            errorcode = ReadNandSectors(l_buffer, offset / 0x200, 1, keyslot);
            if (errorcode != 0) return errorcode;
            memcpy(l_buffer + 0x200 - offset_fix, buffer8, min(offset_fix, count));
            errorcode = WriteNandSectors((const u8*) l_buffer, offset / 0x200, 1, keyslot);
            if (errorcode != 0) return errorcode;
            if (count <= offset_fix) return 0;
            offset += offset_fix;
            buffer8 += offset_fix;
            count -= offset_fix;
        } // offset is now aligned and part of the data is written
        if (count >= 0x200) { // otherwise this is misaligned and will be handled below
            errorcode = WriteNandSectors(buffer8, offset / 0x200, count / 0x200, keyslot);
            if (errorcode != 0) return errorcode;
        }
        if (count % 0x200) { // handle misaligned count
            u32 count_fix = count % 0x200;
            errorcode = ReadNandSectors(l_buffer, (offset + count) / 0x200, 1, keyslot);
            if (errorcode != 0) return errorcode;
            memcpy(l_buffer, buffer8 + count - count_fix, count_fix);
            errorcode = WriteNandSectors((const u8*) l_buffer, (offset + count) / 0x200, 1, keyslot);
            if (errorcode != 0) return errorcode;
        }
        return errorcode;
    }
}

int ReadNandSectors(void* buffer, u32 sector, u32 count, u32 keyslot)
{
    if (!count) return 0; // <--- just to be safe
    int errorcode = sdmmc_nand_readsectors(sector, count, buffer);
    if (errorcode) return errorcode;
    if ((keyslot == 0x11) && (sector == 0x96)) CryptSector0x96(buffer, false);
    else if (keyslot < 0x40) CryptNand(buffer, sector, count, keyslot);
    
    return 0;
}

int WriteNandSectors(const void* buffer, u32 sector, u32 count, u32 keyslot)
{
    // buffer must not be changed, so this is a little complicated
    const u8* buffer8 = (u8*) buffer;
    for (u32 s = 0; s < count; s += (NAND_BUFFER_SIZE / 0x200)) {
        u32 pcount = min((NAND_BUFFER_SIZE/0x200), (count - s));
        memcpy(NAND_BUFFER, buffer8 + (s*0x200), pcount * 0x200);
        if ((keyslot == 0x11) && (sector == 0x96)) CryptSector0x96(NAND_BUFFER, true);
        else if (keyslot < 0x40) CryptNand(NAND_BUFFER, sector + s, pcount, keyslot);
        int errorcode = sdmmc_nand_writesectors(sector + s, pcount, NAND_BUFFER);
        if (errorcode) return errorcode;
    }
    
    return 0;
}

u32 CheckNandHeader(void* header)
{
    // TWL MBR check - ignored
    u8 header_dec[0x200];
    memcpy(header_dec, header, 0x200);
    CryptNand(header_dec, 0, 1, 0x03);
    if (memcmp(header_dec + 0x1BE, twl_mbr, sizeof(twl_mbr)) != 0)
        return 0; // header does not belong to console
    
    // header type check
    u8* header_enc = header;
    if (memcmp(header_enc + 0x100, nand_magic_n3ds, sizeof(nand_magic_n3ds) == 0) == 0)
        return (IS_O3DS) ? 0 : NAND_TYPE_N3DS;
    else if (memcmp(header_enc + 0x100, nand_magic_o3ds, sizeof(nand_magic_o3ds) == 0) == 0)
        return NAND_TYPE_O3DS;
    
    return 0;
}

u32 CheckNandType(void)
{
    if (ReadNandSectors(NAND_BUFFER, 0, 1, 0xFF) != 0)
        return 0;
    if (memcmp(NAND_BUFFER + 0x100, nand_magic_n3ds, 0x60) == 0) {
        return (IS_O3DS) ? 0 : NAND_TYPE_N3DS;
    } else if (memcmp(NAND_BUFFER + 0x100, nand_magic_o3ds, 0x60) == 0) {
        u8 magic[8] = {0xE9, 0x00, 0x00, 0x43, 0x54, 0x52, 0x20, 0x20};
        if (ReadNandSectors(NAND_BUFFER, 0x5CAE5, 1, 0x04) != 0)
            return 0;
        return ((IS_O3DS) || (memcmp(magic, NAND_BUFFER, 8) == 0)) ?
            NAND_TYPE_O3DS : NAND_TYPE_NO3DS;
    }
    
    return 0;
}

u64 GetNandSizeSectors(void)
{
    return getMMCDevice(0)->total_size;
}
