#ifndef _FAT_CACHE_H_
#define _FAT_CACHE_H_

typedef struct fat_sector_cache
{
	u32  sector;
	u32  counter;/* read counter */
	u8   flag;
	u8   *cache;
}FAT_SECTOR_CACHE;

#define FAT_BUF_P(buf) (void*)((buf)->cache)

int fat_read(DRIVE_INFO *drv, u32 start_sector, int sector_num, 
	FAT_SECTOR_CACHE **p);
int fat_write(DRIVE_INFO *drv, int start_sector, int sector_num, FAT_SECTOR_CACHE *fat_buf);
int fat_write_submit(DRIVE_INFO *drv, int err);
FAT_SECTOR_CACHE *fat_buf_get(DRIVE_INFO *drv);
void fat_cache_destroy(DRIVE_INFO *drv);

#endif/* _FAT_CACHE_H_ */
