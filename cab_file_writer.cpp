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
const size_t OUT_OF_FREE_SPACE = 0;

typedef struct boot_record
{
    unsigned int sectors_per_block;
    unsigned int bytes_per_sector;
    unsigned int total_blocks;
    unsigned int bitmap_size_in_blocks;
    unsigned int n_root_entries;

    unsigned char padding[492];
} __attribute__((packed)) boot_record;

typedef struct dir_entry
{
    unsigned int first_block;
    unsigned int file_size_in_bytes;
    unsigned char file_type;
    char file_name[23];
} __attribute__((packed)) dir_entry;

class BitMap
{
public:
    BitMap(const boot_record b_record)
        : b_record(b_record)
    {
        bit_map.resize(b_record.bitmap_size_in_blocks * b_record.sectors_per_block * b_record.bytes_per_sector);
        addressable_bits = b_record.bitmap_size_in_blocks * b_record.sectors_per_block * b_record.bytes_per_sector * 8;
    }

    BitMap(std::ifstream &disk)
    {
        boot_record temp_b_record;
        disk.seekg(0);
        disk.read((char *)(&temp_b_record), 512);
        b_record = temp_b_record;

        bit_map.resize(b_record.bitmap_size_in_blocks * b_record.sectors_per_block * b_record.bytes_per_sector);
        addressable_bits = b_record.bitmap_size_in_blocks * b_record.sectors_per_block * b_record.bytes_per_sector * 8;

        loadBufferFromImage(disk);
    }

    void format()
    {
        fillReservedBlocks();
        fillNonReachableBlocks();
        defaultUsableToZero();
    }

    std::vector<unsigned char> getBuffer()
    {
        return bit_map;
    }

    unsigned char getBit(size_t bit_index)
    {

        size_t byte_index = bit_index / 8;
        unsigned char offset = bit_index % 8;
        unsigned char mask = 0b10000000 >> offset;

        return (bit_map[byte_index] & mask) >> (7 - offset);
    }

    void setBit(size_t bit_index, unsigned char value)
    {

        // value is expected to be either 00000001 or 00000000
        value = value << 7;
        size_t byte_index = bit_index / 8;
        unsigned char offset = bit_index % 8;
        unsigned char mask = value >> offset;
        // std::cout << "settando o bit \n";
        // std::cout << "byte index == " << bit_map[byte_index] << std::endl;
        bit_map[byte_index] = bit_map[byte_index] | mask;
        // std::cout << "settei o bit\n";
    }

    size_t getAdressableBits()
    {
        return addressable_bits;
    }

    void writeBits(size_t first_bit, size_t size, unsigned char bit_to_write)
    {
        // bit to write is expected to be either 00000001 or 00000000
        for (size_t i = first_bit; i < first_bit + size; i++)
        {
            // std::cout << "iteração n " << i << std::endl;
            setBit(i, bit_to_write);
        }
    }

    size_t getFirstBlock(size_t block_amount)
    {
        size_t address_first_block = OUT_OF_FREE_SPACE;
        size_t first_current = 0;
        size_t contiguous_blocks_found = 0;
        for (size_t i = 0; i < addressable_bits; i++)
        {
            unsigned char bit = getBit(i);

            if (first_current == 0)
            {
                first_current = i;
            }

            if (bit == 0)
            {
                contiguous_blocks_found++;
            }
            else
            {
                contiguous_blocks_found = 0;
                first_current = 0;
            }

            if (contiguous_blocks_found == block_amount)
            {
                address_first_block = first_current;
                break;
            }
        }

        return address_first_block;
    }

private:
    boot_record b_record;
    size_t addressable_bits;
    std::vector<unsigned char> bit_map;

    void fillReservedBlocks()
    {

        // 1 because of the boot record block
        size_t reserved_blocks = 1 + b_record.bitmap_size_in_blocks;
        for (size_t i = 0; i < reserved_blocks; i++)
        {
            this->setBit(i, 1);
        }
    }

    void fillNonReachableBlocks()
    {

        size_t non_reachable_blocks = addressable_bits - b_record.total_blocks;
        for (size_t i = addressable_bits - 1; i >= b_record.total_blocks; i--)
        {
            this->setBit(i, 1);
        }
    }

    void defaultUsableToZero()
    {
        size_t reserved_blocks = 1 + b_record.bitmap_size_in_blocks;
        for (size_t i = reserved_blocks; i < b_record.total_blocks; i++)
        {
            this->setBit(i, 0);
        }
    }

