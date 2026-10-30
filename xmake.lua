-- solo - Physical I/O, System Foundations & Hardware Abstractions
-- Projectum Lunare <https://github.com/projectum-lunare/solo>
-- Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
-- Mozilla Public License Version 2.0

set_project("solo")
set_version("0.1.0")

set_languages("c89")
set_warnings("all", "extra")

add_rules("mode.debug", "mode.release")

-- Public C headers exposed to downstream consumers
add_includedirs("include", {public = true})

-- Official Projectum Lunare Package Repository & Build Infrastructure Addon
add_repositories("projectum-lunare https://github.com/projectum-lunare/xmake-repo.git")
add_addons("forge master")
includes("@addon/forge/forge.lua")

-- Host dependencies for Lua C bindings and C test suites
if is_plat("linux", "macosx", "bsd", "windows", "mingw") then
    add_requires("lua >=5.4", "greatest")
end

-- ==============================================================================
-- 1. Static C89 Library (libsolo.a / solo.lib) - Multi-Platform Engine
-- ==============================================================================
target("solo_static")
    set_kind("static")
    set_basename("solo")

    -- Core foundation sources and public API headers
    add_files(
        "src/solo.c",
        "src/platform/platform.c",
        "src/filesystem/filesystem.c",
        "src/socket/socket_common.c"
    )
    add_headerfiles("include/(solo/*.h)")

    -- Platform-specific drivers and cross-compilation rules
    if is_plat("amiga") then
        set_targetdir("build/amiga")
        set_languages("gnu89")
        add_files(
            "src/filesystem/filesystem_amiga.c",
            "src/parallel/parallel_amiga.c",
            "src/serial/serial_amiga.c",
            "src/socket/socket_bsd.c",
            "src/sandbox/sandbox_stub.c"
        )
    elseif is_plat("atari") then
        set_targetdir("build/atarist")
        set_languages("gnu89")
        add_files(
            "src/filesystem/filesystem_atarist.c",
            "src/parallel/parallel_atarist.c",
            "src/serial/serial_atarist.c",
            "src/socket/socket_bsd.c",
            "src/sandbox/sandbox_stub.c"
        )
    elseif is_plat("kolibri") then
        set_targetdir("build/kolibri")
        set_languages("gnu89")
        add_cxflags("-nostdlib", "-U_WIN32", "-D__KOLIBRIOS__", {force = true})
        add_files(
            "src/filesystem/filesystem_kolibri.c",
            "src/parallel/parallel_kolibri.c",
            "src/serial/serial_kolibri.c",
            "src/socket/socket_kolibri.c",
            "src/sandbox/sandbox_stub.c"
        )
    elseif is_plat("dos") then
        set_targetdir("build/dos")
        add_rules("forge.watcom", {override = true})
        add_files(
            "src/filesystem/filesystem_dos.c",
            "src/parallel/parallel_dos.c",
            "src/serial/serial_dos.c",
            "src/socket/socket_bsd.c",
            "src/sandbox/sandbox_stub.c"
        )
        -- WATT-32 TCP/IP stack integration
        local watt32_dir = os.getenv("WATT32_DIR") or path.join(os.getenv("HOME") or "", ".local/opt/watt32")
        if os.isdir(path.join(watt32_dir, "inc")) then
            add_defines("USE_WATT32")
            add_includedirs(path.join(watt32_dir, "inc"))
        end
    elseif is_plat("os2") then
        set_targetdir("build/os2")
        add_rules("forge.watcom", {override = true})
        add_files(
            "src/filesystem/filesystem_os2.c",
            "src/parallel/parallel_os2.c",
            "src/serial/serial_os2.c",
            "src/socket/socket_bsd.c",
            "src/sandbox/sandbox_stub.c"
        )
    elseif is_plat("wince") then
        local arch = get_config("arch") or "arm"
        set_targetdir("build/windows_ce/" .. arch)
        add_rules("forge.evc3", {override = true})
        add_files(
            "src/filesystem/filesystem_windows_ce.c",
            "src/parallel/parallel_windows_ce.c",
            "src/serial/serial_windows_ce.c",
            "src/socket/socket_bsd.c",
            "src/sandbox/sandbox_stub.c"
        )
    elseif is_plat("windows", "mingw") then
        set_targetdir("build/windows_nt")
        add_files(
            "src/filesystem/filesystem_windows_nt.c",
            "src/parallel/parallel_windows_nt.c",
            "src/serial/serial_windows_nt.c",
            "src/socket/socket_bsd.c",
            "src/sandbox/sandbox_stub.c"
        )
    elseif is_plat("plan9") then
        set_targetdir("build/plan9")
        add_files(
            "src/filesystem/filesystem_plan9.c",
            "src/parallel/parallel_plan9.c",
            "src/serial/serial_plan9.c",
            "src/socket/socket_plan9.c",
            "src/sandbox/sandbox_stub.c"
        )
    elseif is_plat("linux", "macosx", "bsd") then
        set_targetdir("build/posix")
        add_cxflags("-fPIC")
        add_files(
            "src/filesystem/filesystem_posix.c",
            "src/parallel/parallel_posix.c",
            "src/serial/serial_posix.c",
            "src/socket/socket_bsd.c"
        )
        if is_plat("linux") then
            add_files("src/sandbox/sandbox_linux.c")
        elseif is_plat("openbsd") then
            add_files("src/sandbox/sandbox_openbsd.c")
        elseif is_plat("freebsd", "bsd") then
            add_files("src/sandbox/sandbox_freebsd.c")
        else
            add_files("src/sandbox/sandbox_stub.c")
        end
    else
        -- Generic C89 stub fallback
        set_targetdir("build/stub")
        add_files(
            "src/filesystem/filesystem_stub.c",
            "src/parallel/parallel_stub.c",
            "src/serial/serial_stub.c",
            "src/socket/socket_stub.c",
            "src/sandbox/sandbox_stub.c"
        )
    end

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
local sandbox_driver = is_plat("linux") and "src/sandbox/sandbox_linux.c"
                    or (is_plat("openbsd") and "src/sandbox/sandbox_openbsd.c"
                    or (is_plat("freebsd", "bsd") and "src/sandbox/sandbox_freebsd.c"
                    or "src/sandbox/sandbox_stub.c"))

