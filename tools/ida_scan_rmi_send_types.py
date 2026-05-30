import ida_funcs, idautils, idc

mw = idc.get_name_ea_simple('j_?Message_Write@Proud@@YAXAAVCMessage@1@W4MessageType@1@@Z')
add_fn = idc.get_name_ea_simple('j_?Add@CSendFragRefs@Proud@@QAEXABVCMessage@2@@Z')
stack_ctor = idc.get_name_ea_simple('j_??0CSmallStackAllocMessage@Proud@@QAE@XZ')

def fn_has_call_to(fn_ea, target):
    if target == idc.BADADDR:
        return False
    fn = ida_funcs.get_func(fn_ea)
    if not fn:
        return False
    for head in idautils.FuncItems(fn.start_ea):
        for xr in idautils.XrefsFrom(head, 0):
            if xr.to == target:
                return True
    return False

seen = {}
for xr in idautils.XrefsTo(mw):
    fn = ida_funcs.get_func(xr.frm)
    if not fn:
        continue
    ea = xr.frm
    imm = None
    cur = ea
    for _ in range(25):
        cur = idc.prev_head(cur)
        if cur < fn.start_ea:
            break
        if idc.print_insn_mnem(cur) == 'push':
            if idc.get_operand_type(cur, 0) == idc.o_imm:
                v = idc.get_operand_value(cur, 0)
                if v <= 0xFF:
                    imm = v
                    break
    if imm is None:
        continue
    start = fn.start_ea
    if start in seen:
        continue
    name = ida_funcs.get_func_name(start) or ''
    rmi_like = fn_has_call_to(start, add_fn) and fn_has_call_to(start, stack_ctor)
    has_add = fn_has_call_to(start, add_fn)
    if rmi_like or 'RmiSend' in name or has_add:
        seen[start] = (imm, name, rmi_like, has_add)

items = sorted(seen.items(), key=lambda kv: (kv[1][0], kv[0]))
for start, (imm, name, rl, ha) in items:
    print('0x%08X  typ=%3d  rmi_like=%d  has_add=%d  %s' % (start, imm, rl, ha, name[:65]))
print('TOTAL', len(items))
