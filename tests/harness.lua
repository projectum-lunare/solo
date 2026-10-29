--
-- solo - Physical I/O, System Foundations & Hardware Abstractions
-- Projectum Lunare <https://github.com/projectum-lunare/solo>
-- Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
-- Mozilla Public License Version 2.0
--

---@class TestHarness
local M = {}

-- State
local current_describe = nil
local before_hooks = {}
local after_hooks = {}

local total_tests = 0
local passed_tests = 0
local failed_tests = 0
local total_assertions = 0
local failures = {}
local start_time = os.clock()

-- Color palette (ANSI)
local use_color = not os.getenv("NO_COLOR")
local C = {
    reset  = use_color and "\27[0m" or "",
    bold   = use_color and "\27[1m" or "",
    dim    = use_color and "\27[2m" or "",
    green  = use_color and "\27[32m" or "",
    red    = use_color and "\27[31m" or "",
    yellow = use_color and "\27[33m" or "",
    cyan   = use_color and "\27[36m" or "",
}

---Recursively compares two values for deep equality.
---@param a any
---@param b any
---@return boolean
local function deep_eq(a, b)
    if a == b then return true end
    if type(a) ~= type(b) then return false end
    if type(a) ~= "table" then return false end
    for k, v in pairs(a) do
        if not deep_eq(v, b[k]) then return false end
    end
    for k in pairs(b) do
        if a[k] == nil then return false end
    end
    return true
end

---Dumps a value into a readable string for error messages.
---@param v any
---@param depth? integer
---@return string
local function dump_val(v, depth)
    depth = depth or 0
    if depth > 3 then return "{ ... }" end
    local t = type(v)
    if t == "string" then
        return string.format("%q", v)
    elseif t == "table" then
        local parts = {}
        for k, val in pairs(v) do
            local k_str = type(k) == "string" and k or "[" .. tostring(k) .. "]"
            table.insert(parts, k_str .. " = " .. dump_val(val, depth + 1))
        end
        return "{ " .. table.concat(parts, ", ") .. " }"
    else
        return tostring(v)
    end
end

---Resets test harness state.
function M.reset()
    current_describe = nil
    before_hooks = {}
    after_hooks = {}
    total_tests = 0
    passed_tests = 0
    failed_tests = 0
    total_assertions = 0
    failures = {}
    start_time = os.clock()
end

---Registers a test suite group.
---@param title string
---@param fn fun()
function M.describe(title, fn)
    local prev_describe = current_describe
    local prev_before = before_hooks
    local prev_after = after_hooks

    current_describe = title
    before_hooks = {}
    after_hooks = {}

    io.write(string.format("\n%s%s● %s%s\n", C.bold, C.cyan, title, C.reset))
    fn()

    current_describe = prev_describe
    before_hooks = prev_before
    after_hooks = prev_after
end

---Registers a hook to execute before each test in the current suite.
---@param fn fun()
function M.before_each(fn)
    table.insert(before_hooks, fn)
end

---Registers a hook to execute after each test in the current suite.
---@param fn fun()
function M.after_each(fn)
    table.insert(after_hooks, fn)
end

---Executes an atomic test case.
---@param name string
---@param fn fun()
function M.test(name, fn)
    total_tests = total_tests + 1

    for _, hook in ipairs(before_hooks) do
        hook()
    end

    local ok, err = pcall(fn)

    for _, hook in ipairs(after_hooks) do
        pcall(hook)
    end

    if ok then
        passed_tests = passed_tests + 1
        io.write(string.format("  %s✓%s %s%s%s\n", C.green, C.reset, C.dim, name, C.reset))
    else
        failed_tests = failed_tests + 1
        local failure_info = {
            suite = current_describe or "Global",
            name = name,
            err = tostring(err)
        }
        table.insert(failures, failure_info)
        io.write(string.format("  %s✗ %s%s\n", C.red, name, C.reset))
    end
end

M.it = M.test

-- Assertions

---Asserts deep or primitive equality.
---@param actual any
---@param expected any
---@param msg? string
function M.assert_eq(actual, expected, msg)
    total_assertions = total_assertions + 1
    if not deep_eq(actual, expected) then
        local err = string.format("Expected: %s\n  Actual:   %s", dump_val(expected), dump_val(actual))
        if msg then err = msg .. "\n  " .. err end
        error(err, 2)
    end
end

