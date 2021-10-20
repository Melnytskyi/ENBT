#include <iostream>
#include <fstream>
#include <ms_core/enbt.hpp>
int main()
{
    ENBT core = ENBT::Type_ID(ENBT::Type::compound, ENBT::Type_ID::LenType::Tiny,false);
    ENBT darr = ENBT::Type::darray;
    darr.resize(10);
    std::cout << "DArray start init" << std::endl;
    for (int i = 0; i < 10; i++) {
        ENBT int_v = ENBT::Type_ID(ENBT::Type::integer, ENBT::Type_ID::LenType::Default, std::endian(rand()),false);
        uint32_t readat = rand();
        int_v = readat;
        darr[i] = int_v;
    }
    std::cout << "DArray end init" << std::endl;
    
    ENBT arr = ENBT::Type::array;
    arr.resize(10);
    std::cout << "Array start init" << std::endl;
    for (int i = 0; i < 10; i++) {
        ENBT int_v = ENBT::Type_ID(ENBT::Type::var_integer, ENBT::Type_ID::LenType::Default, std::endian::big, false);
        uint32_t readat = rand();
        int_v = readat;
        arr[i] = int_v;
    }
    std::cout << "Array end init" << std::endl;
    int temp[] = { 123456, 123456, 123456, 123456, 123456 };
    ENBT sarr = ENBT(temp, 5, std::endian::big);
    
    ENBT i = 123456789101112;
    ENBT f = 12345678.123456789;
    ENBT vi(123456784343, true);
    core["darr"] = darr;
    core["arr"] = arr;
    core["sarr"] = sarr;
    core["int"] = i;
    core["float"] = f;
    core["var"] = vi;
    
    
    std::fstream fs;
    fs.open("test.enbt", std::fstream::out | std::fstream::binary);
    ENBTHelper::InitalizeVersion(fs);
    ENBTHelper::WriteToken(fs, core);
    fs.close();

    fs.open("test.enbt", std::ios::in);
    fs >> std::noskipws;
    ENBTHelper::CheckVersion(fs);
    ENBT to_test = ENBTHelper::ReadToken(fs);
    fs.close();
    
    for (int i = 0; i < 10; i++)
        std::cout << (uint32_t)to_test["darr"][i] <<std::endl;
    std::cout << std::endl;
    for (int i = 0; i < 10; i++)
        std::cout << (uint32_t)to_test["arr"][i] << std::endl;
    std::cout << std::endl;
    for (int i = 0; i < 5; i++)
        std::cout << (uint32_t)to_test["sarr"].getIndex(i) << std::endl;
    std::cout << std::endl;
    std::cout << (int64_t)to_test["int"] << std::endl;
    std::cout << (double)to_test["float"] << std::endl;
    std::cout << (int64_t)to_test["var"] << std::endl;
}