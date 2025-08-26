
[![Language](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/) 
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)


# ENBT - Extended Named Binary Tag format v1.1

## About

ENBT is a multipurpose file format designed for the most compact storage of typed and valued data in a binary format. Unlike the standard Named Binary Tag (NBT) format, ENBT does not require a file to begin with a generically named compound tag. This allows for greater flexibility, such as appending data to a file without needing to rewrite the entire file. ENBT is also platform-independent, enabling data to be read correctly across systems with different endianness. 

## Key Features

  * **Efficient Data Storage**: ENBT utilizes several optimizations to minimize file size:

      * A single byte stores the type, endianness, and size information for each value. 
      * Boolean values are stored within the type byte, eliminating the need for a separate byte. 
      * A custom variable-length format is used for `comp_integer` and certain array sizes, where the first two bits encode the integer's size, saving space. 
      * The `log_item` type is designed for logs, allowing for efficient skipping over items without reading their entire content.
      * For bit arrays, eight boolean values are packed into a single byte. 

  * **Flexible Structure**: Data can be appended to a file without rewriting existing content, as there is no requirement for a single root compound. 

  * **Cross-Platform Compatibility**: The format is designed to handle endianness correctly, ensuring that data can be read on any platform. 

  * **Rich Data Types**: ENBT supports a wide variety of data types to accommodate most use cases.

  * **Low-Level I/O Control**: The library provides granular control over stream operations for reading and writing data, including lightweight stream wrappers (`value_read_stream` and `value_write_stream`) that minimize memory allocations.

  * **Advanced Indexing**: The library offers functions to efficiently seek to specific values within a stream, for instance by key in a compound or by index in an array, without reading all preceding data. 

  * **Text-Based Representation (SENBT)**: ENBT provides an optional human-readable text format, SENBT (String ENBT), which can be parsed into and serialized from the binary ENBT format for easier debugging and editing. 

## Data Types

ENBT supports the following data types:

  * **none**: Represents a null value.
  * **integer**: A standard integer type.
  * **floating**: A floating-point number.
  * **var\_integer**: A variable-length integer.
  * **comp\_integer**: A custom-compressed integer for space efficiency. 
  * **uuid**: A universally unique identifier.
  * **string**: A sequence of characters.
  * **bit**: A boolean value, stored efficiently. 
  * **optional**: Represents a value that may or may not be present.
  * **log\_item**: A special type designed for logs, allowing easy skipping over items without parsing their full content.
  * **Arrays**:
      * **sarray**: A simple array that stores only numeric types, defining their type once for the entire array. 
      * **darray**: A dynamic array that can hold elements of any type. 
      * **array**: An array where all elements are of the same, single type. 
  * **compound**: A key-value map, similar to a dictionary or object, where keys are strings.

## Library Structure

The repository consists of the following key header files:

  * `enbt.hpp`: (Not provided in the source) This file is inferred to contain the core definitions of ENBT types and values.
  * `io.hpp`: Contains the core I/O functionalities for reading and writing ENBT data from/to C++ streams (`std::istream`, `std::ostream`).
  * `io_tools.hpp`: Provides a high-level serialization framework for mapping C++ standard library containers and user-defined types to ENBT.
  * `senbt.hpp`: Includes the parser and serializer for the SENBT text-based format. 
  * `LICENSE`: Contains the MIT License under which the software is distributed. 

## Usage

### Low-Level I/O (`io.hpp`)

The `io.hpp` header provides a set of functions and classes for direct interaction with ENBT data streams.

#### Writing Data

You can write individual values or build complex structures using the `value_write_stream` class, which offers a lightweight way to write to an output stream without allocations. 

**Example: Writing a compound**

```cpp
#include "enbt/io.hpp"
#include <fstream>

void write_data() {
    std::ofstream out_stream("data.enbt", std::ios::binary);
    enbt::io_helper::value_write_stream writer(out_stream);

    auto compound_writer = writer.write_compound();
    compound_writer.write("name", "example");
    compound_writer.write("value", 123);
}
```

#### Reading Data

The `value_read_stream` class allows for efficient reading from an input stream without unnecessary allocations.

**Example: Reading a compound**

```cpp
#include "enbt/io.hpp"
#include <fstream>
#include <iostream>

void read_data() {
    std::ifstream in_stream("data.enbt", std::ios::binary);
    enbt::io_helper::value_read_stream reader(in_stream);

    auto compound_reader = reader.read_compound(); 
    compound_reader.iterate([](std::string& name, enbt::io_helper::value_read_stream& item_stream) {
        if (name == "value") {
            enbt::value val = item_stream.read();
            std::cout << name << ": " << std::get<int64_t>(val) << std::endl;
        } else {
             std::cout << name << ": " << std::get<std::string>(item_stream.read()) << std::endl;
        }
    });
}
```

### High-Level Serialization (`io_tools.hpp`)

The `io_tools.hpp` header simplifies the process of converting standard C++ objects to and from ENBT. It provides `serialization` specializations for common types.

Supported types include:

  * `std::unique_ptr<T>` 
  * `std::shared_ptr<T>` 
  * `std::pair<T1, T2>` 
  * `std::vector<T>` 
  * `std::unordered_map<std::string, T>` 
  * `std::array<T, N>` 
  * C-style arrays `T[N]` 

**Example: Serializing a `std::vector`**

```cpp
#include "enbt/io.hpp"
#include "enbt/io_tools.hpp"
#include <fstream>
#include <vector>

void serialize_vector() {
    std::vector<int32_t> my_vector = {10, 20, 30, 40, 50};
    std::ofstream out_stream("vector.enbt", std::ios::binary);
    enbt::io_helper::value_write_stream writer(out_stream);

    // The serialization tool will automatically choose the efficient 'sarray'
    enbt::io_helper::serialization_write(my_vector, writer); 
}

std::vector<int32_t> deserialize_vector() {
    std::ifstream in_stream("vector.enbt", std::ios::binary);
    enbt::io_helper::value_read_stream reader(in_stream);

    std::vector<int32_t> my_vector;
    enbt::io_helper::serialization_read(my_vector, reader); 
    return my_vector;
}
```

### Text Format (SENBT)

For configuration, logging, or debugging, the SENBT format provides a human-readable representation of ENBT data. 

**SENBT Syntax Example:**

```
{
    "name": "My Awesome Object",
    "version": (1)i, // 'i' specifies integer type
    "enabled": true,
    "data": s'i'[1, 2, 3, 4, 5], // s'i' specifies a simple array of integers
    "children": [
        {"id": "child1"},
        {"id": "child2"}
    ]
}
```


You can parse this text into an `enbt::value` or serialize an `enbt::value` back into an SENBT string.

**Example: Parsing SENBT**

```cpp
#include "senbt.hpp"
#include <string_view>
#include <iostream>

void parse_senbt() {
    std::string_view senbt_string = R"({ "message": "Hello, SENBT!" })";
    enbt::value data = senbt::parse(senbt_string);

    std::string serialized_string = senbt::serialize(data);
    std::cout << serialized_string << std::endl;
}
```

## SAST Tools

[PVS-Studio](https://pvs-studio.com/en/pvs-studio/?utm_source=website&utm_medium=github&utm_campaign=open_source) - static analyzer for C, C++, C#, and Java code.