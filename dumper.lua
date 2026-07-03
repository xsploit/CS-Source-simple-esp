-- CS:S v93 x64 External Netvar Dumper (CE Lua)
-- ===============================================
-- Paste into CE Lua Engine (Ctrl+L), Execute. No Python, no relay.
-- Writes netvar_dump.txt to your Desktop.
--
-- PREREQUISITES: CE attached to cstrike_win64.exe, game in an active map.

-- ---- struct offsets (Source SDK x64) ----
local CC_NETWORKNAME = 0x10  -- const char* m_pNetworkName
local CC_RECVTABLE    = 0x18  -- RecvTable* m_pRecvTable
local CC_NEXT         = 0x20  -- ClientClass* m_pNext
local CC_CLASSID      = 0x28  -- int m_ClassID

local RT_PROPS        = 0x00  -- RecvProp* m_pProps
local RT_NPROPS       = 0x08  -- int m_nProps
local RT_NETTABLENAME = 0x18  -- const char* m_pNetTableName

local RP_VARNAME      = 0x00  -- const char* m_pVarName
local RP_RECVTYPE     = 0x08  -- int m_RecvType
local RP_OFFSET       = 0x4C  -- int m_Offset (ESTIMATED — adjust if wrong)
local RP_SIZE         = 0x58  -- sizeof(RecvProp) on x64 (estimated)

local DPT_NAMES = {
  [0]="int",[1]="float",[2]="vec3",[3]="vec2",
  [4]="char*",[5]="array",[6]="RecvTable*",[7]="int64",
}

-- ---- helpers ----
local function readQ(addr) return readQword(addr) or 0 end
local function readI(addr) return readInteger(addr) or 0 end
local function readStr(addr, n) return readString(addr, n or 64) or "" end

-- ---- get client.dll base ----
local clientBase = getAddress("client.dll")
if not clientBase or clientBase == 0 then
  print("ERROR: can't find client.dll — is CE attached to the game?")
  return
end
print(string.format("client.dll: 0x%X", clientBase))

-- ---- AOB scan for known class name strings ----
-- find "CWorld\0" or "CCSPlayer\0" in memory, then find what points AT it
local searchStrings = {
  { bytes = "43 57 6F 72 6C 64 00",             name = "CWorld" },     -- "CWorld\0"
  { bytes = "43 43 53 50 6C 61 79 65 72 00",    name = "CCSPlayer" },  -- "CCSPlayer\0"
  { bytes = "44 54 5F 42 61 73 65 45 6E 74 69 74 79 00", name = "DT_BaseEntity" },
}

local ccHead = nil

for _, search in ipairs(searchStrings) do
  print(string.format("scanning for '%s'...", search.name))
  local results = AOBScan(search.bytes, "+X-W-C")
  if results and results.Count > 0 then
    print(string.format("  found %d hit(s)", results.Count))
    -- for each string hit, scan for a pointer TO it (the m_pNetworkName field)
    for i = 0, math.min(results.Count - 1, 9) do
      local strAddr = tonumber(results[i])
      -- build AOB for the pointer value (little-endian 8 bytes)
      local p1 = strAddr % 256
      local p2 = math.floor(strAddr / 256) % 256
      local p3 = math.floor(strAddr / 65536) % 256
      local p4 = math.floor(strAddr / 16777216) % 256
      local p5 = math.floor(strAddr / 4294967296) % 256
      local p6 = math.floor(strAddr / 1099511627776) % 256
      local p7 = math.floor(strAddr / 281474976710656) % 256
      local p8 = math.floor(strAddr / 72057594037927936) % 256
      local ptrAOB = string.format("%02X %02X %02X %02X %02X %02X %02X %02X",
                                   p1, p2, p3, p4, p5, p6, p7, p8)

      local ptrResults = AOBScan(ptrAOB, "+X-W-C")
      if ptrResults and ptrResults.Count > 0 then
        print(string.format("  found %d pointer(s) to '%s' string", ptrResults.Count, search.name))
        -- check each pointer hit — if it's a m_pNetworkName field,
        -- the ClientClass starts 0x10 bytes before it
        for j = 0, math.min(ptrResults.Count - 1, 4) do
          local ptrAddr = tonumber(ptrResults[j])
          local ccAddr = ptrAddr - CC_NETWORKNAME

          -- validate: does m_pNext look like a pointer? does m_pRecvTable?
          local nextCC = readQ(ccAddr + CC_NEXT)
          local recvTable = readQ(ccAddr + CC_RECVTABLE)
          local nameCheck = readStr(readQ(ptrAddr), 32)

          if nextCC > 0x10000 and recvTable > 0x10000 and nameCheck == search.name then
            print(string.format("  ** VALID ClientClass at 0x%X (name='%s') **", ccAddr, nameCheck))
            ccHead = ccAddr
            break
          end
        end
        if ccHead then break end
      end
      -- cleanup scan results object
      if ptrResults then ptrResults.Destroy() end
    end
    results.Destroy()
    if ccHead then break end
  else
    print("  no hits")
  end
