#include <fat32.h>
#include <printk.h>
#include <virtio.h>
#include <string.h>
#include <mbr.h>
#include <mm.h>

struct fat32_bpb fat32_header;

struct fat32_volume fat32_volume;

uint8_t fat32_buf[VIRTIO_BLK_SECTOR_SIZE];
uint8_t fat32_table_buf[VIRTIO_BLK_SECTOR_SIZE];

uint64_t cluster_to_sector(uint64_t cluster) {
    return (cluster - 2) * fat32_volume.sec_per_cluster + fat32_volume.first_data_sec;
}

uint32_t next_cluster(uint64_t cluster) {
    uint64_t fat_offset = cluster * 4;
    uint64_t fat_sector = fat32_volume.first_fat_sec + fat_offset / VIRTIO_BLK_SECTOR_SIZE;
    virtio_blk_read_sector(fat_sector, fat32_table_buf);
    int index_in_sector = fat_offset % (VIRTIO_BLK_SECTOR_SIZE / sizeof(uint32_t));
    return *(uint32_t*)(fat32_table_buf + index_in_sector);
}

void fat32_init(uint64_t lba, uint64_t size) {
    virtio_blk_read_sector(lba, (void*)&fat32_header);
    fat32_volume.first_fat_sec = lba + fat32_header.rsvd_sec_cnt;
    fat32_volume.sec_per_cluster = fat32_header.sec_per_clus;
    fat32_volume.first_data_sec = fat32_volume.first_fat_sec + fat32_header.fat_sz32 * fat32_header.num_fats;
    fat32_volume.fat_sz = fat32_header.fat_sz32;

    virtio_blk_read_sector(fat32_volume.first_data_sec, fat32_buf); // Get the root directory
    struct fat32_dir_entry *dir_entry = (struct fat32_dir_entry *)fat32_buf;
}

int is_fat32(uint64_t lba) {
    virtio_blk_read_sector(lba, (void*)&fat32_header);
    if (fat32_header.boot_sector_signature != 0xaa55) {
        return 0;
    }
    return 1;
}

int next_slash(const char* path) {
    int i = 0;
    while (path[i] != '\0' && path[i] != '/') {
        i++;
    }
    if (path[i] == '\0') {
        return -1;
    }
    return i;
}

void to_upper_case(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= 'a' && str[i] <= 'z') {
            str[i] -= 32;
        }
    }
}

struct fat32_file fat32_open_file(const char *path) {
    struct fat32_file file;
    /* todo: open the file according to path */
    char filename[9];
    memset(filename, ' ', 9);
    filename[8] = '\0';

    if (strlen(path) - 7 > 8)
        memcpy(filename, path + 7, 8);
    else 
        memcpy(filename, path + 7, strlen(path) - 7);
    to_upper_case(filename);

    uint64_t sector = fat32_volume.first_data_sec;

    virtio_blk_read_sector(sector, fat32_buf);
    struct fat32_dir_entry *entry = (struct fat32_dir_entry *)fat32_buf;

    for (int entry_index = 0; entry_index < fat32_volume.sec_per_cluster * FAT32_ENTRY_PER_SECTOR; ++entry_index) {
        char name[8];
        memcpy(name, entry->name, 8);
        for (int k = 0; k < 9; ++k)     //to upper case
            if (name[k] <= 'z' && name[k] >= 'a') 
                name[k] += 'A' - 'a';
        if (memcmp(filename, name, 8) == 0) {
            file.cluster = ((uint32_t)entry->starthi << 16) | entry->startlow;
            file.dir.index = entry_index;
            file.dir.cluster = 2;
            return file;
        }
        ++entry;
    }
    printk("[S] file not found!\n");

    return file;
}

uint32_t get_file_size(struct file* file) {
    uint64_t sector = cluster_to_sector(file->fat32_file.dir.cluster) + file->fat32_file.dir.index / FAT32_ENTRY_PER_SECTOR;
    virtio_blk_read_sector(sector, fat32_table_buf);
    uint32_t index = file->fat32_file.dir.index % FAT32_ENTRY_PER_SECTOR;
    return ((struct fat32_dir_entry *)fat32_table_buf)[index].size;
}

int64_t fat32_lseek(struct file* file, int64_t offset, uint64_t whence) {
    if (whence == SEEK_SET) {
        file->cfo = offset;
    } else if (whence == SEEK_CUR) {
        file->cfo = file->cfo + offset;
    } else if (whence == SEEK_END) {
        /* Calculate file length */
        file->cfo = get_file_size(file) + offset;
    } else {
        printk("fat32_lseek: whence not implemented\n");
        while (1);
    }
    return file->cfo;
}

uint64_t fat32_table_sector_of_cluster(uint32_t cluster) {
    return fat32_volume.first_fat_sec + cluster / (VIRTIO_BLK_SECTOR_SIZE / sizeof(uint32_t));
}