---Asserts that two numbers are approximately equal within an epsilon tolerance.
---@param actual number
---@param expected number
---@param eps? number Epsilon tolerance (defaults to 1e-6)
---@param msg? string
function M.assert_near(actual, expected, eps, msg)
    total_assertions = total_assertions + 1
    if type(eps) == "string" and msg == nil then
        msg = eps
        eps = 1e-6
    else
        eps = eps or 1e-6
    end
    if type(actual) ~= "number" or type(expected) ~= "number" or math.abs(actual - expected) > eps then
        local diff = (type(actual) == "number" and type(expected) == "number")
            and tostring(math.abs(actual - expected)) or "n/a"
        local err = string.format("Expected %s to be near %s (delta <= %s, diff = %s)",
            dump_val(actual), dump_val(expected), tostring(eps), diff)
        if msg then err = msg .. "\n  " .. err end
        error(err, 2)
    end
end

M.assert_almost_eq = M.assert_near

---Asserts inequality.
---@param actual any
---@param unexpected any
---@param msg? string
function M.assert_ne(actual, unexpected, msg)
    total_assertions = total_assertions + 1
    if deep_eq(actual, unexpected) then
        local err = string.format("Expected value not to equal: %s", dump_val(unexpected))
        if msg then err = msg .. "\n  " .. err end
        error(err, 2)
    end
end

---Asserts that a condition evaluates to true.
---@param cond any
---@param msg? string
function M.assert_true(cond, msg)
    total_assertions = total_assertions + 1
    if not cond then
        error(msg or "Assertion failed: expected true, got " .. tostring(cond), 2)
    end
end

---Asserts that a condition evaluates to false.
---@param cond any
---@param msg? string
function M.assert_false(cond, msg)
    total_assertions = total_assertions + 1
    if cond then
        error(msg or "Assertion failed: expected false, got " .. tostring(cond), 2)
    end
end

---Asserts that a value is nil.
---@param val any
---@param msg? string
function M.assert_nil(val, msg)
    total_assertions = total_assertions + 1
    if val ~= nil then
        error(msg or "Assertion failed: expected nil, got " .. dump_val(val), 2)
    end
end

---Asserts that a value is not nil.
---@param val any
---@param msg? string
function M.assert_not_nil(val, msg)
    total_assertions = total_assertions + 1
    if val == nil then
        error(msg or "Assertion failed: expected non-nil value", 2)
    end
end

---Asserts that a string matches a Lua pattern.
---@param pattern string
---@param str string
---@param msg? string
function M.assert_match(pattern, str, msg)
    total_assertions = total_assertions + 1
    if type(str) ~= "string" or not str:match(pattern) then
        local err = string.format("Pattern %q not found in string: %s", pattern, dump_val(str))
        if msg then err = msg .. "\n  " .. err end
        error(err, 2)
    end
end

---Asserts that a function throws an error.
---@param fn fun()
---@param pattern? string
---@param msg? string
function M.assert_error(fn, pattern, msg)
    total_assertions = total_assertions + 1
    local ok, err = pcall(fn)
    if ok then
        error(msg or "Expected function to throw error, but it completed successfully", 2)
    end
    if pattern and not tostring(err):match(pattern) then
        error(string.format("Expected error matching %q, got: %s", pattern, tostring(err)), 2)
    end
end

---Prints summary of the test run and returns exit code (0 on success, 1 on failure).
---@return integer exit_code
function M.summary()
    local elapsed = os.clock() - start_time
    io.write("\n" .. string.rep("-", 60) .. "\n")

    if #failures > 0 then
        io.write(string.format("%s%sFailures (%d):%s\n\n", C.bold, C.red, #failures, C.reset))
        for i, f in ipairs(failures) do
            io.write(string.format("%s%d) [%s] %s%s\n", C.red, i, f.suite, f.name, C.reset))
            io.write(string.format("   %s\n\n", f.err))
        end
    end

    local status_color = failed_tests == 0 and C.green or C.red
    local status_text = failed_tests == 0 and "SUCCESS" or "FAILED"

    io.write(string.format("%s%s%s%s: %d total tests | %s%d passed%s | %s%d failed%s | %d assertions | %.3fs\n",
        C.bold, status_color, status_text, C.reset,
        total_tests,
        C.green, passed_tests, C.reset,
        failed_tests > 0 and C.red or C.dim, failed_tests, C.reset,
        total_assertions,
        elapsed
    ))

    return failed_tests == 0 and 0 or 1
end

return M
