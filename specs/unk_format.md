# .unk Binary Format Specification

**Unknown OS Native Executable Format**
Author: byrex <byrex.me@proton.me>
Version: 1.0

---

## Overview

The `.unk` format is the native executable format for Unknown OS. It is designed for:
- Fast loading and execution
- Position-independent code support
- Built-in dependency resolution
- Security features (signing, capabilities)
- Cross-architecture support (x86_64, ARM64, RISC-V)

---

## File Structure

```
┌─────────────────────────────────────┐
│         UNK Header (128 bytes)      │
├─────────────────────────────────────┤
│       Program Headers (variable)    │
├─────────────────────────────────────┤
│       Section Headers (variable)    │
├─────────────────────────────────────┤
│         Program Data                │
│          (code, data, etc.)         │
├─────────────────────────────────────┤
│         String Table                │
├─────────────────────────────────────┤
│         Symbol Table                │
├─────────────────────────────────────┤
│         Relocation Table            │
├─────────────────────────────────────┤
│         Signature (optional)        │
└─────────────────────────────────────┘
```

---

## UNK Header (128 bytes)

```c
struct unk_header {
    uint8_t  magic[8];          // "UNKEXEC\0"
    uint8_t  version;           // Format version (1)
    uint8_t  arch;              // Architecture (see below)
    uint16_t flags;             // File flags
    uint64_t entry_point;       // Virtual address of entry point
    uint64_t ph_offset;         // Offset to program headers
    uint64_t sh_offset;         // Offset to section headers
    uint64_t str_offset;        // Offset to string table
    uint64_t sym_offset;        // Offset to symbol table
    uint64_t rel_offset;        // Offset to relocation table
    uint64_t sig_offset;        // Offset to signature (0 if none)
    uint32_t ph_count;          // Number of program headers
    uint32_t sh_count;          // Number of section headers
    uint32_t str_size;          // String table size
    uint32_t sym_count;         // Number of symbols
    uint32_t rel_count;         // Number of relocations
    uint32_t sig_size;          // Signature size
    uint32_t checksum;          // CRC32 of file (excluding this field)
    uint8_t  reserved[24];      // Reserved for future use
};
```

### Architecture Values
```c
#define UNK_ARCH_X86_64   1
#define UNK_ARCH_AARCH64  2
#define UNK_ARCH_RISCV64  3
```

### Flags
```c
#define UNK_FLAG_PIC           (1 << 0)  // Position-independent code
#define UNK_FLAG_STATIC        (1 << 1)  // Statically linked
#define UNK_FLAG_SIGNED        (1 << 2)  // Has valid signature
#define UNK_FLAG_DEBUG         (1 << 3)  // Contains debug info
#define UNK_FLAG_COMPRESSED    (1 << 4)  // Sections compressed (LZ4)
#define UNK_FLAG_CAPABILITIES  (1 << 5)  // Has capability table
```

---

## Program Headers

```c
struct unk_phdr {
    uint32_t type;            // Segment type
    uint32_t flags;           // Segment flags
    uint64_t offset;          // File offset
    uint64_t vaddr;           // Virtual address
    uint64_t paddr;           // Physical address (if applicable)
    uint64_t filesz;          // Size in file
    uint64_t memsz;           // Size in memory
    uint64_t align;           // Alignment requirement
};
```

### Segment Types
```c
#define UNK_PT_NULL      0  // Unused
#define UNK_PT_LOAD      1  // Loadable segment
#define UNK_PT_DYNAMIC   2  // Dynamic linking info
#define UNK_PT_INTERP    3  // Interpreter path
#define UNK_PT_NOTE      4  // Note section
#define UNK_PT_SHLIB     5  // Reserved
#define UNK_PT_PHDR      6  // Program header table
#define UNK_PT_TLS       7  // Thread-local storage
#define UNK_PT_CAPS      8  // Capability table (Unknown OS specific)
#define UNK_PT_METADATA  9  // Application metadata
```

### Segment Flags
```c
#define UNK_PF_R    (1 << 0)  // Readable
#define UNK_PF_W    (1 << 1)  // Writable
#define UNK_PF_X    (1 << 2)  // Executable
```

---

## Section Headers

