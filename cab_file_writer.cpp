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

void writeToCAB(std::ifstream &readable_file, std::ofstream &writable_file, boot_record &b_record, std::string file_name)
{
    std::ifstream file_to_write(file_name, std::ios::binary | std::ios::ate);
    BitMap bmap(readable_file);
    char buf = ' ';
    // obtaining file's size in order to calculate how many blocks it needs
    unsigned int file_size = getDiskSize(file_to_write);
    unsigned int blocks_for_file = ceil((file_size / (b_record.sectors_per_block * b_record.bytes_per_sector)));
    size_t first_block = bmap.getFirstBlock(blocks_for_file);
    if(first_block){
        // first write file's entry in the directory required
        dir_entry file_entry;
        file_entry.first_block = first_block;
        file_entry.file_size_in_bytes = file_size;
        file_entry.file_type = 0;
        strcpy(file_entry.file_name, &*file_name.begin());

        // getting all the dir entries in an array
        char* current_dir = (char*)malloc(N_ROOT_ENTRIES * ENTRY_SIZE * sizeof(char));
        readable_file.seekg((1 + b_record.bitmap_size_in_blocks) * b_record.bytes_per_sector * b_record.sectors_per_block);
        readable_file.read(current_dir, N_ROOT_ENTRIES * ENTRY_SIZE);
        //for(int i = 0; i < )
        free((void*)current_dir);

    }



}

int main(int argc, const char **argv)
{
    std::string file_name_image = argv[1];
    std::ifstream image_file(file_name_image);
    BitMap bmap(image_file);

    size_t first_block = bmap.getFirstBlock(4);
    std::cout << "first free block to contiguous available space = " << first_block << std::endl;
    bmap.setBit(77, 0);
    first_block = bmap.getFirstBlock(4);
    std::cout << "first free block to contiguous available space after change = " << first_block << std::endl;

    //std::ifstream file_to_write(argv[2]);

    return 0;
}