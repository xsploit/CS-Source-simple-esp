#!/usr/bin/env python3
"""
CS:S v93 x64 External Netvar Dumper
====================================
Talks to the CE MCP bridge (TCP relay on 127.0.0.1:9876) to walk the Source
engine's ClientClass linked list and dump every netvar offset for every class.
No injection — pure external memory reading.

PREREQUISITES:
1. CS:S running (cstrike_win64.exe) with bots spawned
2. Cheat Engine attached to the game
3. ce_mcp_bridge.lua executed in CE (pipe listening)
4. ce_tcp_relay.py running on 127.0.0.1:9876

USAGE:
    python dumper.py

OUTPUT:
    netvar_dump.txt — complete offset table, like the internal UC dumper but external.

STRUCTURE LAYOUTS (Source SDK x64, derived from cssourcex64 internal source):
The struct layouts below are the x64 versions. In Source x64:
  - pointers are 8 bytes
  - ClientClass has: m_pCreateFn, m_pCreateEventFn, m_pNetworkName (char*),
    m_pRecvTable (RecvTable*), m_pNext (ClientClass*), m_ClassID (int)
  - RecvTable has: m_pProps (RecvProp*), m_nProps (int), m_pDecoder,
    m_pNetTableName (char*), m_bInitialized (bool)
  - RecvProp has: m_pVarName (char*), m_RecvType (int), m_nFlags,
    StringBuffer, ... m_Offset (int) at a known position
"""

import socket
import struct
import json
import sys
import time

RELAY_HOST = "127.0.0.1"
RELAY_PORT = 9876

# Source SDK x64 struct offsets (derived from cssourcex64 usage patterns).
# these are the byte offsets of each field within the struct, in x64.
CLIENTCLASS_OFFSETS = {
    "m_pCreateFn":       0x00,   # function pointer (8 bytes)
    "m_pCreateEventFn":  0x08,   # function pointer (8 bytes)
    "m_pNetworkName":    0x10,   # const char* (8 bytes) — the class name string
    "m_pRecvTable":      0x18,   # RecvTable* (8 bytes)
    "m_pNext":           0x20,   # ClientClass* (8 bytes) — linked list
    "m_ClassID":         0x28,   # int (4 bytes)
}
CLIENTCLASS_SIZE = 0x30  # 48 bytes, padded

RECVTABLE_OFFSETS = {
    "m_pProps":          0x00,   # RecvProp* (8 bytes) — array of props
    "m_nProps":          0x08,   # int (4 bytes) — count
    # padding (4 bytes on x64 for alignment)
    "m_pDecoder":        0x10,   # pointer
    "m_pNetTableName":   0x18,   # const char* — the DT_ name
    "m_bInitialized":    0x20,   # bool
}
RECVTABLE_SIZE = 0x28

# RecvProp x64 layout — this is the tricky one. The internal code uses
# prop->GetOffset() which is a virtual, but the raw offset field is at a
# known position. From the SDK: m_Offset is stored after the type/flags.
# The layout (x64, packed with alignment):
#   0x00: m_pVarName (char*)
#   0x08: m_RecvType (int, SendPropType)
#   0x0C: m_nFlags (int)
#   0x10: StringBuffer (char[16])
#   0x20: ... proxy fn pointers ...
#   m_Offset: we need to find this empirically (scan for known offset values)
#
# For now, we use the SDK's known layout. m_Offset in RecvProp is typically
# at +0x4C on x64 (after proxy pointers). This may need adjustment.
RECVPROP_OFFSETS = {
    "m_pVarName":        0x00,   # const char*
    "m_RecvType":        0x08,   # int (SendPropType: DPT_Int=1, DPT_Float=2, ...)
    "m_Offset":          0x4C,   # int — THE FIELD OFFSET WE WANT
}
RECVPROP_SIZE = 0x58  # estimated; may vary

# SendPropType enum
DPT_TYPES = {
    0: "DPT_Int",
    1: "DPT_Float",
    2: "DPT_Vector",
    3: "DPT_VectorXY",
    4: "DPT_String",
    5: "DPT_Array",
    6: "DPT_DataTable",
    7: "DPT_Int64",
}

# ---- CE Bridge Client ----

