# Filesystem & Path I/O (`solo.filesystem` / `fs`)

The `filesystem` subsystem (also accessible via the short alias `solo.fs`) provides cross-platform path manipulation, recursive directory operations, file metadata inspection, and streaming file I/O.

---

## File Existence & Path Testing

=== "C (C89)"

    ```c
    #include <solo/solo.h>

    if (solo_fs_exists("/tmp/test.txt") == SOLO_TRUE) {
        if (solo_fs_is_file("/tmp/test.txt") == SOLO_TRUE) {
            /* Regular file */
        }
    }

    if (solo_fs_is_dir("/tmp/output") == SOLO_FALSE) {
        solo_fs_mkdir_p("/tmp/output");
    }
    ```

=== "Lua"

    ```lua
    local fs = require("solo.fs")

    if fs.exists("/tmp/test.txt") then
        if fs.is_file("/tmp/test.txt") then
            -- Regular file
        end
    end

    if not fs.is_dir("/tmp/output") then
        fs.mkdir_p("/tmp/output")
    end
    ```

---

## Whole File Reading & Writing

=== "C (C89)"

    ```c
    #include <stdio.h>
    #include <stdlib.h>
    #include <solo/solo.h>

    void example(void) {
        char *content = NULL;
        size_t size = 0;

        if (solo_fs_write_file("message.txt", "Hello from solo!\n", 17) == SOLO_OK) {
            content = solo_fs_read_file("message.txt", &size);
            if (content) {
                printf("Read %lu bytes: %s", (unsigned long)size, content);
                free(content);
            }
        }
    }
    ```

=== "Lua"

    ```lua
    local fs = require("solo.fs")

    fs.write_file("message.txt", "Hello from solo!\n")
    local content = fs.read_file("message.txt")
    print(content)
    ```

---

## Directory Traversal

=== "Lua"

    ```lua
    local fs = require("solo.fs")

    -- List directory contents as an array of filenames
    local files = fs.list_dir("/var/log")
    for _, name in ipairs(files) do
        print("Found entry: " .. name)
    end
    ```

---

## Recursive Operations

- `solo_fs_copy_r(src, dst)` / `fs.copy_r(src, dst)`: Recursively copy directory trees.
- `solo_fs_remove_r(path)` / `fs.remove_r(path)`: Recursively remove directory trees.
- `solo_fs_mkdir_p(path)` / `fs.mkdir_p(path)`: Create missing parent directories recursively.