int64_t fat32_extend_filesz(struct file* file, uint64_t new_size) {
    uint64_t sector = cluster_to_sector(file->fat32_file.dir.cluster) + file->fat32_file.dir.index / FAT32_ENTRY_PER_SECTOR;

    virtio_blk_read_sector(sector, fat32_table_buf);
    uint32_t index = file->fat32_file.dir.index % FAT32_ENTRY_PER_SECTOR;
    uint32_t original_file_len = ((struct fat32_dir_entry *)fat32_table_buf)[index].size;
    ((struct fat32_dir_entry *)fat32_table_buf)[index].size = new_size;

    virtio_blk_write_sector(sector, fat32_table_buf);

    uint32_t clusters_required = new_size / (fat32_volume.sec_per_cluster * VIRTIO_BLK_SECTOR_SIZE);
    uint32_t clusters_original = original_file_len / (fat32_volume.sec_per_cluster * VIRTIO_BLK_SECTOR_SIZE);
    uint32_t new_clusters = clusters_required - clusters_original;

    uint32_t cluster = file->fat32_file.cluster;
    while (1) {
        uint32_t next_cluster_number = next_cluster(cluster);
        if (next_cluster_number >= 0x0ffffff8) {
            break;
        }
        cluster = next_cluster_number;
    }

    for (int i = 0; i < new_clusters; i++) {
        uint32_t cluster_to_append;
        for (int j = 2; j < fat32_volume.fat_sz * VIRTIO_BLK_SECTOR_SIZE / sizeof(uint32_t); j++) {
            if (next_cluster(j) == 0) {
                cluster_to_append = j;
                break;
            }
        }
        uint64_t fat_sector = fat32_table_sector_of_cluster(cluster);
        virtio_blk_read_sector(fat_sector, fat32_table_buf);
        uint32_t index_in_sector = cluster * 4 % VIRTIO_BLK_SECTOR_SIZE;
        *(uint32_t*)(fat32_table_buf + index_in_sector) = cluster_to_append;
        virtio_blk_write_sector(fat_sector, fat32_table_buf);
        cluster = cluster_to_append;
    }

    uint64_t fat_sector = fat32_table_sector_of_cluster(cluster);
    virtio_blk_read_sector(fat_sector, fat32_table_buf);
    uint32_t index_in_sector = cluster * 4 % VIRTIO_BLK_SECTOR_SIZE;
    *(uint32_t*)(fat32_table_buf + index_in_sector) = 0x0fffffff;
    virtio_blk_write_sector(fat_sector, fat32_table_buf);

    return 0;
}

int64_t fat32_read(struct file* file, void* buf, uint64_t len) {
    uint32_t file_size = get_file_size(file);
    uint64_t read_len = 0; 
    while (read_len < len && file->cfo < file_size) {
        uint32_t cluster = file->fat32_file.cluster; 
        for (uint32_t clusteri = 0; clusteri < file->cfo / (fat32_volume.sec_per_cluster * VIRTIO_BLK_SECTOR_SIZE) && cluster < 0x0FFFFFF8; ++clusteri) {
            //cluster += 1;
            cluster = next_cluster(cluster);
        }
        uint64_t sector = cluster_to_sector(cluster);
        uint64_t offset_in_sector = file->cfo % VIRTIO_BLK_SECTOR_SIZE;
        uint64_t remain_readable_size = VIRTIO_BLK_SECTOR_SIZE - offset_in_sector;
        if (remain_readable_size > len - read_len)
            remain_readable_size = len - read_len;
        if (remain_readable_size > file_size - file->cfo)
            remain_readable_size = file_size - file->cfo;
        virtio_blk_read_sector(sector, fat32_buf);
        memset(buf, 0, len - read_len);
        memcpy(buf, fat32_buf + offset_in_sector, remain_readable_size);

        file->cfo += remain_readable_size;
        buf = (char *)buf + remain_readable_size;
        read_len += remain_readable_size;
        
    }
    return read_len;
    /* todo: read content to buf, and return read length */
}

int64_t fat32_write(struct file* file, const void* buf, uint64_t len) {
    uint64_t write_len = 0;
    while (len > 0) {
        uint32_t cluster = file->fat32_file.cluster;
        for (uint32_t clusteri = 0; clusteri < file->cfo / (fat32_volume.sec_per_cluster * VIRTIO_BLK_SECTOR_SIZE) && cluster < 0x0FFFFFF8; ++clusteri) {
            cluster = next_cluster(cluster);
        }
        uint64_t sector = cluster_to_sector(cluster);
        uint64_t offset_in_sector = file->cfo % VIRTIO_BLK_SECTOR_SIZE;
        uint64_t remain_writable_size = VIRTIO_BLK_SECTOR_SIZE - offset_in_sector;
        if (remain_writable_size > len) {
            remain_writable_size = len;
        }
        virtio_blk_read_sector(sector, fat32_buf);
        memcpy(fat32_buf + offset_in_sector, buf, remain_writable_size);
        virtio_blk_write_sector(sector, fat32_buf);

        file->cfo += remain_writable_size;
        buf += remain_writable_size;
        len -= remain_writable_size;
        write_len += remain_writable_size;
    }
    return write_len;
    /* todo: fat32_write */
}