    void loadBufferFromImage(std::ifstream &readable_file)
    {
        size_t bitmap_total_bytes = b_record.bitmap_size_in_blocks * b_record.sectors_per_block * b_record.bytes_per_sector;
        // putting the head on the beggining of bitmpap
        readable_file.seekg(b_record.bytes_per_sector * b_record.sectors_per_block);
        for (size_t i = 0; i < bitmap_total_bytes; i++)
        {
            bit_map[i] = readable_file.get();
        }
    }
};

unsigned int getDiskSize(std::ifstream &readable_file)
{

    readable_file.seekg(0, std::ios::end);
    auto disk_size = readable_file.tellg();
    return disk_size;
}

dir_entry* loadRootDir(std::ifstream& readable_file, boot_record b_record){

    dir_entry* current_dir = (dir_entry*)malloc(N_ROOT_ENTRIES * sizeof(dir_entry));
    readable_file.seekg((1 + b_record.bitmap_size_in_blocks) * b_record.bytes_per_sector * b_record.sectors_per_block);
    readable_file.read((char*)current_dir, N_ROOT_ENTRIES);
    return current_dir;
}

void writeToCAB(std::ifstream &readable_file, std::ofstream &writable_file, boot_record &b_record, std::string file_name)
{
    std::ifstream file_to_write(file_name, std::ios::binary | std::ios::ate);
    BitMap bmap(readable_file);
    char buf = ' ';
    // obtaining file's size in order to calculate how many blocks it needs
    unsigned int file_size = getDiskSize(file_to_write);
    std::cout << "file size == " << file_size << std::endl;

    unsigned int blocks_for_file = ceil(((double)file_size / (b_record.sectors_per_block * b_record.bytes_per_sector)));
    size_t first_block = bmap.getFirstBlock(blocks_for_file);
    std::cout << "blocks_for_file == " << blocks_for_file << std::endl;
    std::cout << "first block == " << first_block << std::endl;

    //if there is a big enough contiguous block
    if(first_block){

        // first write file's binary entry in the directory required
        dir_entry file_entry;
        file_entry.first_block = first_block;
        file_entry.file_size_in_bytes = file_size;
        file_entry.file_type = 0;
        strcpy(file_entry.file_name, &*file_name.begin());

        // getting all the dir entries in an array
        dir_entry* current_dir = loadRootDir(readable_file, b_record);
        
        //finding an empty root dir entry
        size_t available_entry_index;
        for(size_t i = 0; i < b_record.n_root_entries; i++){
            if(current_dir[i].file_type == 0xff || current_dir[i].first_block == 0x0){
                available_entry_index = i;
                break;
            }
        }
        std::cout << "available_entry_index = " << available_entry_index << std::endl;

        //writing dir_entry in disk
        writable_file.seekp(0);
        writable_file.seekp((ENTRY_SIZE * available_entry_index) + (1 + b_record.bitmap_size_in_blocks) * b_record.bytes_per_sector * b_record.sectors_per_block);
        writable_file.write((const char*)&file_entry, ENTRY_SIZE);

        //write file
        
        writable_file.seekp(file_entry.first_block * b_record.sectors_per_block * b_record.bytes_per_sector);
        //for now, it is loading the entire file on memory instead of buffering to write on the filesystem
        file_to_write.seekg(0);
        std::string file_buffer_string((std::istreambuf_iterator<char>(file_to_write)), std::istreambuf_iterator<char>());
        std::cout << "file buffer = \n" << file_buffer_string.c_str() << std::endl;
        writable_file.write((const char*)&*file_buffer_string.begin(), file_size);

        bmap.writeBits(first_block, blocks_for_file, 1);

        writable_file.seekp(b_record.bytes_per_sector * b_record.sectors_per_block);
        writable_file.write((const char*)&*(bmap.getBuffer().begin()), b_record.bitmap_size_in_blocks * b_record.sectors_per_block * b_record.bytes_per_sector);
        free((void*)current_dir);
    }
}

boot_record readBootRecord(std::ifstream &readable_file){
    boot_record b_record;
    readable_file.seekg(0);
    readable_file.read((char*)&b_record, sizeof(boot_record));

    return b_record;
}

int main(int argc, const char **argv)
{
    std::string file_name_image = argv[1];
    std::ifstream readable_file(file_name_image, std::ios::binary | std::ios::ate);
    std::ofstream writable_file(file_name_image, std::ios::in | std::ios::out);
    boot_record b_record;
    std::string file_name_to_write = argv[2];

    //it must be either 0 for generic binary files or 1 to directorie files
    b_record = readBootRecord(readable_file);

    writeToCAB(readable_file, writable_file, b_record, file_name_to_write);

    readable_file.close();
    writable_file.close();

    return 0;
}