"""Prints a Waves bundle's ParamXML as a table, in preset-array order (research, Phase 1):

    python -I paramxml_table.py <bundle>/Contents/Resources/ParamXML/1000.xml [--md]

Columns: index, ID, label, range, scale type (and its Var), resolution, reset / initial value, units, flags, display
function, link groups. Nothing from the file is copied into the repository beyond what RESEARCH.md quotes.
"""
import sys
import xml.etree.ElementTree as ET


def text(e, path, default=""):
    if e is None: return default
    x = e.find(path)
    return x.text.strip() if x is not None and x.text else default


def rows(path):
    root = ET.parse(path).getroot()
    for i, p in enumerate(root.iter("Parameter")):
        s = p.find("Scale")
        flags = [f.tag for f in p.find("Flags")] if p.find("Flags") is not None else []
        if p.find("IgnoreOnPresetLoad") is not None: flags.append("IgnoreOnPresetLoad")
        reset, init = p.find("ResetValue"), p.find("InitialValue")
        yield dict(i=i, id=p.get("Name"), label=text(p, "Label"), lo=text(s, "Minimum"), hi=text(s, "Maximum"),
                   scale=text(s, "ScaleType") + (" var " + text(s, "Var") if s is not None and s.find("Var") is not None else ""),
                   res=text(s, "Resolution"), units=text(p, "Units"),
                   reset=(reset.get("Type", "") + " " + (reset.text or "").strip()) if reset is not None else "",
                   init=(init.get("Type", "") + " " + (init.text or "").strip()) if init is not None else "",
                   flags=",".join(flags), disp=text(p, "DisplayFunction"), link=text(p, "LinkGroups"))


def main():
    path = sys.argv[1]
    md = "--md" in sys.argv
    if md:
        print("| # | ID | Label | Range | Scale | Res | Reset | Initial | Units | Flags | Display | Link |")
        print("|---|---|---|---|---|---|---|---|---|---|---|---|")
    for r in rows(path):
        if md:
            print("| %(i)d | %(id)s | %(label)s | %(lo)s … %(hi)s | %(scale)s | %(res)s | %(reset)s | %(init)s | %(units)s | %(flags)s | %(disp)s | %(link)s |" % r)
        else:
            print("%(i)3d %(id)-40s %(label)-14s %(lo)8s .. %(hi)-8s %(scale)-26s res %(res)-5s reset %(reset)-24s init %(init)-24s %(units)-4s %(flags)s %(disp)s link %(link)s" % r)


if __name__ == "__main__":
    main()
