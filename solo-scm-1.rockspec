package = "solo"
version = "scm-1"

source = {
   url = "git+https://github.com/projectum-lunare/solo.git"
}

description = {
   summary = "Physical I/O, System Foundations & Hardware Abstractions",
   detailed = [[
      Unified ANSI C89 / C99 system foundation with Lua bindings for physical I/O,
      OS sandboxing, and runtime abstractions across modern and vintage platforms.
      Includes platform (compiler, OS & CPU detection), filesystem / fs (safe filesystem
      operations), parallel (parallel port), serial (serial UART), socket (TCP/UDP sockets),
      and sandbox (OS kernel sandboxing).
   ]],
   homepage = "https://github.com/projectum-lunare/solo",
   license = "MPL-2.0"
}

dependencies = {
   "lua >= 5.1, <= 5.5"
}

build = {
   type = "make",
   build_variables = {
      CFLAGS = "$(CFLAGS)",
      LIBFLAG = "$(LIBFLAG)",
      LUA = "$(LUA)",
      LUA_INCDIR = "$(LUA_INCDIR)"
   },
   install_variables = {
      INST_PREFIX = "$(PREFIX)",
      INST_BINDIR = "$(BINDIR)",
      INST_LIBDIR = "$(LIBDIR)",
      INST_LUADIR = "$(LUADIR)"
   }
}
