import sys
tf=int(sys.argv[1]); old=len(sys.argv)>2 and sys.argv[2]=="old"
o=["scene C_BOOT","flag cfgdefaults 1","wait 6","scene L4_load","flag golevel 4","wait 45","flag msn","wait 3","flag msnmainat 201","wait 4","press cross 0.2","wait 4","shot in_msn_before"]
if old: o.append("flag roomreload1320 0")
if tf: o.append(f"flag tffail {tf}")
o+=["scene KwikEMart_door","foot 213.91 299.04 0.0 5.56","wait 2","shot door_before","press up 0.5","wait 0.4","press triangle 0.2","wait 3","shot in_1","wait 14","shot in_2","end"]
print("\n".join(o))