class CEBridge:
    def __init__(self):
        self.sock = None
        self.req_id = 0

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(30)
        self.sock.connect((RELAY_HOST, RELAY_PORT))
        return True

    def close(self):
        if self.sock:
            self.sock.close()
            self.sock = None

    def call(self, method, params=None):
        """Send one JSON-RPC request, get one response. Single connection, sequential."""
        if not self.sock:
            raise RuntimeError("not connected")
        self.req_id += 1
        req = {"jsonrpc": "2.0", "method": method, "params": params or {}, "id": self.req_id}
        data = json.dumps(req).encode("utf-8")
        header = struct.pack("<I", len(data))
        self.sock.sendall(header + data)
        # read response header
        resp_header = b""
        while len(resp_header) < 4:
            chunk = self.sock.recv(4 - len(resp_header))
            if not chunk:
                raise ConnectionError("bridge closed connection")
            resp_header += chunk
        resp_len = struct.unpack("<I", resp_header)[0]
        if resp_len <= 0 or resp_len > 32 * 1024 * 1024:
            raise RuntimeError(f"bad response length: {resp_len}")
        # read response body
        resp_body = b""
        while len(resp_body) < resp_len:
            chunk = self.sock.recv(resp_len - len(resp_body))
            if not chunk:
                raise ConnectionError("bridge closed mid-response")
            resp_body += chunk
        return json.loads(resp_body.decode("utf-8")).get("result")

    def read_mem(self, addr, size):
        r = self.call("read_memory", {"address": addr, "size": size})
        if not r or "bytes" not in r:
            return None
        return bytes(r["bytes"])

    def read_qword(self, addr):
        b = self.read_mem(addr, 8)
        return struct.unpack_from("<Q", b)[0] if b else 0

    def read_dword(self, addr):
        b = self.read_mem(addr, 4)
        return struct.unpack_from("<I", b)[0] if b else 0

    def read_int(self, addr):
        b = self.read_mem(addr, 4)
        return struct.unpack_from("<i", b)[0] if b else 0

    def read_string(self, addr, max_len=64):
        r = self.call("read_string", {"address": addr, "max_length": max_len})
        if not r:
            return ""
        return r.get("value", "")

    def get_module_base(self, modname):
        r = self.call("enum_modules", {"limit": 300})
        if not r:
            return 0
        for m in r.get("modules", []):
            if m["name"].lower() == modname.lower():
                return int(m["address"], 16)
        return 0

    def aob_scan(self, pattern_hex, max_results=20):
        r = self.call("aob_scan", {"pattern": pattern_hex, "max_results": max_results})
        if not r:
            return []
        return [int(a, 16) if isinstance(a, str) else a for a in r.get("addresses", [])]


# ---- Dumper ----

def find_clientclass_head(bridge, client_base):
    """
    Find the head of the ClientClass linked list.

    Strategy: AOB-scan for a known network name string (e.g. "CWorld"), then
    find what points AT that string address (a ClientClass.m_pNetworkName field).
    The ClientClass struct is 0x10 bytes before the network name pointer.

    Fallback: scan for "CCSPlayer" or "DT_BaseEntity" strings.
    """
    # Try scanning for known class name strings
    for name in [b"CWorld\x00", b"CCSPlayer\x00", b"DT_BaseEntity\x00"]:
        hex_pattern = " ".join(f"{b:02X}" for b in name)
        print(f"  scanning for '{name.decode().strip(chr(0))}'...")
        hits = bridge.aob_scan(hex_pattern, max_results=20)
        if hits:
            print(f"  found {len(hits)} hit(s)")
            return hits, name.decode().strip(chr(0))
    return [], None


def dump_netvars(bridge, output_file="netvar_dump.txt"):
    print("=== CS:S x64 External Netvar Dumper ===")
    print()

    # 1. get module bases
    client_base = bridge.get_module_base("client.dll")
    if not client_base:
        print("ERROR: can't find client.dll — is the game running?")
        return
    print(f"client.dll base: 0x{client_base:X}")

    # 2. find ClientClass strings
    print()
    print("=== finding ClientClass linked list head ===")
    string_hits, found_name = find_clientclass_head(bridge, client_base)
    if not string_hits:
        print("ERROR: could not find any class name strings.")
        print("The AOB scan might need the game in an active state (map loaded, bots spawned).")
        return

    # 3. for each string hit, find what pointer references it
    # (a ClientClass.m_pNetworkName points at this string)
    print()
    print(f"=== searching for pointers to '{found_name}' string ===")
    print(f"  (this finds the ClientClass struct that owns this network name)")

    # The string address is in heap or data segment. We need to find a pointer
    # TO it. We scan the client.dll data segment for the address value.
    for hit_addr in string_hits[:5]:
        print(f"  string at 0x{hit_addr:X}, scanning for pointers to it...")
        # Convert the address to an AOB pattern (little-endian qword)
        addr_bytes = struct.pack("<Q", hit_addr)
        addr_hex = " ".join(f"{b:02X}" for b in addr_bytes)
        ptr_hits = bridge.aob_scan(addr_hex, max_results=10)
        if ptr_hits:
            print(f"  found {len(ptr_hits)} pointer(s) to this string")
            for ptr_addr in ptr_hits[:3]:
                # ptr_addr is the address of the m_pNetworkName field
                # ClientClass struct starts 0x10 bytes before m_pNetworkName
                cc_addr = ptr_addr - CLIENTCLASS_OFFSETS["m_pNetworkName"]
                print(f"  -> ClientClass at 0x{cc_addr:X}")

                # verify: read m_pNext and check it's also a valid pointer
                next_ptr = bridge.read_qword(cc_addr + CLIENTCLASS_OFFSETS["m_pNext"])
                recv_table_ptr = bridge.read_qword(cc_addr + CLIENTCLASS_OFFSETS["m_pRecvTable"])
                print(f"     m_pNext: 0x{next_ptr:X}")
                print(f"     m_pRecvTable: 0x{recv_table_ptr:X}")

                if next_ptr and recv_table_ptr:
                    print(f"  ** looks like a valid ClientClass! Walking the linked list... **")
                    walk_and_dump(bridge, cc_addr, output_file)
                    return

    print("ERROR: could not find a valid ClientClass struct.")
    print("The struct layout offsets might need adjustment for this build.")


