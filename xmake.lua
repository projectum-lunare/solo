-- solo - Physical I/O, System Foundations & Hardware Abstractions
-- Projectum Lunare <https://github.com/projectum-lunare/solo>
-- Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
-- Mozilla Public License Version 2.0

set_project("solo")
set_version("0.2.0")

set_languages("c89")
set_warnings("all", "extra")

add_rules("mode.debug", "mode.release")

-- Public C headers exposed to downstream consumers
add_includedirs("include")

-- Official Projectum Lunare Package Repository & Build Infrastructure Addon
add_repositories("projectum-lunare https://github.com/projectum-lunare/xmake-repo.git")
add_addons("forge master")
includes("@addon/forge/forge.lua")

-- Host dependencies for Lua C bindings and C test suites
if is_plat("linux", "macosx", "bsd", "windows", "mingw") then
    add_requires("lua >=5.4", "greatest")
end

-- ==============================================================================
-- Platform Driver & Toolchain Declarative Matrix
-- ==============================================================================
local PLATFORMS = {
    -- Vintage & Retro Platforms
    amiga = {
        targetdir  = "build/amiga",
        filesystem = "amiga",
        parallel   = "amiga",
        serial     = "amiga",
        socket     = "bsd",
        sandbox    = "stub",
        language   = "gnu89",
    },
    atari = {
        targetdir  = "build/atarist",
        filesystem = "atarist",
        parallel   = "atarist",
        serial     = "atarist",
        socket     = "bsd",
        sandbox    = "stub",
        language   = "gnu89",
    },
    kolibri = {
        targetdir  = "build/kolibri",
        filesystem = "kolibri",
        parallel   = "kolibri",
        serial     = "kolibri",
        socket     = "kolibri",
        sandbox    = "stub",
        language   = "gnu89",
        cxflags    = {"-nostdlib", "-U_WIN32", "-D__KOLIBRIOS__"},
    },
    dos = {
        targetdir  = "build/dos",
        filesystem = "dos",
        parallel   = "dos",
        serial     = "dos",
        socket     = "bsd",
        sandbox    = "stub",
        rules      = "forge.watcom",
        watt32     = true,
    },
    os2 = {
        targetdir  = "build/os2",
        filesystem = "os2",
        parallel   = "os2",
        serial     = "os2",
        socket     = "bsd",
        sandbox    = "stub",
        rules      = "forge.watcom",
    },
    wince = {
        targetdir  = "build/windows_ce",
        filesystem = "windows_ce",
        parallel   = "windows_ce",
        serial     = "windows_ce",
        socket     = "bsd",
        sandbox    = "stub",
        rules      = "forge.evc3",
        arch_dir   = true,
    },

    -- Windows NT & MinGW
    windows = {
        targetdir  = "build/windows_nt",
        filesystem = "windows_nt",
        parallel   = "windows_nt",
        serial     = "windows_nt",
        socket     = "bsd",
        sandbox    = "stub",
        can_test   = true,
    },
    mingw = {
        targetdir  = "build/windows_nt",
        filesystem = "windows_nt",
        parallel   = "windows_nt",
        serial     = "windows_nt",
        socket     = "bsd",
        sandbox    = "stub",
        can_test   = true,
    },

    -- Plan 9 from Bell Labs
    plan9 = {
        targetdir  = "build/plan9",
        filesystem = "plan9",
        parallel   = "plan9",
        serial     = "plan9",
        socket     = "plan9",
        sandbox    = "stub",
    },

    -- POSIX & Modern Unix (Linux, BSDs, macOS)
    linux = {
        targetdir  = "build/posix",
        filesystem = "posix",
        parallel   = "posix",
        serial     = "posix",
        socket     = "bsd",
        sandbox    = "linux",
        pic        = true,
        can_test   = true,
    },
    openbsd = {
        targetdir  = "build/posix",
        filesystem = "posix",
        parallel   = "posix",
        serial     = "posix",
        socket     = "bsd",
        sandbox    = "openbsd",
        pic        = true,
        can_test   = true,
    },
    freebsd = {
        targetdir  = "build/posix",
        filesystem = "posix",
        parallel   = "posix",
        serial     = "posix",
        socket     = "bsd",
        sandbox    = "freebsd",
        pic        = true,
        can_test   = true,
    },
    bsd = {
        targetdir  = "build/posix",
        filesystem = "posix",
        parallel   = "posix",
        serial     = "posix",
        socket     = "bsd",
        sandbox    = "freebsd",
        pic        = true,
        can_test   = true,
    },
    netbsd = {
        targetdir  = "build/posix",
        filesystem = "posix",
        parallel   = "posix",
        serial     = "posix",
        socket     = "bsd",
        sandbox    = "stub",
        pic        = true,
        can_test   = true,
    },
    dragonflybsd = {
        targetdir  = "build/posix",
        filesystem = "posix",
        parallel   = "posix",
        serial     = "posix",
        socket     = "bsd",
        sandbox    = "stub",
        pic        = true,
        can_test   = true,
    },
    macosx = {
        targetdir  = "build/posix",
        filesystem = "posix",
        parallel   = "stub",
        serial     = "posix",
        socket     = "bsd",
        sandbox    = "stub",
        pic        = true,
        can_test   = true,
    },
}

local plat_name = get_config("plat") or os.host()
local conf = PLATFORMS[plat_name] or {
    targetdir  = "build/stub",
    filesystem = "stub",
    parallel   = "stub",
    serial     = "stub",
    socket     = "stub",
    sandbox    = "stub",
}
local target_dir = conf.arch_dir and (conf.targetdir .. "/" .. (get_config("arch") or "arm")) or conf.targetdir

