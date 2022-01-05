# Saleae Controller Area Network (CAN) Analyzer

Saleae Controller Area Network (CAN) Analyzer

## Getting Started

### MacOS

Dependencies:
- XCode with command line tools
- CMake 3.13+

Installing command line tools after XCode is installed:
```
xcode-select --install
```

Then open XCode, open Preferences from the main menu, go to locations, and select the only option under 'Command line tools'.

Installing CMake on MacOS:

1. Download the binary distribution for MacOS, `cmake-*-Darwin-x86_64.dmg`
2. Install the usual way by dragging into applications.
3. Open a terminal and run the following:
```
/Applications/CMake.app/Contents/bin/cmake-gui --install
```
*Note: Errors may occur if older versions of CMake are installed.*

Building the analyzer:
```
mkdir build
cd build
cmake ..
cmake --build .
```

### Ubuntu 16.04

Dependencies:
- CMake 3.13+
- gcc 4.8+

Misc dependencies:

```
sudo apt-get install build-essential
```

Building the analyzer:
```
mkdir build
cd build
cmake ..
cmake --build .
```

### Windows

Dependencies:
- Visual Studio 2015 Update 3
- CMake 3.13+

**Visual Studio 2015**

*Note - newer versions of Visual Studio should be fine.*

Setup options:
- Programming Languages > Visual C++ > select all sub-components.

Note - if CMake has any problems with the MSVC compiler, it's likely a component is missing.

**CMake**

Download and install the latest CMake release here.
https://cmake.org/download/

Building the analyzer:
```
mkdir build
cd build -A x64
cmake ..
```

Then, open the newly created solution file located here: `build\can_analyzer.sln`


## Output Frame Format
  
### Frame Type: `"identifier_field"`

| Property | Type | Description |
| :--- | :--- | :--- |
| `identifier` | int | Identifier, either 11 bit or 29 bit |
| `extended` | bool | (optional) Indicates that this identifier is a 29 bit extended identifier. This key is not present on regular 11 bit identifiers |
| `remote_frame` | bool | (optional) Present and true for remote frames |

### Frame Type: `"control_field"`

| Property | Type | Description |
| :--- | :--- | :--- |
| `num_data_bytes` | int | Number of data bytes in the transaction |

### Frame Type: `"data_field"`

| Property | Type | Description |
| :--- | :--- | :--- |
| `data` | int | The byte |

### Frame Type: `"crc_field"`

| Property | Type | Description |
| :--- | :--- | :--- |
| `crc` | int | 16 bit CRC value |

### Frame Type: `"ack_field"`

| Property | Type | Description |
| :--- | :--- | :--- |
| `ack` | bool | True when an ACK was present |

### Frame Type: `"can_error"`

| Property | Type | Description |
| :--- | :--- | :--- |


Invalid CAN data was encountered