```c
struct unk_shdr {
    uint32_t name;            // Index into string table
    uint32_t type;            // Section type
    uint64_t flags;           // Section flags
    uint64_t vaddr;           // Virtual address
    uint64_t offset;          // File offset
    uint64_t size;            // Section size
    uint32_t link;            // Link to other section
    uint32_t info;            // Additional info
    uint64_t align;           // Alignment
    uint64_t entsize;         // Entry size (for tables)
};
```

### Section Types
```c
#define UNK_SHT_NULL        0
#define UNK_SHT_PROGBITS    1  // Program data
#define UNK_SHT_SYMTAB      2  // Symbol table
#define UNK_SHT_STRTAB      3  // String table
#define UNK_SHT_RELA        4  // Relocation entries with addends
#define UNK_SHT_HASH        5  // Symbol hash table
#define UNK_SHT_DYNAMIC     6  // Dynamic linking info
#define UNK_SHT_NOTE        7  // Notes
#define UNK_SHT_NOBITS      8  // BSS (no space in file)
#define UNK_SHT_REL         9  // Relocation entries (no addends)
#define UNK_SHT_DYNSYM     11  // Dynamic symbol table
#define UNK_SHT_CAPS       12  // Capability table
#define UNK_SHT_METADATA   13  // Application metadata (JSON)
```

---

## Symbol Table

```c
struct unk_sym {
    uint32_t name;            // Index into string table
    uint8_t  info;            // Binding + type
    uint8_t  other;           // Visibility
    uint16_t shndx;           // Section index
    uint64_t value;           // Symbol value
    uint64_t size;            // Symbol size
};
```

### Symbol Binding (info >> 4)
```c
#define UNK_STB_LOCAL   0  // Local symbol
#define UNK_STB_GLOBAL  1  // Global symbol
#define UNK_STB_WEAK    2  // Weak symbol
```

### Symbol Type (info & 0xF)
```c
#define UNK_STT_NOTYPE  0  // Unspecified
#define UNK_STT_OBJECT  1  // Data object
#define UNK_STT_FUNC    2  // Function
#define UNK_STT_SECTION 3  // Section
#define UNK_STT_FILE    4  // File
```

---

## Relocation Table

```c
struct unk_rela {
    uint64_t offset;          // Address to relocate
    uint64_t info;            // Symbol index + type
    int64_t  addend;          // Addend
};
```

### Relocation Types (x86_64)
```c
#define UNK_R_X86_64_NONE       0
#define UNK_R_X86_64_64         1  // Direct 64-bit
#define UNK_R_X86_64_PC32       2  // PC-relative 32-bit
#define UNK_R_X86_64_GOT32      3  // GOT 32-bit
#define UNK_R_X86_64_PLT32      4  // PLT 32-bit
#define UNK_R_X86_64_COPY       5  // Copy symbol
#define UNK_R_X86_64_GLOB_DAT   6  // Global data
#define UNK_R_X86_64_JUMP_SLOT  7  // Jump slot
#define UNK_R_X86_64_RELATIVE   8  // Relative
#define UNK_R_X86_64_GOTPCREL   9  // GOT PC-relative
#define UNK_R_X86_64_32         10 // Direct 32-bit
#define UNK_R_X86_64_32S        11 // Direct 32-bit signed
#define UNK_R_X86_64_16         12 // Direct 16-bit
#define UNK_R_X86_64_PC16       13 // PC-relative 16-bit
#define UNK_R_X86_64_8          14 // Direct 8-bit
#define UNK_R_X86_64_PC8        15 // PC-relative 8-bit
```

---

## Capability Table (Unknown OS Specific)

```c
struct unk_capability {
    uint64_t type;            // Capability type
    uint64_t value;           // Capability value/argument
};
```

