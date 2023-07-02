#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <cstring>
#include <cmath>

const unsigned int SECTORS_PER_BLOCK = 1;
const unsigned int BYTES_PER_SECTOR = 512;
const unsigned int N_ROOT_ENTRIES = 1024;

typedef struct boot_record{
    unsigned int sectors_per_block;
    unsigned int bytes_per_sector;
    unsigned int total_blocks;
    unsigned int bitmap_size_in_blocks;
    unsigned int n_root_entries;

    unsigned char padding[492];
}__attribute__((packed)) boot_record;

void writeBootRecord(std::ifstream& readable_file, std::ofstream& writable_file){
    boot_record b_record;

    // obtendo tamanho do disco
    readable_file.seekg(0, std::ios::end);
    auto disk_size = readable_file.tellg();
    std::cout << disk_size << std::endl;

    b_record.sectors_per_block = SECTORS_PER_BLOCK;
    b_record.bytes_per_sector = BYTES_PER_SECTOR;
    b_record.total_blocks = disk_size / (b_record.bytes_per_sector * b_record.sectors_per_block);
    b_record.bitmap_size_in_blocks = ceil((b_record.total_blocks / 8) / (b_record.bytes_per_sector * b_record.sectors_per_block));
    b_record.n_root_entries = N_ROOT_ENTRIES;

    unsigned char b_record_buffer[512];
    memcpy(b_record_buffer, &b_record, 512);
    writable_file.write((const char*)(b_record_buffer), 512);
}

int main(int argc, const char** argv){

    const std::string image_name(argv[1]);

    std::ifstream readable_file;
    std::ofstream writable_file;
    writable_file.open(image_name, std::ios::in | std::ios::out);
    readable_file.open(image_name, std::ios::binary | std::ios::ate);
    // if (!readable_file) {
    //     std::cout << "Erro ao abrir o arquivo.\n";
    //     return 1;
    // }
    //readable_file.seekg(0, std::ios::end);
    //unsigned int fsize = readable_file.tellg();
    //std::cout << fsize << std::endl;
    
    writeBootRecord(readable_file, writable_file);

    readable_file.close();
    writable_file.close();
    return 0;
}