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

class BitMap{
public: 

    BitMap(const boot_record b_record)
        :b_record(b_record)
    {
        bit_map.resize(b_record.bitmap_size_in_blocks * b_record.sectors_per_block * b_record.bytes_per_sector);
        addressable_bits = b_record.bitmap_size_in_blocks * b_record.sectors_per_block * b_record.bytes_per_sector * 8;
    }

    void format(){
        fillReservedBlocks();
        fillNonReachableBlocks();
        defaultUsableToZero();
    }

    std::vector<unsigned char> getBuffer(){
        return bit_map;
    }

    unsigned char getBit(size_t bit_index){
        
        size_t byte_index = bit_index/8;
        unsigned char offset = bit_index % 8;
        unsigned char mask = 0b10000000 >> offset;

        return (bit_map[byte_index] & mask) >> (7 - offset);

    }

    void setBit(size_t bit_index, unsigned char value){

        // value is expected to be either 00000001 or 00000000
        value = value << 7;
        size_t byte_index = bit_index/8;
        unsigned char offset = bit_index % 8;
        unsigned char mask = value >> offset;

        bit_map[byte_index] = bit_map[byte_index] | mask;
    }

private:
    
    const boot_record b_record;
    size_t addressable_bits; 
    std::vector<unsigned char> bit_map; 

    void fillReservedBlocks(){

        // 1 because of the boot record block
        size_t reserved_blocks = 1 + b_record.bitmap_size_in_blocks;
        for(size_t i = 0; i < reserved_blocks; i++){
            this->setBit(i, 1);
        }
    }

    void fillNonReachableBlocks(){
        
        size_t non_reachable_blocks = addressable_bits - b_record.total_blocks;
        for(size_t i = addressable_bits - 1; i >= b_record.total_blocks; i--){
            this->setBit(i,1);
        }
    }

    void defaultUsableToZero(){

        size_t reserved_blocks = 1 + b_record.bitmap_size_in_blocks;
        for(size_t i = reserved_blocks; i < b_record.total_blocks; i++){
            this->setBit(i,0);
        }
        
    }

};

unsigned int getDiskSize (std::ifstream& readable_file){

    readable_file.seekg(0, std::ios::end);
    auto disk_size = readable_file.tellg();
    return disk_size;

}


boot_record writeBootRecord(std::ifstream& readable_file, std::ofstream& writable_file){
    boot_record b_record;

    auto disk_size = getDiskSize(readable_file);

    b_record.sectors_per_block = SECTORS_PER_BLOCK;
    b_record.bytes_per_sector = BYTES_PER_SECTOR;
    b_record.total_blocks = disk_size / (b_record.bytes_per_sector * b_record.sectors_per_block);
    b_record.bitmap_size_in_blocks = ceil((b_record.total_blocks / 8) / (b_record.bytes_per_sector * b_record.sectors_per_block));
    b_record.n_root_entries = N_ROOT_ENTRIES;

    writable_file.write((const char*)&b_record, 512);
    
    return b_record;
}

void writeBitMap(std::ifstream& readable_file, std::ofstream& writable_file, const boot_record& b_record){
    BitMap* bit_map = new BitMap(b_record);
    bit_map->format();
    std::vector<unsigned char> bit_map_vector = bit_map->getBuffer();
    delete bit_map;
    
}

int main(int argc, const char** argv){

    const std::string image_name(argv[1]);

    std::cout << "Initializing formatting process\n...\n"; 

    std::ifstream readable_file;
    std::ofstream writable_file;
    writable_file.open(image_name, std::ios::in | std::ios::out);
    readable_file.open(image_name, std::ios::binary | std::ios::ate);
    
    boot_record b_record = writeBootRecord(readable_file, writable_file);
    writeBitMap(readable_file, writable_file, b_record);

    readable_file.close();
    writable_file.close();

    std::cout << "Done :D\n";
    return 0;
}