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
const unsigned char DIRECTORY_TYPE = 1;
const unsigned char BINARY_TYPE = 0;
const unsigned int ENTRY_SIZE = 32;
const unsigned int DIR_SIZE_IN_BLOCKS = (N_ROOT_ENTRIES * ENTRY_SIZE) / (SECTORS_PER_BLOCK * BYTES_PER_SECTOR);

typedef struct boot_record{
    unsigned int sectors_per_block;
    unsigned int bytes_per_sector;
    unsigned int total_blocks;
    unsigned int bitmap_size_in_blocks;
    unsigned int n_root_entries;

    unsigned char padding[492];
}__attribute__((packed)) boot_record;

typedef struct dir_entry{
    unsigned int first_block;
    unsigned int file_size_in_bytes;
    unsigned char file_type;
    char file_name[23];
}__attribute__((packed)) dir_entry;

class BitMap{
public: 

    BitMap(const boot_record b_record)
        :b_record(b_record)
    {
        bit_map.resize(b_record.bitmap_size_in_blocks * b_record.sectors_per_block * b_record.bytes_per_sector);
        addressable_bits = b_record.bitmap_size_in_blocks * b_record.sectors_per_block * b_record.bytes_per_sector * 8;
    }

    BitMap(std::ifstream& disk)
    {
        boot_record temp_b_record;
        disk.seekg(0); 
        disk.read((char*)(&temp_b_record), 512);
        b_record = temp_b_record;

        bit_map.resize(b_record.bitmap_size_in_blocks * b_record.sectors_per_block * b_record.bytes_per_sector);
        addressable_bits = b_record.bitmap_size_in_blocks * b_record.sectors_per_block * b_record.bytes_per_sector * 8;

        loadBufferFromImage(disk);
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
        //std::cout << "settando o bit \n";
        //std::cout << "byte index == " << bit_map[byte_index] << std::endl;
        bit_map[byte_index] = bit_map[byte_index] | mask;
        //std::cout << "settei o bit\n";
    }

    size_t getAdressableBits(){
        return addressable_bits;
    }

    void writeBits(size_t first_bit, size_t size, unsigned char bit_to_write){
        //bit to write is expected to be either 00000001 or 00000000
        for (size_t i = first_bit; i < first_bit + size; i++){
            //std::cout << "iteração n " << i << std::endl;
            setBit(i, bit_to_write);
        }
    }
private:
    
    boot_record b_record;
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

    void loadBufferFromImage(std::ifstream& readable_file){
        size_t bitmap_total_bytes = b_record.bitmap_size_in_blocks * b_record.sectors_per_block * b_record.bytes_per_sector;
        //putting the head on the beggining of bitmpap
        readable_file.seekg(b_record.bytes_per_sector * b_record.sectors_per_block);
        for(size_t i = 0; i < bitmap_total_bytes; i++){
            bit_map[i] = readable_file.get();
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
    writable_file.seekp(b_record.sectors_per_block * b_record.bytes_per_sector);
    writable_file.write((const char*)&*bit_map_vector.begin(), (b_record.bitmap_size_in_blocks * b_record.sectors_per_block * b_record.bytes_per_sector));

    delete bit_map;
}

void writeRootDir(std::ifstream& readable_file, std::ofstream& writable_file, const boot_record& b_record){
            // written in portuguese just out of a habit

    //. and .. directories   
    dir_entry ponto, pontoponto;
    ponto.first_block = (1 + b_record.bitmap_size_in_blocks);
    ponto.file_size_in_bytes = 0;
    ponto.file_type = DIRECTORY_TYPE;
    strcpy(ponto.file_name, "ponto");

    pontoponto.first_block = (1 + b_record.bitmap_size_in_blocks);
    pontoponto.file_size_in_bytes = 0;
    pontoponto.file_type = DIRECTORY_TYPE;
    strcpy(pontoponto.file_name, "pontoponto");
    

    //setting the corresponding bits in the bitmap
    BitMap aux_bitmap(readable_file); 
    //std::cout << "chegou aqui1\n";
    //std::cout << "first bit = " << 1 + b_record.bitmap_size_in_blocks << std::endl;
    //std::cout << "last bit = " << DIR_SIZE_IN_BLOCKS << std::endl;
    aux_bitmap.writeBits(1 + b_record.bitmap_size_in_blocks, DIR_SIZE_IN_BLOCKS, 1);
    //std::cout << "chegou aqui2\n";
    //writing...
    writable_file.seekp((1 + b_record.bitmap_size_in_blocks) * b_record.sectors_per_block * b_record.bytes_per_sector);

    // making everything == 0 just like my energy rn
    char* zero_vector_block = (char*)calloc(b_record.sectors_per_block * b_record.bytes_per_sector, sizeof(char));
    for(size_t i = 0; i < b_record.total_blocks - (1 + b_record.bitmap_size_in_blocks); i++){
        writable_file.write(zero_vector_block, b_record.sectors_per_block * b_record.bytes_per_sector);
    }
    
    writable_file.write(0, getDiskSize(readable_file) - ((1 + b_record.bitmap_size_in_blocks) * b_record.sectors_per_block * b_record.bytes_per_sector));
    writable_file.seekp((1 + b_record.bitmap_size_in_blocks) * b_record.sectors_per_block * b_record.bytes_per_sector);
    writable_file.write((const char*)&ponto, ENTRY_SIZE);
    writable_file.write((const char*)&pontoponto, ENTRY_SIZE);

    writable_file.seekp(b_record.bytes_per_sector * b_record.sectors_per_block);
    writable_file.write((const char*)&*aux_bitmap.getBuffer().begin(), b_record.bitmap_size_in_blocks * b_record.sectors_per_block * b_record.bytes_per_sector);

    
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
    writeRootDir(readable_file, writable_file, b_record);

    readable_file.close();
    writable_file.close();

    std::cout << "Done :D\n";
    return 0;
}