### Capability Types
```c
#define UNK_CAP_NONE              0
#define UNK_CAP_FS_READ           1  // Read filesystem
#define UNK_CAP_FS_WRITE          2  // Write filesystem
#define UNK_CAP_FS_EXECUTE        3  // Execute files
#define UNK_CAP_NET_TCP_CLIENT    4  // TCP client
#define UNK_CAP_NET_TCP_SERVER    5  // TCP server
#define UNK_CAP_NET_UDP           6  // UDP
#define UNK_CAP_NET_RAW           7  // Raw sockets
#define UNK_CAP_HW_GPU            8  // GPU access
#define UNK_CAP_HW_AUDIO          9  // Audio device
#define UNK_CAP_HW_CAMERA         10 // Camera
#define UNK_CAP_HW_USB            11 // USB
#define UNK_CAP_SYS_PROC_SPAWN    12 // Spawn processes
#define UNK_CAP_SYS_PROC_KILL     13 // Kill processes
#define UNK_CAP_SYS_DEBUG         14 // Debug other processes
#define UNK_CAP_SYS_ADMIN         15 // Admin operations
#define UNK_CAP_IPC_SEND          16 // Send IPC messages
#define UNK_CAP_IPC_RECV          17 // Receive IPC messages
#define UNK_CAP_CRYPTO_RANDOM     18 // Hardware RNG
#define UNK_CAP_CRYPTO_KEYS       19 // Key storage
```

---

## Application Metadata Section (JSON)

Stored in section of type `UNK_SHT_METADATA`. Contains:

```json
{
  "name": "Application Name",
  "version": "1.0.0",
  "author": "Developer Name",
  "description": "Application description",
  "license": "MIT",
  "homepage": "https://example.com",
  "icon": "icon.png",
  "categories": ["Utility", "System"],
  "min_os_version": "1.0.0",
  "capabilities": ["fs_read", "net_tcp_client"],
  "entry_point": "main",
  "args": [
    {"name": "input", "type": "file", "description": "Input file"},
    {"name": "verbose", "type": "bool", "description": "Verbose output"}
  ]
}
```

---

## Signature

Optional Ed25519 signature of the entire file (excluding signature section).

```c
struct unk_signature {
    uint8_t  algorithm;         // 1 = Ed25519
    uint8_t  reserved[3];
    uint32_t key_id;            // Key identifier
    uint64_t signed_offset;     // Start of signed region
    uint64_t signed_size;       // Size of signed region
    uint8_t  signature[64];     // Ed25519 signature
};
```

---

## Loading Process

1. **Verify magic** - Check `UNKEXEC\0`
2. **Verify checksum** - CRC32 of file
3. **Verify signature** (if `UNK_FLAG_SIGNED`)
4. **Parse program headers** - Find loadable segments
5. **Allocate memory** - Map segments at virtual addresses
6. **Apply relocations** - Fix up addresses
7. **Resolve dynamic symbols** - Link shared libraries
8. **Set up capabilities** - Enforce capability table
9. **Jump to entry point** - Start execution

---

## Converter: exe2unk

Tool to convert ELF/PE/Mach-O to .unk format.

```bash
# Convert ELF to UNK
exe2unk input.elf output.unk

# Convert with signing
exe2unk input.elf output.unk --sign --key-id 0x1234

# Convert with capabilities
exe2unk input.elf output.unk --cap fs_read --cap net_tcp_client

# Add metadata
exe2unk input.elf output.unk --metadata app.json
```

---

## Example: Minimal .unk File (Hex Dump)

```
00000000: 55 4e 4b 45 58 45 43 00  01 01 00 01 00 00 00 00  |UNKEXEC.........|
00000010: 00 10 00 00 00 00 00 00  80 00 00 00 00 00 00 00  |................|
00000020: 00 01 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000030: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000040: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000050: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000060: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000070: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|

Program Header (LOAD):
00000080: 01 00 00 00 07 00 00 00  80 00 00 00 00 00 00 00  |................|
00000090: 00 10 00 00 00 00 00 00  00 10 00 00 00 00 00 00  |................|
000000a0: 50 00 00 00 00 00 00 00  50 00 00 00 00 00 00 00  |P.......P.......|
000000b0: 00 10 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|

Code section follows...
```

---

## References

- [ELF Specification](https://refspecs.linuxfoundation.org/elf/)
- [PE/COFF Specification](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)
- [Mach-O Format](https://developer.apple.com/documentation/kernel/mach-o_runtime_architecture)
- [Limine Boot Protocol](https://limine-bootloader.org/)

---

*Unknown OS - India's First Independent Operating System*
*byrex.me@proton.me*