local is_win = is_plat("windows", "mingw")
local is_wince = is_plat("wince")
local serial_driver     = is_win and "src/serial/serial_windows_nt.c"   or "src/serial/serial_posix.c"
local parallel_driver   = is_win and "src/parallel/parallel_windows_nt.c" or "src/parallel/parallel_posix.c"
local filesystem_driver = is_win and "src/filesystem/filesystem_windows_nt.c"
                       or (is_wince and "src/filesystem/filesystem_windows_ce.c" or "src/filesystem/filesystem_posix.c")
local socket_driver     = "src/socket/socket_bsd.c"

local submodules = {
    platform   = {"src/platform/platform_lua.c", "src/platform/platform.c"},
    filesystem = {"src/filesystem/filesystem_lua.c", "src/filesystem/filesystem.c", filesystem_driver, "src/platform/platform.c"},
    fs         = {"src/filesystem/filesystem_lua.c", "src/filesystem/filesystem.c", filesystem_driver, "src/platform/platform.c"},
    serial     = {"src/serial/serial_lua.c", serial_driver, "src/platform/platform.c"},
    parallel   = {"src/parallel/parallel_lua.c", parallel_driver, "src/platform/platform.c"},
    socket     = {"src/socket/socket_lua.c", "src/socket/socket_common.c", socket_driver, "src/platform/platform.c"},
    sandbox    = {"src/sandbox/sandbox_lua.c", sandbox_driver, "src/platform/platform.c"},
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

end -- is_plat(host)

-- ==============================================================================
-- 4. Cross-Platform Smoke Binary
-- ==============================================================================
target("solo_smoke")
    set_kind("binary")
    set_rundir(os.projectdir())
    add_files("tests/smoke.c")
    add_deps("solo_static")
    if is_plat("amiga") then
        set_targetdir("build/amiga")
        set_languages("gnu89")
    elseif is_plat("atari") then
        set_targetdir("build/atarist")
        set_languages("gnu89")
    elseif is_plat("dos") then
        set_targetdir("build/dos")
        add_rules("forge.watcom", {override = true})
    elseif is_plat("os2") then
        set_targetdir("build/os2")
        add_rules("forge.watcom", {override = true})
    elseif is_plat("kolibri") then
        set_targetdir("build/kolibri")
        add_cxflags("-nostdlib", "-U_WIN32", "-D__KOLIBRIOS__", {force = true})
    else
        set_targetdir("build/bin")
        add_tests("smoke")
    end