local function apply_platform_rules(c)
    if c.language then set_languages(c.language) end
    if c.rules then add_rules(c.rules, {override = true}) end
    if c.cxflags then add_cxflags(c.cxflags, {force = true}) end
    if c.pic then add_cxflags("-fPIC") end
    if c.watt32 then
        local watt32 = os.getenv("WATT32_DIR") or path.join(os.getenv("HOME") or "", ".local/opt/watt32")
        if os.isdir(path.join(watt32, "inc")) then
            add_defines("USE_WATT32")
            add_includedirs(path.join(watt32, "inc"))
        end
    end
end

-- ==============================================================================
-- 1. Static C89 Library (libsolo.a / solo.lib) - Multi-Platform Engine
-- ==============================================================================
target("solo_static")
    set_kind("static")
    set_basename("solo")
    set_targetdir(target_dir)
    add_headerfiles("include/(solo/*.h)")
    add_files(
        "src/solo.c",
        "src/platform/platform.c",
        "src/filesystem/filesystem.c",
        "src/socket/socket_common.c",
        "src/filesystem/filesystem_" .. conf.filesystem .. ".c",
        "src/parallel/parallel_" .. conf.parallel .. ".c",
        "src/serial/serial_" .. conf.serial .. ".c",
        "src/socket/socket_" .. conf.socket .. ".c",
        "src/sandbox/sandbox_" .. conf.sandbox .. ".c"
    )
    apply_platform_rules(conf)

-- ==============================================================================
-- 2. Lua Subsystem & C Modules (Powered by forge.lua.module)
-- ==============================================================================
if is_plat("linux", "macosx", "bsd", "windows", "mingw") then

-- 2.1 Static Lua C Module (libsolo_lua.a / solo_lua.lib) for embedding
target("solo_lua_static")
    add_rules("forge.lua.module")
    set_values("forge.lua.kind", "static")
    set_basename("solo_lua")
    add_deps("solo_static")
    add_defines("LUA_USE_C89")
    add_files("src/**_lua.c")

-- 2.2 Umbrella Shared Lua C Module (build/lua/solo.so & ./solo.so)
target("solo")
    add_rules("forge.lua.module")
    set_values("forge.lua.kind", "shared")
    add_deps("solo_static")
    add_defines("LUA_USE_C89")
    add_files("src/**_lua.c")
    after_build(function (target)
        -- Mirror shared module to project root for zero-config require("solo")
        os.trycp(target:targetfile(), path.join(os.projectdir(), path.filename(target:targetfile())))
    end)

-- 2.3 Standalone C Modules (build/lua/solo/<submodule>.so)
local submodules = {
    platform   = {"src/platform/platform_lua.c", "src/platform/platform.c"},
    filesystem = {"src/filesystem/filesystem_lua.c", "src/filesystem/filesystem.c", "src/filesystem/filesystem_" .. conf.filesystem .. ".c", "src/platform/platform.c"},
    fs         = {"src/filesystem/filesystem_lua.c", "src/filesystem/filesystem.c", "src/filesystem/filesystem_" .. conf.filesystem .. ".c", "src/platform/platform.c"},
    serial     = {"src/serial/serial_lua.c", "src/serial/serial_" .. conf.serial .. ".c", "src/platform/platform.c"},
    parallel   = {"src/parallel/parallel_lua.c", "src/parallel/parallel_" .. conf.parallel .. ".c", "src/platform/platform.c"},
    socket     = {"src/socket/socket_lua.c", "src/socket/socket_common.c", "src/socket/socket_" .. conf.socket .. ".c", "src/platform/platform.c"},
    sandbox    = {"src/sandbox/sandbox_lua.c", "src/sandbox/sandbox_" .. conf.sandbox .. ".c", "src/platform/platform.c"},
}

for mod_name, files in pairs(submodules) do
    target("solo." .. mod_name)
        add_rules("forge.lua.module")
        set_values("forge.lua.kind", "shared")
        add_defines("LUA_USE_C89")
        add_files(files)
end

-- ==============================================================================
-- 3. Test Suites & Application Launchers (Powered by forge.lua.app)
-- ==============================================================================

-- 3.1 C API Test Suite
target("test_c_api")
    set_kind("binary")
    set_targetdir("tests")
    set_rundir(os.projectdir())
    add_files("tests/c/*.c")
    add_deps("solo_static")
    add_packages("greatest")
    add_tests("c_api")

-- 3.2 Lua Test Suite & Portable Launcher (xmake run test_lua)
target("test_lua")
    add_rules("forge.lua.app")
    set_values("forge.lua.entry", "tests/runner.lua")
    add_deps(
        "solo",
        "solo.platform",
        "solo.filesystem",
        "solo.fs",
        "solo.serial",
        "solo.parallel",
        "solo.socket",
        "solo.sandbox"
    )
    on_test(function (target)
        os.execv(path.join(os.projectdir(), "build/bin/test_lua"))
        return true
    end)
    add_tests("lua_suite")

end -- is_plat(host)

-- ==============================================================================
-- 4. Cross-Platform Smoke Binary
-- ==============================================================================
target("solo_smoke")
    set_kind("binary")
    set_rundir(os.projectdir())
    set_targetdir(target_dir)
    add_files("tests/smoke.c")
    add_deps("solo_static")
    apply_platform_rules(conf)
    if conf.can_test then
        set_targetdir("build/bin")
        add_tests("smoke")
    end