def walk_and_dump(bridge, head_cc, output_file):
    """
    Walk the ClientClass linked list starting at head_cc.
    For each class, read its RecvTable and dump every RecvProp.
    """
    classes = []
    cc = head_cc
    visited = set()
    count = 0

    print()
    print(f"=== walking ClientClass linked list from 0x{head_cc:X} ===")

    while cc and cc not in visited and count < 200:
        visited.add(cc)
        count += 1

        # read ClientClass fields
        name_ptr = bridge.read_qword(cc + CLIENTCLASS_OFFSETS["m_pNetworkName"])
        recv_table_ptr = bridge.read_qword(cc + CLIENTCLASS_OFFSETS["m_pRecvTable"])
        next_cc = bridge.read_qword(cc + CLIENTCLASS_OFFSETS["m_pNext"])
        class_id = bridge.read_int(cc + CLIENTCLASS_OFFSETS["m_ClassID"])

        if not name_ptr or not recv_table_ptr:
            print(f"  [#{count}] 0x{cc:X}: invalid name/table ptr, stopping")
            break

        class_name = bridge.read_string(name_ptr, 64)
        if not class_name:
            class_name = "(unknown)"

        # read RecvTable
        props_ptr = bridge.read_qword(recv_table_ptr + RECVTABLE_OFFSETS["m_pProps"])
        n_props = bridge.read_int(recv_table_ptr + RECVTABLE_OFFSETS["m_nProps"])
        table_name_ptr = bridge.read_qword(recv_table_ptr + RECVTABLE_OFFSETS["m_pNetTableName"])
        table_name = bridge.read_string(table_name_ptr, 64) if table_name_ptr else class_name

        print(f"  [#{count}] {class_name} (id={class_id}, {n_props} props) — DT: {table_name}")

        # walk RecvProp array
        props = []
        if props_ptr and n_props > 0 and n_props < 500:
            for i in range(n_props):
                prop_addr = props_ptr + (i * RECVPROP_SIZE)
                var_name_ptr = bridge.read_qword(prop_addr + RECVPROP_OFFSETS["m_pVarName"])
                recv_type = bridge.read_int(prop_addr + RECVPROP_OFFSETS["m_RecvType"])
                offset = bridge.read_int(prop_addr + RECVPROP_OFFSETS["m_Offset"])

                var_name = bridge.read_string(var_name_ptr, 64) if var_name_ptr else ""
                if not var_name or var_name[0].isdigit():
                    continue
                if var_name == "baseclass":
                    continue

                type_str = DPT_TYPES.get(recv_type, f"type_{recv_type}")
                props.append((var_name, type_str, offset))

        classes.append({
            "class_name": class_name,
            "table_name": table_name,
            "class_id": class_id,
            "props": props,
        })

        cc = next_cc

    print()
    print(f"=== dumped {len(classes)} classes ===")

    # write output
    with open(output_file, "w") as f:
        f.write(f"// CS:S v93 x64 Netvar Dump (external, via CE MCP bridge)\n")
        f.write(f"// Dumped: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"// {len(classes)} classes\n\n")

        for cls in classes:
            f.write(f"class {cls['class_name']} {{\n")
            f.write(f"public:\n")
            f.write(f"    // DT: {cls['table_name']}, ClassID: {cls['class_id']}\n")
            for var_name, type_str, offset in cls["props"]:
                f.write(f"    VAROFF({var_name}, {type_str}, 0x{offset:X});\n")
            f.write(f"}};\n\n")

    print(f"=== written to {output_file} ===")

    # also print a summary of the important classes
    print()
    print("=== key offsets summary ===")
    important = ["CCSPlayer", "CBaseEntity", "CBasePlayer", "CBaseAnimating",
                 "CBaseCombatWeapon", "CCSPlayerResource"]
    for cls in classes:
        for imp in important:
            if imp in cls["class_name"]:
                print(f"\n  {cls['class_name']} ({cls['table_name']}):")
                for var_name, type_str, offset in cls["props"]:
                    print(f"    {var_name:40s} {type_str:10s} 0x{offset:04X}")
                break


if __name__ == "__main__":
    bridge = CEBridge()
    try:
        print("connecting to CE bridge on 127.0.0.1:9876...")
        bridge.connect()
        print("connected!")
        print()
        dump_netvars(bridge)
    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
    finally:
        bridge.close()
        print("\ndone.")
