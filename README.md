ENBT - Extended Named Binary Tag format v1.1
============================================


### About
This multipurpose file format created for shortest way possible to store types and values in binary format.
Unlike NBT it does not require to start with a compound with generic name, and can contain multiple types in one file like list so this allows to just append data to file without need to rewrite it.
Also allows to use any endianness and correctly read data in any platform.



List of optimizations for file size:
* Type, endianness and size stored in one byte for each value.
* Instead of storing bool in separate byte, it stored in type byte instead endianness.
* Value `comp_integer` and the size of some arrays stored in custom format to save space. (first two bits encodes size of integer)
* For logs created `log_item` type that allows to store item that can be easily jumped without need to read all data in it. 
* For various values there three arrays types: `sarray` `darray` and `array` that allows to store values in different ways.
    * `sarray` - stores only numbers and uses in itself their type definition, like normal `integer` type.
    * `darray` - stores any type. 
    * `array` - stores only one type. (for bit type, 8 bits packed in one byte)

### Types
* none
* integer
* floating
* var_integer
* comp_integer
* uuid
* sarray
* compound
* darray
* array
* optional
* bit
* string
* log_item

### Note 

Requires C++ 20 to compile.
