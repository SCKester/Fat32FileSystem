# FAT32 File System Utility

A small psuedo filesystem driver implementing FAT32 standard and showcasing ability to work with file system drivers in C.
can read and write and navigate directories on a psuedo disk image with opened and closed files.

## Group Members
- **James Fontaine**: jwf22c@fsu.edu
- **Steven Kester**: js19@fsu.edu
- **HUGH LONG**: hal20a@fsu.edu
## Division of Labor

### Part 1: Mounting the Image
- **Responsibilities**: mount image and info and exit
- **Assigned to**: Steven Kester

### Part 2: Navigation
- **Responsibilities**: Implementing cd and ls 
- **Assigned to**: James Fontaine

### Part 3: Create
- **Responsibilities**: creat and mkdir
- **Assigned to**: Steven Kester

### Part 4: Read
- **Responsibilities**: implement ls , open , lsof , lseek , and read
- **Assigned to**: HUGH LONG

### Part 5: Update
- **Responsibilities**: Write and mv
- **Assigned to**: HUGH LONG , Steven Kester

### Part 6: Delete
- **Responsibilities**: Implementing rm and rmdir
- **Assigned to**: James Fontaine

## File Listing
```
filesys/
│
├── src/
│ ├── lexer.c
│ └── utils.c
| |__ fat32.c
│
├── include/
│ └── lexer.h
│ └── utils.h
| |__ fat32.h
│
├── README.md
└── Makefile
```
## How to Compile & Execute

### Requirements
- **Compiler**: `gcc`

### Compilation
For a C/C++ example:
```bash
make
```
This will build the executable in bin/filesys
### Execution
```bash
./bin/filesys fat32.img
```

Once launched, the shell prompt will appear:

## Bugs
- **Bug 1**: mv does not work on multicluster directories.
- **Bug 2**: This is bug 2.
- **Bug 3**: This is bug 3.


## Considerations

