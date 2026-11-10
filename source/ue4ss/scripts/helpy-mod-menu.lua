-- Derived from Maxxfilth's Mod Menu 2.0.2 (MIT), Copyright (c) 2026 Maxxfilth.
-- Adapted for RuneSchema's process bridge; see licenses/ModMenu-MIT.txt.
-- No UMG widgets, input hooks or pause-menu hooks are installed by this adapter.
if type(RuneSchemaModMenuExchange)~="function" then return end
local offline=RuneSchemaModMenuOffline==true
if not offline then
local ok,other=pcall(function() return ModRef:GetSharedVariable("ModMenu.version") end)
if ok and type(other)=="string" and other~="" then
    RuneSchemaModMenuExchange('{"Op":"stop","Reason":"Separate Mod Menu was already loaded. Restart after disabling it."}')
    return
end
if type(ExecuteInGameThreadWithDelay)~="function" then
    RuneSchemaModMenuExchange('{"Op":"stop","Reason":"UE4SS game-thread delayed actions are unavailable"}')
    return
end
end
local function HMencode(value)
    local kind=type(value)
    if kind=="nil" then return "null" end
    if kind=="boolean" then return tostring(value) end
    if kind=="number" then assert(value==value and math.abs(value)<math.huge,"Invalid number");return tostring(value) end
    if kind=="string" then return '"'..value:gsub('[%z\1-\31\\"]',function(c) if c=='"' then return '\\"' elseif c=='\\' then return '\\\\' else return string.format('\\u%04x',c:byte()) end end)..'"' end
    assert(kind=="table","Invalid bridge value")
    local parts={};if #value>0 or (getmetatable(value) or {}).__json_array then for _,v in ipairs(value) do parts[#parts+1]=HMencode(v) end;return "["..table.concat(parts,",").."]" end
    for k,v in pairs(value) do parts[#parts+1]=HMencode(tostring(k))..":"..HMencode(v) end
    return "{"..table.concat(parts,",").."}"
end

-- RuneSchema owns presentation. The licensed backend below retains config
-- coercion, comments, live variables and action/revision compatibility.
local VERSION = "2.0.2"
local MAX_SETTINGS = 1024
local function log(msg) RuneSchemaModMenuExchange(HMencode({Op="log",Text=tostring(msg)})) end

-- JSON --------------------------------------------------------------------------------------------
-- Small strict decoder: objects, arrays, strings (with \uXXXX), numbers, true/false/null.
local json = {}
do
    local function skip(s, i)
        while true do
            local c = s:sub(i, i)
            if c == " " or c == "\t" or c == "\r" or c == "\n" then i = i + 1
            elseif c == "/" and s:sub(i + 1, i + 1) == "/" then   -- tolerate // comments
                local e = s:find("\n", i, true); i = e and e + 1 or #s + 1
            elseif c=="/" and s:sub(i+1,i+1)=="*" then
                local e=s:find("*/",i+2,true);if not e then error("unterminated JSON block comment",0) end;i=e+2
            else return i end
        end
    end
    local function utf8char(cp)
        if cp < 0x80 then return string.char(cp)
        elseif cp < 0x800 then return string.char(0xC0 + math.floor(cp / 0x40), 0x80 + cp % 0x40)
        else return string.char(0xE0 + math.floor(cp / 0x1000), 0x80 + math.floor(cp / 0x40) % 0x40, 0x80 + cp % 0x40) end
    end
    local value
    local function err(s, i, what) error(string.format("%s at character %d", what, i), 0) end
    local function str(s, i)
        local out, j = {}, i + 1
        while true do
            local c = s:sub(j, j)
            if c == "" then err(s, j, "unterminated string") end
            if c == '"' then return table.concat(out), j + 1 end
            if c == "\\" then
                local n = s:sub(j + 1, j + 1)
                local map = { ['"'] = '"', ["\\"] = "\\", ["/"] = "/", b = "\b", f = "\f", n = "\n", r = "\r", t = "\t" }
                if map[n] then out[#out + 1] = map[n]; j = j + 2
                elseif n == "u" then
                    local hex = s:sub(j + 2, j + 5)
                    if not hex:match("^%x%x%x%x$") then err(s, j, "bad \\u escape") end
                    out[#out + 1] = utf8char(tonumber(hex, 16)); j = j + 6
                else err(s, j, "bad escape") end
            else out[#out + 1] = c; j = j + 1 end
        end
    end
    value = function(s, i, depth)
        depth=depth or 0;if depth>32 then err(s,i,"JSON nesting is too deep") end
        i = skip(s, i)
        local c = s:sub(i, i)
        if c == "{" then
            local obj = {}
            i = skip(s, i + 1)
            if s:sub(i, i) == "}" then return obj, i + 1 end
            while true do
                i = skip(s, i)
                if s:sub(i, i) ~= '"' then err(s, i, "expected a quoted key") end
                local k; k, i = str(s, i)
                i = skip(s, i)
                if s:sub(i, i) ~= ":" then err(s, i, "expected ':'") end
                obj[k], i = value(s, i + 1, depth + 1)
                i = skip(s, i)
                local d = s:sub(i, i)
                if d == "}" then return obj, i + 1 end
                if d ~= "," then err(s, i, "expected ',' or '}'") end
                i = skip(s, i + 1)
                if s:sub(i, i) == "}" then return obj, i + 1 end   -- tolerate a trailing comma
            end
        elseif c == "[" then
            local arr = {}
            i = skip(s, i + 1)
            if s:sub(i, i) == "]" then return arr, i + 1 end
            while true do
                arr[#arr + 1], i = value(s, i, depth + 1)
                i = skip(s, i)
                local d = s:sub(i, i)
                if d == "]" then return arr, i + 1 end
                if d ~= "," then err(s, i, "expected ',' or ']'") end
                i = skip(s, i + 1)
                if s:sub(i, i) == "]" then return arr, i + 1 end   -- tolerate a trailing comma
            end
        elseif c == '"' then return str(s, i)
        elseif s:sub(i, i + 3) == "true" then return true, i + 4
        elseif s:sub(i, i + 4) == "false" then return false, i + 5
        elseif s:sub(i, i + 3) == "null" then return nil, i + 4
        else
            local num = s:match("^-?%d+%.?%d*[eE]?[-+]?%d*", i)
            if num and num ~= "" and tonumber(num) then return tonumber(num), i + #num end
            err(s, i, "unexpected '" .. c .. "'")
        end
    end
    function json.decode(s)
        s = s:gsub("^\239\187\191", "")                       -- UTF-8 BOM
        local ok, v, i = pcall(value, s, 1)
        if not ok then return nil, v end
        i = skip(s, i)
        if i <= #s then return nil, "extra text after the end at character " .. i end
        return v
    end
    local function esc(str)
        return '"' .. str:gsub('[%c"\\]', function(ch)
            local m = { ['"'] = '\\"', ["\\"] = "\\\\", ["\n"] = "\\n", ["\r"] = "\\r", ["\t"] = "\\t" }
            return m[ch] or string.format("\\u%04x", ch:byte())
        end) .. '"'
    end
    function json.encodeFlat(obj, order)
        local lines = {}
        for _, k in ipairs(order) do
            local v = obj[k]
            local out
            if type(v) == "boolean" then out = tostring(v)
            elseif type(v) == "number" then out = tostring(v)
            else out = esc(tostring(v)) end
            lines[#lines + 1] = "  " .. esc(k) .. ": " .. out
        end
        return "{\n" .. table.concat(lines, ",\n") .. "\n}\n"
    end
end

-- Files -------------------------------------------------------------------------------------------
local baselines = {}
local function exchange(op, request)
    request=request or {};request.Op=op
    local reply,issue=json.decode(RuneSchemaModMenuExchange(HMencode(request)))
    if not reply or reply.Error then error(reply and reply.Error or issue or "Mod Menu bridge unavailable",0) end
    return reply
end
local function readFile(path)
    local reply=exchange("read",{Path=path});baselines[path]=reply
    return reply.Exists and reply.Text or nil
end
local function writeFile(path,text,jsonPatch)
    local previous=baselines[path];if not previous then return false end
    local reply=exchange("write",{Path=path,Exists=previous.Exists,Expected=previous.Text,Text=text,JsonPatch=jsonPatch})
    if reply.Saved then return true end
    return false
end

-- Schemas -----------------------------------------------------------------------------------------
-- The schema file has two accepted names. CurseForge's "RSDragonwilds UE4SS Mods" category only
-- accepts .txt/.lua/.dll/.pak/.utoc/.ucas and rejects the whole archive over a .json, so builds for
-- that site ship the identical file as modmenu.txt. The contents are JSON either way; only the name
-- differs. .json is tried first so an existing install keeps loading exactly the file it always did.
local SCHEMA_FILES = { "modmenu.json", "modmenu.txt" }

local TYPES = { header = true, toggle = true, slider = true, number = true, choice = true, key = true, text = true, button = true }
-- 1.1: where a key works. Two settings on one key only clash when they share a place, or one is "anywhere".
local CONTEXTS = { world = true, chest = true, build = true, menus = true, anywhere = true }
local mods = {}          -- sorted list of mod entries
local modsById = {}

-- "1.0.15" < "1.1.0", compared number by number.
local function verLess(a, b)
    local pa, pb = {}, {}
    for d in tostring(a):gmatch("%d+") do pa[#pa + 1] = tonumber(d) end
    for d in tostring(b):gmatch("%d+") do pb[#pb + 1] = tonumber(d) end
    for i = 1, math.max(#pa, #pb) do
        local x, y = pa[i] or 0, pb[i] or 0
        if x ~= y then return x < y end
    end
    return false
end

local function round(v, decimals)
    local m = 10 ^ (decimals or 0)
    return math.floor(v * m + 0.5) / m
end

local function decimalsFor(s)
    if s.decimals then return math.max(0, math.min(4, math.floor(s.decimals))) end
    local step = s.step or 1
    if step >= 1 then return 0 end
    local d = 0
    while d < 4 and math.abs(step * 10 ^ d - math.floor(step * 10 ^ d + 0.5)) > 1e-9 do d = d + 1 end
    return d
end

-- Validates one setting. Returns a cleaned copy or nil plus a reason.
local function checkSetting(raw, idx, seen, restartDefault)
    if type(raw) ~= "table" then return nil, "setting " .. idx .. " is not an object" end
    local t = raw.type
    if type(t) ~= "string" or not TYPES[t] then return nil, "setting " .. idx .. ": unknown type '" .. tostring(t) .. "'" end
    if t == "number" then t = "slider" end
    local s = { type = t, label = type(raw.label) == "string" and raw.label:sub(1, 80) or nil,
                tooltip = type(raw.tooltip) == "string" and raw.tooltip:sub(1, 300) or nil }
    -- 2.0 (design 2): optional fields; 1.x copies known fields only, so a file using them still loads there.
    -- runsOn = where the value counts; a word it does not know is left out (no tag).
    local ro = raw.runsOn
    if ro == "yours" or ro == "host" or ro == "server" or ro == "both" then s.runsOn = ro end
    if t == "header" then
        s.label = s.label or ""
        s.idx = idx                                         -- 2.0: the fold state is kept per header
        if raw.collapsed == true then s.collapsed = true end   -- C10: anything but true = the section starts open
        return s
    end
    -- 2.0: showIf = { key = <a key in this file>, is = value or list }; checked against the file in loadSchema
    if type(raw.showIf) == "table" and type(raw.showIf.key) == "string" and raw.showIf.is ~= nil then
        s.showIf = { key = raw.showIf.key, is = raw.showIf.is }
    end
    if t == "button" then
        if type(raw.action) ~= "string" or raw.action == "" then return nil, "button " .. idx .. " needs an \"action\"" end
        s.action = raw.action:sub(1, 60)
        s.label = s.label or s.action
        return s
    end
    if type(raw.key) ~= "string" or not raw.key:match("^[%w_%.%-]+$") then
        return nil, "setting " .. idx .. " needs a \"key\" made of letters, digits, _ . -"
    end
    if seen[raw.key] then return nil, "setting " .. idx .. ": key '" .. raw.key .. "' is used twice" end
    s.key = raw.key
    s.label = s.label or raw.key
    if raw.restart == nil then s.restart = restartDefault == true else s.restart = raw.restart == true end
    if t == "toggle" then
        s.default = raw.default == true
    elseif t == "slider" then
        local mn, mx = tonumber(raw.min), tonumber(raw.max)
        if not mn or not mx or mn >= mx then return nil, "slider '" .. s.key .. "' needs min < max" end
        s.min, s.max = mn, mx
        s.step = tonumber(raw.step)
        if not s.step or s.step <= 0 then s.step = (mx - mn) / 100 end
        s.decimals = decimalsFor({ decimals = tonumber(raw.decimals), step = s.step })
        s.suffix = type(raw.suffix) == "string" and raw.suffix:sub(1, 8) or ""
        local d = tonumber(raw.default) or mn
        s.default = math.max(mn, math.min(mx, d))
    elseif t == "choice" then
        if type(raw.options) ~= "table" or #raw.options < 1 then return nil, "choice '" .. s.key .. "' needs an \"options\" list" end
        s.options = {}
        for _, o in ipairs(raw.options) do
            if type(o) == "string" or type(o) == "number" then s.options[#s.options + 1] = tostring(o):sub(1, 40) end
            if #s.options >= 50 then break end
        end
        if #s.options == 0 then return nil, "choice '" .. s.key .. "' has no usable options" end
        s.default = tostring(raw.default or s.options[1])
        local found = false
        for _, o in ipairs(s.options) do if o == s.default then found = true end end
        if not found then s.default = s.options[1] end
    elseif t == "key" then
        s.default = type(raw.default) == "string" and raw.default or "none"
        local c = type(raw.context) == "string" and raw.context:lower() or "anywhere"
        s.context = CONTEXTS[c] and c or "anywhere"
        -- 1.1.3: "keyNames": "unreal" = the mod binds Unreal key names itself (Key Unlocker), so keys UE4SS has no
        -- name for (Shift, Ctrl, Alt, the mouse wheel) are saved as the Unreal name instead of refused.
        if raw.keyNames == "unreal" then s.keyNames = "unreal" end
    elseif t == "text" then
        s.maxLength = math.max(1, math.min(200, tonumber(raw.maxLength) or 60))
        s.default = type(raw.default) == "string" and raw.default:sub(1, s.maxLength) or ""
    end
    -- 2.0: valueLabels = { "<value as formatValue prints it>" = "word" } for sliders, choices and toggles (C10)
    if type(raw.valueLabels) == "table" and (t == "slider" or t == "choice" or t == "toggle") then
        local vl, any = {}, false
        for k, v in pairs(raw.valueLabels) do
            if type(k) == "string" and type(v) == "string" then vl[k] = v:sub(1, 40); any = true end
        end
        if any then s.valueLabels = vl end
    end
    seen[s.key] = true
    return s
end

local function validColor(value, alpha)
    return type(value)=="string" and #value==(alpha and 9 or 7) and value:match("^#%x+$")~=nil
end

-- Converts a stored string (or JSON value) into the setting's type. nil = unusable.
local function coerce(s, v)
    if v == nil then return nil end
    if s.presentation == "color" then return validColor(v,s.colorAlpha) and v or nil end
    if s.type == "toggle" then
        if type(v) == "boolean" then return v end
        local l = tostring(v):lower()
        if l == "true" or l == "1" or l == "yes" or l == "on" then return true end
        if l == "false" or l == "0" or l == "no" or l == "off" then return false end
        return nil
    elseif s.type == "slider" then
        -- 1.1: the number in the file is kept as it is, also outside min/max (a player's MaxResults = 200 on a
        -- 0-100 slider was cut to 100 and sent to the mod). Only moving the slider replaces it (panelTick).
        local n = tonumber(v)
        if not n or n ~= n or n == math.huge or n == -math.huge then return nil end
        return n
    elseif s.type == "choice" then
        v = tostring(v)
        for _, o in ipairs(s.options) do if o:lower() == v:lower() then return o end end
        return nil
    elseif s.type == "key" or s.type == "text" then
        v = tostring(v):gsub("[\r\n]", " ")
        if s.maxLength then v = v:sub(1, s.maxLength) end
        return v
    end
    return nil
end

local function formatValue(s, v)
    if s.type == "toggle" then return v and "true" or "false" end
    if s.type == "slider" then
        if s.decimals == 0 then return tostring(math.floor(v + 0.5)) end
        local str = string.format("%." .. s.decimals .. "f", v)
        return (str:gsub("0+$", ""):gsub("%.$", ""))
    end
    return tostring(v)
end

-- Config files ------------------------------------------------------------------------------------
local function stripComment(v)
    -- "value   # note" -> "value". A # or ; only starts a comment after whitespace.
    local cut = v:find("%s[#;]")
    if cut then return (v:sub(1, cut - 1):gsub("%s+$", "")), v:sub(cut) end
    return (v:gsub("%s+$", "")), ""
end

-- Lua files: settings written as plain assignments near the top of the script, e.g.
--     local MaxStack = 999          -- comment kept
--     ShowIcons = true,             (a field inside a settings table)
-- Only the first assignment to each name, with a literal number, true/false or quoted string, is used.
local function luaLiteral(v)
    local q = v:sub(1, 1)
    if q == '"' or q == "'" then
        local i = 2
        while i <= #v do
            local ch = v:sub(i, i)
            if ch == "\\" then i = i + 2
            elseif ch == q then
                local body = v:sub(2, i - 1):gsub("\\(.)", "%1")
                return body, v:sub(i + 1), q
            else i = i + 1 end
        end
        return nil
    end
    local b = v:match("^true") or v:match("^false")
    if b then return b == "true", v:sub(#b + 1) end
    local n = v:match("^%-?%d+%.?%d*")
    if n and tonumber(n) then return tonumber(n), v:sub(#n + 1) end
    return nil
end

local function luaLine(line)
    local indent, loc, name, eq, rest = line:match("^(%s*)(local%s+)([%a_][%w_]*)(%s*=%s*)(.-)%s*$")
    if not indent then
        indent, name, eq, rest = line:match("^(%s*)([%a_][%w_]*)(%s*=%s*)(.-)%s*$")
        loc = ""
    end
    if not indent or #indent > 8 then return nil end
    local val, tail, q = luaLiteral(rest)
    if val == nil then return nil end
    if not (tail:match("^%s*,?%s*$") or tail:match("^%s*,?%s*%-%-")) then return nil end
    return { pre = indent .. loc .. name .. eq, name = name, value = val, tail = tail, quote = q }
end

local function readConfig(m)
    local text = readFile(m.cfgPath)
    local found = {}
    if not text then return found end
    if m.format == "json" then
        local obj = json.decode(text)
        if type(obj) == "table" then found = obj
        else m.errors[#m.errors + 1] = "config file is not valid JSON; defaults shown" end
        return found
    end
    if m.format == "lua" then
        for line in (text .. "\n"):gmatch("([^\n]*)\n") do
            local l = luaLine((line:gsub("\r$", "")))
            if l and found[l.name] == nil then found[l.name] = l.value end
        end
        for _, st in ipairs(m.settings) do
            if st.key and found[st.key] == nil then
                m.errors[#m.errors + 1] = "'" .. st.key .. "' is not a plain value in " .. (m.cfgPath:match("[^\\]+$") or "the Lua file") .. ", so it cannot be changed here"
                st.locked = true
            end
        end
        return found
    end
    for line in (text .. "\n"):gmatch("([^\n]*)\n") do
        line = line:gsub("\r$", "")
        local k, v = line:match("^%s*([%w_%.%-]+)%s*=%s*(.-)%s*$")
        if k then found[k] = (stripComment(v)) end
    end
    return found
end

local function loadValues(m)
    local found = readConfig(m)
    m.values = {}
    for _, s in ipairs(m.settings) do
        if s.key then
            local v = coerce(s, found[s.key])
            if v == nil then v = s.default end
            m.values[s.key] = v
        end
    end
end

local function saveValues(m)
    local before=baselines[m.cfgPath]
    local text=before and before.Exists and before.Text or nil
    if m.format == "json" then
        if offline then return writeFile(m.cfgPath,HMencode(m.values),true) end
        local obj = (text and json.decode(text)) or {}
        if type(obj) ~= "table" then obj = {} end
        local order, seenK = {}, {}
        for _, s in ipairs(m.settings) do
            if s.key then obj[s.key] = m.values[s.key]; order[#order + 1] = s.key; seenK[s.key] = true end
        end
        for k in pairs(obj) do if not seenK[k] and type(obj[k]) ~= "table" then order[#order + 1] = k end end
        return writeFile(m.cfgPath, json.encodeFlat(obj, order))
    end
    text = text or ""
    local nl = text:find("\r\n", 1, true) and "\r\n" or "\n"
    if m.format == "lua" then
        if text == "" then return false end
        local out, doneL, byKeyL = {}, {}, {}
        for _, st in ipairs(m.settings) do if st.key and not st.locked then byKeyL[st.key] = st end end
        for line in (text .. "\n"):gmatch("([^\n]*)\n") do
            line = line:gsub("\r$", "")
            local l = luaLine(line)
            local st = l and byKeyL[l.name]
            if st and not doneL[l.name] and coerce(st, l.value) == m.values[l.name] then
                doneL[l.name] = true                        -- 1.1: unchanged, the line stays as it was typed
            elseif st and not doneL[l.name] then
                local v = m.values[l.name]
                local lit
                if type(v) == "boolean" then lit = tostring(v)
                elseif type(v) == "number" then lit = formatValue(st, v)
                else
                    local q = l.quote or '"'
                    lit = q .. (tostring(v):gsub("\\", "\\\\"):gsub(q, "\\" .. q)) .. q
                end
                line = l.pre .. lit .. l.tail
                doneL[l.name] = true
            end
            out[#out + 1] = line
        end
        while #out > 0 and out[#out] == "" do out[#out] = nil end
        return writeFile(m.cfgPath, table.concat(out, nl) .. nl)
    end
    local lines, done = {}, {}
    local byKey = {}
    for _, s in ipairs(m.settings) do if s.key then byKey[s.key] = s end end
    for line in (text .. "\n"):gmatch("([^\n]*)\n") do
        line = line:gsub("\r$", "")
        local indent, k, v = line:match("^(%s*)([%w_%.%-]+)%s*=%s*(.-)%s*$")
        local s = k and byKey[k]
        if s and not done[k] and coerce(s, (stripComment(v))) == m.values[k] then
            done[k] = true                                  -- 1.1: unchanged, the line stays as it was typed
        elseif s and not done[k] then
            local _, comment = stripComment(v)
            line = indent .. k .. " = " .. formatValue(s, m.values[k]) .. (comment ~= "" and ("   " .. comment:gsub("^%s+", "")) or "")
            done[k] = true
        end
        lines[#lines + 1] = line
    end
    while #lines > 0 and lines[#lines] == "" do lines[#lines] = nil end
    -- Settings missing from the file are only added when they differ from the default, so files stay tidy.
    local header = text:find("# Set in Mod Menu", 1, true) ~= nil
    for _, s in ipairs(m.settings) do
        if s.key and not done[s.key] and m.values[s.key] ~= s.default then
            if not header then lines[#lines + 1] = ""; lines[#lines + 1] = "# Set in Mod Menu"; header = true end
            lines[#lines + 1] = s.key .. " = " .. formatValue(s, m.values[s.key])
        end
    end
    return writeFile(m.cfgPath, table.concat(lines, nl) .. nl)
end

-- Shared variables --------------------------------------------------------------------------------
-- 1.1: a value is sent only when it differs from what Mod Menu sent last. Before, every menu open sent every
-- value of every mod again.
local sent = {}
local function share(name, v)
    if offline then return end
    if v ~= nil and sent[name] == v then return end
    local ok, e = pcall(function() ModRef:SetSharedVariable(name, v) end)
    if ok then sent[name] = v
    else log("could not publish " .. name .. ": " .. tostring(e)) end
end

local function publishAll(m)
    for _, s in ipairs(m.settings) do
        if s.key then share("ModMenu." .. m.id .. "." .. s.key, m.values[s.key]) end
    end
    share("ModMenu." .. m.id .. ".rev", m.rev)
end

-- Discovery ---------------------------------------------------------------------------------------
-- 1.1: is the mod switched on? UE4SS starts a mod whose folder has an enabled.txt, or that has a
-- "<folder> : 1" line in mods.txt. A page is drawn from modmenu.json either way, so say when neither is there.
local modsTxtCache = {}  -- Mods folder -> { [folder name, lower case] = "0" or "1" }; emptied on every discover()
local function modsTxtFor(root)
    local t = modsTxtCache[root]
    if t then return t end
    t = {}
    local text = readFile(root .. "\\mods.txt")
    for line in (((text or ""):gsub("^\239\187\191", "")) .. "\n"):gmatch("([^\n]*)\n") do
        local name, on = line:match("^%s*([^;:][^:]-)%s*:%s*(%d)")
        if name then t[name:lower()] = on end
    end
    modsTxtCache[root] = t
    return t
end

local function switchedOff(folder, dir, runeSchema)
    if runeSchema then return nil end -- RuneSchema presents only packages of enabled parents.
    if readFile(dir .. "\\enabled.txt") then return nil end
    local root = dir:match("^(.*)[/\\][^/\\]*$")
    if not root then return nil end
    if modsTxtFor(root)[folder:lower()] == "1" then return nil end
    return "This mod is switched off: its folder has no enabled.txt and mods.txt does not set it to 1. "
        .. "Changes made here are saved, but do nothing until the mod is switched on."
end

local function templateContext(m)
    local definition=m.helpy and m.helpy.templates
    local variable=definition and definition.contextVariable
    if not variable then return "" end
    if offline then return nil end
    local ok,value=pcall(function()return ModRef:GetSharedVariable(variable)end)
    if ok and type(value)=="string" and #value>0 and #value<=256 then return value end
    return nil
end

local function loadSchema(folderName, dirPath)
    local m = { folder = folderName, dir = dirPath, errors = {}, settings = {}, values = {}, rev = 0, actionN = 0 }
    local path = dirPath:gsub("\\", "/"):gsub("/+$", ""):lower()
    local owner, name = path:match("/mods/runeschema/mods/([^/]+)/ue4ss/([^/]+)$")
    if not owner then owner = path:match("/mods/runeschema/mods/([^/]+)/ue4ss$"); name = owner end
    if owner and name == folderName:lower() then m.runeSchema = { owner = owner, name = name } end
    m.off = switchedOff(folderName, dirPath, m.runeSchema)
    local text, ferr = nil, nil
    for _, fname in ipairs(SCHEMA_FILES) do
        text, ferr = readFile(dirPath .. "\\" .. fname)
        if text then break end
    end
    if not text then m.errors[#m.errors + 1] = ferr or "modmenu.json could not be read"; text = nil end
    local doc, jerr = nil, nil
    if text then doc, jerr = json.decode(text) end
    if text and type(doc) ~= "table" then
        m.errors[#m.errors + 1] = "modmenu.json is not valid JSON: " .. tostring(jerr)
        doc = {}
    end
    doc = doc or {}
    m.id = (type(doc.id) == "string" and doc.id:match("^[%w_%-]+$")) and doc.id or folderName
    m.name = type(doc.name) == "string" and doc.name:sub(1, 40) or folderName
    m.author = type(doc.author) == "string" and doc.author:sub(1, 40) or nil
    m.version = (type(doc.version) == "string" or type(doc.version) == "number") and tostring(doc.version):sub(1, 20) or nil
    m.description = type(doc.description) == "string" and doc.description:sub(1, 300) or nil
    local extension=doc["--Helpy"]
    if extension~=nil then
        if type(extension)~="table" or extension.schema~=1 then
            m.errors[#m.errors+1]="Unsupported --Helpy extension schema; legacy settings remain available"
            extension=nil
        end
    elseif type(doc.helpy)=="table" then extension=doc.helpy end
    if type(extension)=="table" then
        m.helpy={layout=extension.layout=="character" and "character" or "settings",tabs={}}
        if type(extension.tabs)=="table" then for i,tab in ipairs(extension.tabs) do
            if i>16 then break end
            if type(tab)=="table" and type(tab.name)=="string" and type(tab.keys)=="table" then
                local keys={};for n,key in ipairs(tab.keys) do if n>MAX_SETTINGS then break end;if type(key)=="string" and key:match("^[%w_.%-]+$") then keys[#keys+1]=key end end
                m.helpy.tabs[#m.helpy.tabs+1]={name=tab.name:sub(1,80),keys=keys}
            end
        end end
        local p=extension.preview
        if type(p)=="table" and type(p.channelVariable)=="string" and #p.channelVariable<=192 and p.channelVariable:match("^[%w_.%-]+$") then m.helpy.preview={channelVariable=p.channelVariable} end
    end
    local ro = doc.runsOn                                      -- 2.0: the mod's own runsOn word (a setting's wins)
    if ro == "yours" or ro == "host" or ro == "server" or ro == "both" then m.runsOn = ro end
    if type(doc.minModMenu) == "string" and doc.minModMenu:match("^%d+[%d%.]*$") and verLess(VERSION, doc.minModMenu) then
        m.errors[#m.errors + 1] = "this page was made for Mod Menu " .. doc.minModMenu:sub(1, 12) .. " or newer and you have "
            .. VERSION .. "; update Mod Menu to see every setting"
    end
    local c = type(doc.config) == "table" and doc.config or {}
    local file = type(c.file) == "string" and c.file or "config.txt"
    if file:find("%.%.") or file:find("^[/\\]") or file:find(":") then
        m.errors[#m.errors + 1] = "config file must be inside the mod folder"
        file = "config.txt"
    end
    m.format = (c.format == "json" or c.format == "lua") and c.format or "keyvalue"
    if m.format == "lua" and type(c.file) ~= "string" then file = "Scripts/main.lua" end
    m.cfgPath = dirPath .. "\\" .. file:gsub("/", "\\")
    local seen = {}
    if type(doc.settings) == "table" then
        for i, raw in ipairs(doc.settings) do
            if i > MAX_SETTINGS then m.errors[#m.errors + 1] = "only the first "..MAX_SETTINGS.." settings are shown"; break end
            local s, why = checkSetting(raw, i, seen, m.format == "lua")
            if s then m.settings[#m.settings + 1] = s else m.errors[#m.errors + 1] = why end
        end
    elseif text then
        m.errors[#m.errors + 1] = "modmenu.json has no \"settings\" list"
    end
    -- Only Helpy reads this namespace. The original menu sees valid JSON and ignores it.
    if type(extension)=="table" and type(extension.settings)=="table" then
        for _,raw in ipairs(extension.settings) do
            if #m.settings>=MAX_SETTINGS then m.errors[#m.errors+1]="Only "..MAX_SETTINGS.." combined settings are supported";break end
            local copy={};if type(raw)=="table" then for key,value in pairs(raw) do copy[key]=value end end
            local widget=copy.type;copy.type=widget=="dial" and "slider" or widget=="radial" and "choice" or widget=="color" and "text" or copy.type
            local alpha=copy.alpha==true
            if widget=="color" then
                if copy.default==nil then copy.default=alpha and "#FFFFFFFF" or "#FFFFFF" end
                copy.maxLength=alpha and 9 or 7
            end
            local setting,why
            if widget=="color" and ((copy.alpha~=nil and type(copy.alpha)~="boolean") or not validColor(copy.default,alpha)) then why="Color settings require a #RRGGBB default, or #RRGGBBAA with alpha=true"
            else setting,why=checkSetting(copy,#m.settings+1,seen,m.format=="lua") end
            if setting then
                if widget=="dial" or (widget=="radial" and #setting.options<=12) then setting.presentation=widget
                elseif widget=="radial" then m.errors[#m.errors+1]="Radial choices support at most 12 options; using the standard choice control"
                elseif widget=="color" then setting.presentation="color";setting.colorAlpha=alpha end
                m.settings[#m.settings+1]=setting
            else m.errors[#m.errors+1]=why end
        end
    end
    local byKey={};for _,setting in ipairs(m.settings)do if setting.key then byKey[setting.key]=setting end end
    if type(extension)=="table" and type(extension.controls)=="table" then
        for key,widget in pairs(extension.controls)do
            local setting=byKey[key]
            if setting and widget=="color" and setting.type=="text" and validColor(setting.default,#setting.default==9) then setting.presentation="color";setting.colorAlpha=#setting.default==9;setting.maxLength=setting.colorAlpha and 9 or 7
            elseif setting and ((widget=="dial" and setting.type=="slider") or (widget=="radial" and setting.type=="choice" and #setting.options<=12)) then setting.presentation=widget
            else m.errors[#m.errors+1]="Invalid Helpy presentation for "..tostring(key) end
        end
    end
    if type(extension)=="table" and type(extension.templates)=="table" then
        local raw=extension.templates;local keys,used={},{};local valid=type(raw.id)=="string" and #raw.id<=80 and raw.id:match("^[%w_.%-]+$") and type(raw.version)=="number" and raw.version>=1 and raw.version<=65535 and raw.version%1==0 and type(raw.keys)=="table" and #raw.keys>=1 and #raw.keys<=MAX_SETTINGS
        if valid then for _,key in ipairs(raw.keys)do local setting=byKey[key];if type(key)~="string" or used[key] or not setting or setting.type=="key" then valid=false;break end;used[key]=true;keys[#keys+1]=key end end
        local context=raw.contextVariable
        if context~=nil and (type(context)~="string" or #context>192 or not context:match("^[%w_.%-]+$")) then valid=false end
        if valid then m.helpy.templates={id=raw.id,version=raw.version,keys=keys,contextVariable=context}
        else m.errors[#m.errors+1]="Invalid Helpy template contract; template import/export disabled" end
    end
    -- 2.0 (C10): a showIf naming no key of this file, or its own row, is dropped: that row always shows
    local keys = {}
    for _, st in ipairs(m.settings) do if st.key then keys[st.key] = true end end
    for _, st in ipairs(m.settings) do
        if st.showIf and (not keys[st.showIf.key] or st.showIf.key == st.key) then st.showIf = nil end
    end
    loadValues(m)
    m.templateContext=templateContext(m)
    return m
end


local openN,openId,openRow,lastStatus,lastOpen,lastSet=0,"","","","",""
local running,hostAuthority=true,offline
local discoveryWarning=""
local function publish()
    local pages=setmetatable({},{__json_array=true})
    for _,m in ipairs(mods) do
        pages[#pages+1]={Id=m.id,Name=m.name,Author=m.author,Version=m.version,Description=m.description,
            Settings=m.settings,Values=m.values,Errors=m.errors,Off=m.off,RunsOn=m.runsOn,Revision=m.rev,Helpy=m.helpy,PreviewChannel=m.previewChannel,TemplateContext=m.templateContext}
        share("ModMenu.health."..m.id,m.off and "off" or "unknown")
    end
    exchange("publish",{Snapshot={Pages=pages,OpenSequence=openN,OpenId=openId,OpenRow=openRow,Status=lastStatus,DiscoveryWarning=discoveryWarning}})
end
local function refresh()
    local catalog=exchange("catalog");local list,byId={},{}
    discoveryWarning=catalog.Warning or ""
    for _,entry in ipairs(catalog.Pages or {}) do
        local ok,m
        if not entry.Error then ok,m=pcall(loadSchema,entry.Folder,entry.Path) end
        if not ok then
            m={id=entry.Folder,name=entry.Folder,folder=entry.Folder,dir=entry.Path,settings=setmetatable({},{__json_array=true}),values={},errors={entry.Error or tostring(m)},rev=0,actionN=0,unavailable=true}
        end
        if byId[m.id] then
            local base=m.id.."_"..m.folder;local index=1;m.id=base
            while byId[m.id] do index=index+1;m.id=base.."_"..index end
            m.errors[#m.errors+1]="Duplicate Mod Menu id; this page uses "..m.id
        end
        local previous=modsById[m.id];if previous and previous.dir==m.dir then m.rev=previous.rev;m.actionN=previous.actionN end
        byId[m.id]=m;list[#list+1]=m
    end
    table.sort(list,function(a,b)return a.name:lower()<b.name:lower() end);mods,modsById=list,byId
    for _,m in ipairs(mods) do if not m.unavailable then publishAll(m) end end
end
local function change(id,key,value,expected,reset,action)
    local previous=assert(modsById[id],"Mod Menu page is unavailable")
    assert(not previous.unavailable,"Refresh after repairing this mod's menu schema")
    local m=loadSchema(previous.folder,previous.dir);m.id=id;m.rev=previous.rev;m.actionN=previous.actionN;modsById[id]=m
    for i,current in ipairs(mods) do if current.id==id then mods[i]=m;break end end
    if action then
        for _,s in ipairs(m.settings) do if s.type=="button" and s.action==action then
            local role=s.runsOn or m.runsOn;assert(hostAuthority or (role~="host" and role~="server"),"The host controls this setting")
            m.actionN=m.actionN+1;share("ModMenu."..m.id..".action",action.."#"..m.actionN);return
        end end
        error("Unknown Mod Menu action")
    end
    local found=false
    for _,s in ipairs(m.settings) do if s.key and not s.locked and (reset or s.key==key) then
        local role=s.runsOn or m.runsOn;assert(hostAuthority or (role~="host" and role~="server"),"The host controls this setting")
        if not reset and expected~=nil and m.values[s.key]~=expected then error("This setting changed outside Helpy; refresh before editing") end
        local nextValue=reset and s.default or coerce(s,value);assert(nextValue~=nil,"Invalid setting value")
        m.values[s.key]=nextValue;found=true
    end end
    assert(found,"Unknown or locked Mod Menu setting")
    assert(saveValues(m),"Config changed outside Helpy or could not be saved; original retained")
    m.rev=m.rev+1;publishAll(m)
end
local function applyTemplate(c)
    local previous=assert(modsById[c.Id],"Mod Menu page is unavailable")
    assert(not previous.unavailable,"Refresh after repairing this mod's menu schema")
    local m=loadSchema(previous.folder,previous.dir);m.id=c.Id;m.rev=previous.rev;m.actionN=previous.actionN
    local definition=assert(m.helpy and m.helpy.templates,"This mod has not declared template support")
    assert(c.TemplateId==definition.id and c.TemplateVersion==definition.version,"Template contract changed; refresh the preview")
    assert(c.Revision==m.rev,"This page changed; review the template again")
    local context=templateContext(m);assert(context~=nil and c.Context==context,"Selected character or target changed; review the template again")
    assert(type(c.Values)=="table" and type(c.Expected)=="table","Invalid template values")
    local byKey,allowed,plan={},{},{}
    for _,setting in ipairs(m.settings)do if setting.key then byKey[setting.key]=setting end end
    for _,key in ipairs(definition.keys)do allowed[key]=true end
    for key in pairs(c.Values)do assert(allowed[key],"Template contains an undeclared setting")end
    for key in pairs(c.Expected)do assert(allowed[key],"Template contains an undeclared baseline")end
    for _,key in ipairs(definition.keys)do
        local setting=assert(byKey[key],"Template setting is unavailable");assert(not setting.locked,"Template includes a locked setting")
        local role=setting.runsOn or m.runsOn;assert(hostAuthority or (role~="host" and role~="server"),"The host controls this template setting")
        assert(c.Expected[key]~=nil and m.values[key]==c.Expected[key],"A setting changed outside Helpy; review the template again")
        local value=c.Values[key];local valid=false
        if setting.presentation=="color" then valid=validColor(value,setting.colorAlpha)
        elseif setting.type=="toggle" then valid=type(value)=="boolean"
        elseif setting.type=="slider" then valid=type(value)=="number" and value==value and value~=math.huge and value~=-math.huge and value>=setting.min and value<=setting.max
        elseif setting.type=="choice" then if type(value)=="string" then for _,option in ipairs(setting.options)do if value==option then valid=true;break end end end
        elseif setting.type=="text" then valid=type(value)=="string" and #value<=setting.maxLength and not value:find("[%z\r\n]") end
        assert(valid,"Template contains an invalid value for "..key);plan[key]=value
    end
    for key,value in pairs(plan)do m.values[key]=value end
    assert(saveValues(m),"Config changed outside Helpy or could not be saved; original retained")
    m.rev=m.rev+1;modsById[m.id]=m;for i,current in ipairs(mods)do if current.id==m.id then mods[i]=m;break end end
    publishAll(m)
end
local function command(c)
    if c.Op=="template_apply" then applyTemplate(c)
    elseif c.Op=="refresh" then refresh()
    elseif c.Op=="set" then change(c.Id,c.Key,c.Value,c.Expected)
    elseif c.Op=="reset" then change(c.Id,nil,nil,nil,true)
    elseif c.Op=="button" then change(c.Id,nil,nil,nil,false,c.Action)
    elseif c.Op=="page" then share("ModMenu.page",c.Id or "");share("ModMenu.rect",c.Rect or "");share("ModMenu.quick","") end
end
if offline then
    refresh()
    publish()
    return function(input)
        local c,issue=json.decode(input)
        if not c then error(issue or "Invalid Mod Menu command") end
        assert(c.Op=="set" or c.Op=="reset" or c.Op=="refresh" or c.Op=="template_apply","This action requires a running game.")
        local ok,problem=pcall(command,c)
        if not ok then refresh() end
        lastStatus=ok and (c.Op=="refresh" and "" or "Saved for the next game launch") or tostring(problem)
        publish()
    end
end
local function tick()
    local version=ModRef:GetSharedVariable("ModMenu.version")
    if version~=nil and version~="" and version~=VERSION then running=false;exchange("stop",{Reason="Another Mod Menu published its runtime version; restart after disabling it"});return end
    local pending=exchange("next");if not pending.Active then running=false;pcall(function()ModRef:SetSharedVariable("ModMenu.version",nil);ModRef:SetSharedVariable("ModMenu.page","");ModRef:SetSharedVariable("ModMenu.rect","");ModRef:SetSharedVariable("ModMenu.quick","")end);return end
    hostAuthority=pending.HostAuthority==true
    if pending.Command then local ok,issue=pcall(command,pending.Command);lastStatus=ok and "Saved" or tostring(issue);publish() end
    local requested=ModRef:GetSharedVariable("ModMenu.request.open")
    if type(requested)=="string" and requested~="" and requested~=lastOpen then
        lastOpen=requested;local id=requested:match("^([^#|]+)");if id and modsById[id] then openN=openN+1;openId=id;openRow=requested:match("|row=([%w_.%-]+)") or "";publish() end
    end
    requested=ModRef:GetSharedVariable("ModMenu.request.set")
    if type(requested)=="string" and requested~="" and requested~=lastSet then
        lastSet=requested;local id=requested:match("^([^#|]+)")
        if id and modsById[id] then for key,value in requested:gmatch("|([%w_.-]+)=([^|]*)") do
            for _,s in ipairs(modsById[id].settings) do if s.key==key and s.type~="key" then local ok,issue=pcall(change,id,key,value);if not ok then lastStatus=tostring(issue) end;break end end
        end;publish() end
    end
    local m=modsById[sent["ModMenu.page"]]
    if m and m.helpy and m.helpy.templates then local context=templateContext(m);if context~=m.templateContext then m.templateContext=context;publish()end end
    if m and m.helpy and type(m.helpy.preview)=="table" and type(m.helpy.preview.channelVariable)=="string" then
        local name=m.helpy.preview.channelVariable;if #name<=192 then local channel=ModRef:GetSharedVariable(name);if type(channel)=="string" and #channel<=128 and channel~=m.previewChannel then m.previewChannel=channel;publish() end end
    end
end
local step
local function schedule(delay)
    local ok,issue=pcall(ExecuteInGameThreadWithDelay,delay,step)
    if not ok then
        running=false
        pcall(exchange,"stop",{Reason="UE4SS game-thread scheduling failed: "..tostring(issue)})
        pcall(function()ModRef:SetSharedVariable("ModMenu.version",nil);ModRef:SetSharedVariable("ModMenu.page","");ModRef:SetSharedVariable("ModMenu.rect","");ModRef:SetSharedVariable("ModMenu.quick","")end)
    end
end
step=function()
    if not running then return end
    local ok,issue=pcall(tick);if not ok then lastStatus=tostring(issue);pcall(publish) end
    if running then schedule(sent["ModMenu.page"]~="" and 100 or 1000) end
end
refresh();share("ModMenu.version",VERSION);share("ModMenu.page","");share("ModMenu.rect","");share("ModMenu.quick","");publish()
schedule(1000)