end

if not ccHead then
  print("ERROR: could not find ClientClass head.")
  print("Make sure the game is in an active map (not main menu).")
  return
end

-- ---- walk the linked list ----
print(string.format("\n=== walking ClientClass list from 0x%X ===", ccHead))

local classes = {}
local cc = ccHead
local visited = {}
local count = 0

while cc and cc > 0x10000 and not visited[cc] and count < 200 do
  visited[cc] = true
  count = count + 1

  local namePtr  = readQ(cc + CC_NETWORKNAME)
  local rtPtr    = readQ(cc + CC_RECVTABLE)
  local nextCC   = readQ(cc + CC_NEXT)
  local classID  = readI(cc + CC_CLASSID)

  if namePtr == 0 or rtPtr == 0 then break end

  local className = readStr(namePtr, 64)
  if className == "" then className = "(unknown)" end

  -- read RecvTable
  local propsPtr  = readQ(rtPtr + RT_PROPS)
  local nProps    = readI(rtPtr + RT_NPROPS)
  local tnPtr     = readQ(rtPtr + RT_NETTABLENAME)
  local tableName = readStr(tnPtr, 64)
  if tableName == "" then tableName = className end

  -- walk RecvProp array
  local props = {}
  if propsPtr > 0 and nProps > 0 and nProps < 500 then
    for i = 0, nProps - 1 do
      local propAddr = propsPtr + (i * RP_SIZE)
      local varNamePtr = readQ(propAddr + RP_VARNAME)
      local recvType   = readI(propAddr + RP_RECVTYPE)
      local offset     = readI(propAddr + RP_OFFSET)

      local varName = readStr(varNamePtr, 64)
      if varName and varName ~= "" and not varName:match("^%d") and varName ~= "baseclass" then
        local typeStr = DPT_NAMES[recvType] or string.format("type_%d", recvType)
        props[#props + 1] = { name = varName, type = typeStr, offset = offset }
      end
    end
  end

  classes[#classes + 1] = {
    className = className,
    tableName = tableName,
    classID   = classID,
    props     = props,
  }

  print(string.format("  [#%d] %s (id=%d, %d props) DT: %s",
        count, className, classID, #props, tableName))

  cc = nextCC
end

print(string.format("\n=== %d classes dumped ===", #classes))

-- ---- write output file ----
local desktop = os.getenv("USERPROFILE") .. "\\Desktop\\"
local outPath = desktop .. "netvar_dump.txt"
local f = io.open(outPath, "w")
if not f then
  print("ERROR: can't write to " .. outPath)
  return
end

f:write(string.format("// CS:S v93 x64 Netvar Dump (CE Lua)\n"))
f:write(string.format("// Dumped: %s\n\n", os.date("%Y-%m-%d %H:%M:%S")))
f:write(string.format("// %d classes\n\n", #classes))

for _, cls in ipairs(classes) do
  f:write(string.format("class %s {\n", cls.className))
  f:write(string.format("public:\n"))
  f:write(string.format("    // DT: %s, ClassID: %d\n", cls.tableName, cls.classID))
  for _, prop in ipairs(cls.props) do
    f:write(string.format("    VAROFF(%s, %s, 0x%04X);\n", prop.name, prop.type, prop.offset))
  end
  f:write("};\n\n")
end
f:close()

print(string.format("=== written to %s ===", outPath))

-- ---- print key classes summary ----
local important = { CCSPlayer=1, CBaseEntity=1, CBasePlayer=1, CBaseAnimating=1,
                    CBaseCombatWeapon=1, CCSPlayerResource=1 }

print("\n=== key offsets ===")
for _, cls in ipairs(classes) do
  for imp in pairs(important) do
    if cls.className:find(imp) then
      print(string.format("\n  %s (%s):", cls.className, cls.tableName))
      for _, prop in ipairs(cls.props) do
        print(string.format("    %-40s %-10s 0x%04X", prop.name, prop.type, prop.offset))
      end
    end
  end
end

print("\n=== done ===")
