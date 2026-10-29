-- solo - Physical I/O, System Foundations & Hardware Abstractions
-- Projectum Lunare <https://github.com/projectum-lunare/solo>
-- Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
-- Mozilla Public License Version 2.0

set_project("solo")
set_version("0.1.0")

set_languages("c89")
set_warnings("all", "extra")

add_rules("mode.debug", "mode.release")

-- Public C headers
add_includedirs("include", {public = true})

-- Official Projectum Lunare Package Repository
add_repositories("projectum-lunare https://github.com/projectum-lunare/xmake-repo.git")

add_addons("forge")
includes("@addon/forge/forge.lua")

if is_plat("linux", "macosx", "bsd") then
    add_requires("lua >=5.4")
    add_requires("greatest")
end

-- ==============================================================================
-- 1. Static C89 Library (libsolo.a / libsolo.lib) - Multi-Platform
-- ==============================================================================
target("solo_static")
    set_kind("static")
    set_basename("solo")
    add_files(
        "src/solo.c",
        "src/platform/platform.c",
        "src/filesystem/filesystem.c"
    )
    add_headerfiles("include/(solo/*.h)")

    -- Vintage platform driver mapping
    local drivers = {
        amiga   = "amiga",
        atari   = "atarist",
        kolibri = "kolibri",
        dos     = "dos",
        os2     = "os2",
        wince   = "windows_ce"
    }

    local plat = get_config("plat")
    local driver = drivers[plat]

    if driver then
        local targetdir = (plat == "wince") and ("build/windows_ce/" .. (get_config("arch") or "arm"))
                                            or ("build/" .. (plat == "atari" and "atarist" or plat))
        set_targetdir(targetdir)
        add_files(
            "src/parallel/parallel_" .. driver .. ".c",
            "src/serial/serial_" .. driver .. ".c",
            "src/socket/socket_" .. driver .. ".c",
            "src/sandbox/sandbox_stub.c"
        )
        if is_plat("amiga", "atari", "kolibri") then
            set_languages("gnu89")
            if is_plat("kolibri") then
                add_cxflags("-nostdlib", "-U_WIN32", "-D__KOLIBRIOS__", {force = true})
            end
        elseif is_plat("dos", "os2") then
            add_rules("forge.watcom", {override = true})
            if is_plat("dos") then
                local watt32_dir = os.getenv("WATT32_DIR") or path.join(os.getenv("HOME"), ".local/opt/watt32")
                if os.isdir(path.join(watt32_dir, "inc")) then
                    add_defines("USE_WATT32")
                    add_includedirs(path.join(watt32_dir, "inc"))
                end
            end
        elseif is_plat("wince") then
            add_rules("forge.evc3", {override = true})
        end
    elseif is_plat("linux", "macosx", "bsd") then
        set_targetdir("build/posix")
        add_cxflags("-fPIC")
        add_files(
            "src/parallel/parallel_posix.c",
            "src/serial/serial_posix.c",
            "src/socket/socket_posix.c"
        )
        if is_plat("linux") then
            add_files("src/sandbox/sandbox_linux.c")
        elseif is_plat("bsd") then
            add_files("src/sandbox/sandbox_freebsd.c")
        else
            add_files("src/sandbox/sandbox_stub.c")
        end
    end

-- ==============================================================================
-- 2. Host Lua C Modules and Test Suites (Active on Host POSIX / Windows)
-- ==============================================================================
if is_plat("linux", "macosx", "bsd") then

target("solo_lua_static")
    set_kind("static")
    set_basename("solo_lua")
    add_deps("solo_static")
    add_packages("lua")
    add_defines("LUA_USE_C89")
    add_cxflags("-fPIC")
    add_files("src/**_lua.c")

target("solo_lua")
    set_kind("shared")
    set_basename("solo")
    set_prefixname("")
    set_targetdir(".")
    add_deps("solo_static")
    add_packages("lua")
    add_defines("LUA_USE_C89")
    add_files("src/**_lua.c")

local sandbox_driver = is_plat("linux") and "src/sandbox/sandbox_linux.c" or "src/sandbox/sandbox_stub.c"

local submodules = {
    platform   = {"src/platform/platform_lua.c", "src/platform/platform.c"},
    filesystem = {"src/filesystem/filesystem_lua.c", "src/filesystem/filesystem.c", "src/platform/platform.c"},
    fs         = {"src/filesystem/filesystem_lua.c", "src/filesystem/filesystem.c", "src/platform/platform.c"},
    serial     = {"src/serial/serial_lua.c", "src/serial/serial_posix.c", "src/platform/platform.c"},
    parallel   = {"src/parallel/parallel_lua.c", "src/parallel/parallel_posix.c", "src/platform/platform.c"},
    socket     = {"src/socket/socket_lua.c", "src/socket/socket_posix.c", "src/platform/platform.c"},
    sandbox    = {"src/sandbox/sandbox_lua.c", sandbox_driver, "src/platform/platform.c"},
}

for mod_name, files in pairs(submodules) do
    target("solo." .. mod_name)
        set_kind("shared")
        set_basename(mod_name)
        set_prefixname("")
        set_targetdir("build/lua/solo")
        add_cxflags("-fPIC")
        add_packages("lua")
        add_defines("LUA_USE_C89")
        add_files(files)
end

target("test_c_api")
    set_kind("binary")
    set_targetdir("tests")
    set_rundir(os.projectdir())
    add_files("tests/test_c_api.c")
    add_deps("solo_static")
    add_packages("greatest")

end -- is_plat(host)