# FAT32 File System Utility

A small psuedo filesystem driver implementing FAT32 standard and showcasing ability to work with file system drivers in C.
can read and werite and navigate directories on a psuedo disk image with opened and closed files.

## Group Members
- **John Doe**: jd19@fsu.edu
- **Jane Smith**: js19@fsu.edu
- **HUGH LONG**: hal20a@fsu.edu
## Division of Labor

### Part 1: Mounting the Image
- **Responsibilities**: [Description]
- **Assigned to**: John Doe

### Part 2: Navigation
- **Responsibilities**: [Description]
- **Assigned to**: Jane Smith

### Part 3: Create
- **Responsibilities**: [Description]
- **Assigned to**: Alex Brown

### Part 4: Read
- **Responsibilities**: [Description]
- **Assigned to**: HUGH LONG

### Part 5: Update
- **Responsibilities**: [Description]
- **Assigned to**: HUGH LONG

### Part 6: Delete
- **Responsibilities**: [Description]
- **Assigned to**: Jane Smith

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
- **Bug 1**: This is bug 1.
- **Bug 2**: This is bug 2.
- **Bug 3**: This is bug 3.

## Extra Credit
- **Extra Credit 1**: [Extra Credit Option]
- **Extra Credit 2**: [Extra Credit Option]
- **Extra Credit 3**: [Extra Credit Option]

## Considerations
[Description]
