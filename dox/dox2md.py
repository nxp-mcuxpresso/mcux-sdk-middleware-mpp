#!/usr/bin/env python3
# Copyright 2024-2026 NXP
# SPDX-License-Identifier: Apache-2.0
#
# dox2md.py - Convert doxygen XML output to Markdown documentation.
#
# Parses the XML files produced by doxygen (GENERATE_XML=YES) and outputs a
# single .md file that matches the format of the hand-crafted API docs in
# docs/mpp_api/mpp_api.md and docs/hal_api/hal_api.md.
#
# Usage:
#   python dox2md.py --xml-dir <path> --title "..." --version "x.y"
#                    [--intro-file <intro.md>] --output <out.md>

import argparse
import os
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


# ---------------------------------------------------------------------------
# XML text extraction helpers
# ---------------------------------------------------------------------------

def _text(node):
    """Return the stripped text content of a node, ignoring child tags."""
    return (node.text or "").strip()


def _text_safe(node):
    """Return stripped text content of a node, or '' if node is None."""
    if node is None:
        return ""
    return (node.text or "").strip()


def _ref_anchor(ref_node):
    """Return the anchor id for a <ref> element.

    Use the visible text of the ref (the type/symbol name) lowercased, which
    matches the hand-crafted doc anchors (e.g. #mpp_t, #mpp_params_t).
    Fall back to stripping the doxygen refid prefix if text is absent.
    """
    name = (ref_node.text or "").strip()
    if name:
        return re.sub(r"\s+", "-", name.lower())
    refid = ref_node.get("refid", "")
    anchor = re.sub(r"^(struct|union|group__)", "", refid)
    anchor = anchor.replace("__", "_").lower()
    return anchor


def _node_to_md(node, inline=False):
    """Recursively convert a doxygen XML description node to markdown text.

    inline=True produces a single-line string (newlines become spaces).
    """
    if node is None:
        return ""

    parts = []
    tag = node.tag

    if tag in ("briefdescription", "detaileddescription", "inbodydescription"):
        for child in node:
            parts.append(_node_to_md(child))
        return "\n".join(p for p in parts if p).strip()

    if tag == "para":
        inner_parts = []
        if node.text:
            inner_parts.append(node.text)
        for child in node:
            inner_parts.append(_node_to_md(child, inline=True))
            if child.tail:
                inner_parts.append(child.tail)
        text = "".join(inner_parts).strip()
        # skip parameter/return sections - handled separately
        if node.find(".//parameterlist") is not None:
            return ""
        if node.find(".//simplesect") is not None:
            return ""
        return text

    if tag == "linebreak":
        # Return only the break character; the tail is handled by the parent loop.
        return "\n" if not inline else " "

    if tag == "ref":
        name = (node.text or "").strip()
        anchor = _ref_anchor(node)
        # Do NOT include tail here; the caller (para handler) appends child.tail.
        return f"[{name} ](#{anchor})"

    if tag == "itemizedlist":
        items = []
        for li in node.findall("listitem"):
            content = _node_to_md(li).strip()
            items.append(f"- {content}")
        return "\n".join(items)

    if tag == "listitem":
        for child in node:
            return _node_to_md(child, inline=True)
        return (node.text or "").strip()

    if tag == "computeroutput":
        # Collect text including from nested <ref> children (doxygen wraps identifiers
        # in both <computeroutput> and <ref> simultaneously).
        code_parts = [node.text or ""]
        for child in node:
            if child.tag == "ref":
                code_parts.append(child.text or "")
            code_parts.append(child.tail or "")
        code = "".join(code_parts).strip()
        # Do NOT include tail here; the caller (para handler) appends child.tail.
        return f"`{code}`"

    if tag == "bold":
        text = (node.text or "").strip()
        # Do NOT include tail here; the caller (para handler) appends child.tail.
        return f"**{text}**"

    if tag == "emphasis":
        text = (node.text or "").strip()
        # Do NOT include tail here; the caller (para handler) appends child.tail.
        return f"*{text}*"

    if tag in ("parameterlist", "simplesect"):
        return ""  # handled by caller

    # default: concatenate text content
    result = (node.text or "")
    for child in node:
        result += _node_to_md(child, inline=inline)
        result += (child.tail or "")
    return result.strip()


def _get_description(memberdef):
    """Return (brief, detailed) plain text for a memberdef/compounddef node."""
    brief = ""
    detail = ""

    brief_node = memberdef.find("briefdescription")
    if brief_node is not None:
        brief = _node_to_md(brief_node).strip()

    detail_node = memberdef.find("detaileddescription")
    if detail_node is not None:
        # Collect paragraphs that are NOT param/return/pre sections.
        # Skip the first paragraph if it duplicates the brief description.
        paras = []
        for para in detail_node.findall("para"):
            has_special = (para.find("parameterlist") is not None or
                           para.find("simplesect") is not None)
            if has_special:
                continue
            # Paras containing an itemizedlist must be rendered with
            # _convert_para_with_image to preserve list item structure;
            # collapsing their lines to spaces would produce a run-on list.
            if para.find("itemizedlist") is not None:
                para_lines = _convert_para_with_image(para)
                chunk = "\n".join(ln.rstrip() for ln in para_lines).strip()
                if chunk and chunk != brief:
                    paras.append(chunk)
            else:
                text = _node_to_md(para, inline=False).strip()
                # Normalize internal newlines (from <linebreak/>) to spaces
                text = " ".join(line.strip() for line in text.splitlines() if line.strip())
                if text and text != brief:
                    paras.append(text)
        detail = "\n".join(paras)

    return brief, detail


def _get_params(memberdef):
    """Return list of (direction, name, description) tuples from parameterlist."""
    params = []
    detail_node = memberdef.find("detaileddescription")
    if detail_node is None:
        return params
    for plist in detail_node.findall(".//parameterlist[@kind='param']"):
        for item in plist.findall("parameteritem"):
            name_list = item.find("parameternamelist")
            desc_node = item.find("parameterdescription")
            if name_list is None:
                continue
            pname_node = name_list.find("parametername")
            if pname_node is None:
                continue
            direction = pname_node.get("direction", "")
            pname = (pname_node.text or "").strip()
            pdesc = ""
            if desc_node is not None:
                for para in desc_node.findall("para"):
                    txt = (para.text or "").strip()
                    for child in para:
                        txt += (child.text or "") + (child.tail or "")
                    pdesc = txt.strip()
            params.append((direction, pname, pdesc))
    return params


def _get_return(memberdef):
    """Return the return description string from a simplesect kind=return."""
    detail_node = memberdef.find("detaileddescription")
    if detail_node is None:
        return ""
    for para in detail_node.findall("para"):
        for ss in para.findall("simplesect[@kind='return']"):
            ret_para = ss.find("para")
            if ret_para is not None:
                txt = (ret_para.text or "").strip()
                for child in ret_para:
                    if child.tag == "ref":
                        name = (child.text or "").strip()
                        anchor = _ref_anchor(child)
                        txt += f"[{name}](#{anchor})"
                    txt += (child.tail or "")
                return txt.strip()
    return ""


def _get_precondition(memberdef):
    """Return precondition text if present."""
    detail_node = memberdef.find("detaileddescription")
    if detail_node is None:
        return ""
    for para in detail_node.findall("para"):
        for ss in para.findall("simplesect[@kind='pre']"):
            items = []
            for li in ss.findall(".//listitem"):
                for p in li.findall("para"):
                    txt = _node_to_md(p, inline=True).strip()
                    if txt:
                        items.append("- " + txt)
            if items:
                return "\n".join(items)
    return ""


def _escape_md(name):
    """Escape underscores in identifiers for markdown."""
    return name.replace("_", r"\_")


def _fmt_type(type_node):
    """Format a <type> node as a markdown string with links."""
    if type_node is None:
        return ""
    parts = []
    if type_node.text:
        parts.append(type_node.text)
    for child in type_node:
        if child.tag == "ref":
            name = (child.text or "").strip()
            anchor = _ref_anchor(child)
            parts.append(f"[{name} ](#{anchor})")
        else:
            parts.append(child.text or "")
        parts.append(child.tail or "")
    return "".join(parts).strip()


# ---------------------------------------------------------------------------
# Section generators
# ---------------------------------------------------------------------------

def _gen_function_summary(functions):
    """Generate the **Functions** summary list."""
    lines = ["**Functions**\n"]
    for m in functions:
        ret_type = _fmt_type(m.find("type"))
        name = _text(m.find("name"))
        args = (m.find("argsstring").text or "").strip() if m.find("argsstring") is not None else "()"
        # build arg list with links
        arg_parts = []
        for param in m.findall("param"):
            ptype = _fmt_type(param.find("type"))
            pname = ""
            dn = param.find("declname")
            if dn is not None:
                pname = _escape_md((dn.text or "").strip())
            arg_parts.append(f"{ptype} {pname}".strip())
        arg_str = ", ".join(arg_parts)
        anchor = name.lower()
        line = f"- {ret_type}[ {name} (](#{anchor}){arg_str})"
        lines.append(line)
    return "\n".join(lines)


def _gen_struct_summary(compounds):
    """Generate the **Data Structures** summary list."""
    if not compounds:
        return ""
    lines = ["**Data Structures**\n"]
    for kind, name, refid in compounds:
        anchor = re.sub(r"^(struct|union|group__)", "", refid).replace("__", "_").lower()
        lines.append(f"- {kind} [{name}](#{anchor})")
    return "\n".join(lines)


def _gen_macro_summary(macros):
    """Generate the **Macros** summary list."""
    if not macros:
        return ""
    lines = ["**Macros**\n"]
    for m in macros:
        name = _text(m.find("name"))
        anchor = name.lower()
        init = m.find("initializer")
        val = ""
        if init is not None and init.text:
            val = " " + init.text.strip()
        lines.append(f"- #define [{name}](#{anchor}){val}")
    return "\n".join(lines)


def _gen_typedef_summary(typedefs):
    """Generate the **Typedefs** summary list."""
    if not typedefs:
        return ""
    lines = ["**Typedefs**\n"]
    for m in typedefs:
        ret_type = _fmt_type(m.find("type"))
        name = _text(m.find("name"))
        anchor = name.lower()
        args_node = m.find("argsstring")
        args = (args_node.text or "").strip() if args_node is not None else ""
        esc_name = _escape_md(name)
        lines.append(f"- typedef {ret_type} [{esc_name}](#{anchor}){args}")
    return "\n".join(lines)


def _gen_enum_summary(enums):
    """Generate the **Enumerations** summary list."""
    if not enums:
        return ""
    lines = ["**Enumerations**\n"]
    for m in enums:
        name = _text(m.find("name"))
        anchor = name.lower()
        values = []
        for ev in m.findall("enumvalue"):
            evname = _text(ev.find("name"))
            ev_anchor = evname.lower()
            values.append(f"\n\n  [{evname}](#{ev_anchor})")
        val_str = ",".join(values)
        if val_str:
            val_str += " }"
            lines.append(f"- enum [{name}](#{anchor}) {{{val_str}")
        else:
            lines.append(f"- enum [{name}](#{anchor})")
    return "\n".join(lines)


def _gen_function_detail(m):
    """Generate the full markdown block for a single function."""
    lines = []
    name = _text(m.find("name"))
    ret_type = _fmt_type(m.find("type"))
    esc_name = _escape_md(name)

    lines.append(f"##### {name}\n")

    # Signature block: always single line matching hand-crafted doc style
    arg_parts = []
    for param in m.findall("param"):
        ptype = _fmt_type(param.find("type"))
        dn = param.find("declname")
        pname = _escape_md((dn.text or "").strip()) if dn is not None else ""
        arg_parts.append(f"{ptype} {pname}".strip())
    args_str = " ( " + ", ".join(arg_parts) + " )"

    lines.append(f"{ret_type} {esc_name}{args_str}\n")

    brief, detail = _get_description(m)
    if brief:
        lines.append(brief + "\n")
    if detail:
        lines.append(detail + "\n")

    param_list = _get_params(m)
    if param_list:
        lines.append("**Parameters**\n")
        lines.append("|in/out|name|description|")
        lines.append("| - | - | - |")
        for direction, pname, pdesc in param_list:
            esc_pname = _escape_md(pname)
            lines.append(f"|{direction}|{esc_pname}|{pdesc}|")
        lines.append("")

    ret = _get_return(m)
    if ret:
        lines.append("**Returns**\n")
        lines.append(ret + "\n")

    pre = _get_precondition(m)
    if pre:
        lines.append("**Precondition**\n")
        lines.append(pre + "\n")

    return "\n".join(lines)


def _gen_struct_detail(compound_xml_path):
    """Generate the full markdown block for a struct/union compound."""
    tree = ET.parse(compound_xml_path)
    root = tree.getroot()
    cdef = root.find("compounddef")
    if cdef is None:
        return ""

    lines = []
    name = _text(cdef.find("compoundname"))
    kind = cdef.get("kind", "struct")
    esc_name = _escape_md(name)
    anchor = re.sub(r"^(struct|union)", "", cdef.get("id", "")).replace("__", "_").lower()

    lines.append(f"##### {name}\n")
    lines.append(f"**{kind} {esc_name}**\n")

    brief, detail = _get_description(cdef)
    if brief:
        lines.append(brief + "\n")
    if detail:
        lines.append(detail + "\n")

    # Collect members
    members = cdef.findall(".//memberdef[@kind='variable']")
    func_members = cdef.findall(".//memberdef[@kind='function']")
    all_members = members + func_members

    if all_members:
        # Check if any have descriptions
        has_desc = any(_text_safe(m.find("detaileddescription/para")) or
                       _text_safe(m.find("briefdescription/para"))
                       for m in all_members)

        if has_desc:
            lines.append("**Data Fields**\n")
            lines.append("|type|name|description|")
            lines.append("| - | - | - |")
            for m in all_members:
                mtype = _fmt_type(m.find("type"))
                mname = _text(m.find("name"))
                args_node = m.find("argsstring")
                args = (args_node.text or "").strip() if args_node is not None else ""
                # Function pointer members: doxygen puts "rettype (*" in <type>
                # and the args string starts with ")(...)".  Reconstruct so that
                # the name column shows "(*funcname)(args)" and type shows rettype.
                if mtype.endswith("(*") and args.startswith(")"):
                    mtype = mtype[:-2].rstrip()
                    # args is ")(...)" - strip the leading ")" so name becomes "(*name)(...)"
                    mname = f"(*{mname}){args[1:]}"
                    args = ""
                esc_mname = _escape_md(mname)
                # description
                desc = ""
                dp = m.find("detaileddescription/para")
                if dp is not None and dp.text:
                    desc = dp.text.strip()
                elif m.find("briefdescription/para") is not None:
                    desc = _text(m.find("briefdescription/para"))
                lines.append(f"|{mtype}|{esc_mname}{args}|{desc}|")
        else:
            # no descriptions - just list fields
            lines.append("**Data Fields**\n")
            for m in all_members:
                mtype = _fmt_type(m.find("type"))
                mname = _escape_md(_text(m.find("name")))
                args_node = m.find("argsstring")
                args = (args_node.text or "").strip() if args_node is not None else ""
                lines.append(f"- {mtype} {mname}{args}")
        lines.append("")

    return "\n".join(lines)


def _gen_enum_detail(m):
    """Generate the full markdown block for an enum memberdef."""
    lines = []
    name = _text(m.find("name"))
    esc_name = _escape_md(name)
    lines.append(f"##### {name}\n")
    lines.append(f"**enum {esc_name}**\n")

    # enum value list
    values = [_text(ev.find("name")) for ev in m.findall("enumvalue")]
    if values:
        lines.append("{ " + ",\n\n  ".join(values) + " }\n")

    brief, detail = _get_description(m)
    if brief:
        lines.append(brief + "\n")
    if detail:
        lines.append(detail + "\n")

    # enumerator table
    ev_rows = []
    for ev in m.findall("enumvalue"):
        evname = _text(ev.find("name"))
        evdesc = ""
        dp = ev.find("detaileddescription/para")
        if dp is not None:
            evdesc = (dp.text or "").strip()
        ev_rows.append((evname, evdesc))

    if any(d for _, d in ev_rows):
        lines.append("**Enumerator**\n")
        lines.append("|name|description|")
        lines.append("| - | - |")
        for evname, evdesc in ev_rows:
            esc_evname = _escape_md(evname)
            lines.append(f"|{esc_evname}|{evdesc}|")
        lines.append("")

    return "\n".join(lines)


def _gen_macro_detail(m):
    """Generate the full markdown block for a #define macro memberdef."""
    lines = []
    name = _text(m.find("name"))
    esc_name = _escape_md(name)
    lines.append(f"##### {name}\n")
    init = m.find("initializer")
    val = (" " + init.text.strip()) if init is not None and init.text else ""
    lines.append(f"#define **{esc_name}**{val}\n")
    brief, detail = _get_description(m)
    if brief:
        lines.append(brief + "\n")
    if detail:
        lines.append(detail + "\n")
    return "\n".join(lines)


def _gen_typedef_detail(m):
    """Generate the full markdown block for a typedef memberdef."""
    lines = []
    name = _text(m.find("name"))
    esc_name = _escape_md(name)
    ret_type = _fmt_type(m.find("type"))
    args_node = m.find("argsstring")
    args = (args_node.text or "").strip() if args_node is not None else ""
    lines.append(f"##### {name}\n")
    lines.append(f"typedef {ret_type} **{esc_name}**{args}\n")
    brief, detail = _get_description(m)
    if brief:
        lines.append(brief + "\n")
    if detail:
        lines.append(detail + "\n")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Group section generator
# ---------------------------------------------------------------------------

def _load_group(xml_dir, refid):
    """Parse a group XML file and return its compounddef element."""
    xml_path = xml_dir / f"{refid}.xml"
    if not xml_path.exists():
        return None
    tree = ET.parse(xml_path)
    return tree.getroot().find("compounddef")


def _section_for_group(xml_dir, refid, section_num, use_chapter_heading=False):
    """Generate a full chapter section for a doxygen group.

    use_chapter_heading -- when True, use '## Chapter N' / '### N.1 Title' style
                           (used when an intro chapter precedes the groups).
    """
    cdef = _load_group(xml_dir, refid)
    if cdef is None:
        return ""

    title = _text(cdef.find("title")) or _text(cdef.find("compoundname"))
    lines = []

    # Chapter heading - emit an explicit anchor using the title so that
    # cross-references like [Return Codes](#return-codes) resolve correctly
    # regardless of the section number prefix in the heading.
    title_anchor = re.sub(r"\s+", "-", title.lower())
    lines.append(f'<a id="{title_anchor}"></a>\n')
    if use_chapter_heading:
        lines.append(f"## Chapter {section_num}\n")
    else:
        lines.append(f"## {section_num}. {title}\n")

    # -- Summary lists --
    # Collect all member sections; deduplicate by name to avoid repeated entries
    # when the same symbol is added to multiple groups via \addtogroup.
    functions = []
    macros = []
    typedefs = []
    enums = []
    inner_classes = []  # (kind, name, refid)
    _seen_names = set()

    for sdef in cdef.findall("sectiondef"):
        kind = sdef.get("kind", "")
        for m in sdef.findall("memberdef"):
            mk = m.get("kind", "")
            mname = _text(m.find("name"))
            key = (mk, mname)
            if key in _seen_names:
                continue
            _seen_names.add(key)
            if mk == "function":
                functions.append(m)
            elif mk == "define":
                macros.append(m)
            elif mk == "typedef":
                typedefs.append(m)
            elif mk == "enum":
                enums.append(m)

    for ic in cdef.findall("innerclass"):
        ic_refid = ic.get("refid", "")
        ic_name = ic.text or ""
        # determine kind from refid prefix
        ic_kind = "union" if ic_refid.startswith("union") else "struct"
        inner_classes.append((ic_kind, ic_name, ic_refid))

    # summary blocks
    summaries = []
    if inner_classes:
        summaries.append(_gen_struct_summary(inner_classes))
    if macros:
        summaries.append(_gen_macro_summary(macros))
    if typedefs:
        summaries.append(_gen_typedef_summary(typedefs))
    if enums:
        summaries.append(_gen_enum_summary(enums))
    if functions:
        summaries.append(_gen_function_summary(functions))

    if summaries:
        lines.append("\n\n".join(s for s in summaries if s))
        lines.append("")

    # -- Detailed description --
    brief, detail = _get_description(cdef)
    if use_chapter_heading:
        lines.append(f"### {section_num}.1 {title}\n")
    else:
        lines.append(f"### {section_num}.1 Detailed Description\n")
    if brief:
        lines.append(brief + "\n")
    if detail:
        lines.append(detail + "\n")

    sub_num = 1

    # Function documentation
    if functions:
        lines.append(f"#### {section_num}.1.{sub_num} Function Documentation\n")
        sub_num += 1
        for m in functions:
            lines.append(_gen_function_detail(m))

    # Data structure documentation
    if inner_classes:
        lines.append(f"#### {section_num}.1.{sub_num} Data Structure Documentation\n")
        sub_num += 1
        for ic_kind, ic_name, ic_refid in inner_classes:
            xml_path = xml_dir / f"{ic_refid}.xml"
            if xml_path.exists():
                lines.append(_gen_struct_detail(xml_path))

    # Macro documentation
    if macros:
        lines.append(f"#### {section_num}.1.{sub_num} Macro Definition Documentation\n")
        sub_num += 1
        for m in macros:
            lines.append(_gen_macro_detail(m))

    # Typedef documentation
    if typedefs:
        lines.append(f"#### {section_num}.1.{sub_num} Typedef Documentation\n")
        sub_num += 1
        for m in typedefs:
            lines.append(_gen_typedef_detail(m))

    # Enumeration documentation
    if enums:
        lines.append(f"#### {section_num}.1.{sub_num} Enumeration Type Documentation\n")
        sub_num += 1
        for m in enums:
            lines.append(_gen_enum_detail(m))

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Intro chapter from a markdown mainpage
# ---------------------------------------------------------------------------

def _intro_chapter_from_page(xml_dir, section_num, title):
    """Extract chapter 1 intro text from the doxygen page XML (e.g. HalReadme).

    Returns a markdown string for the section.
    """
    # Try to find a page compound in the index
    index_path = xml_dir / "index.xml"
    if not index_path.exists():
        return ""

    index = ET.parse(index_path).getroot()
    page_refid = None
    for compound in index.findall("compound[@kind='page']"):
        page_refid = compound.get("refid", "")
        break

    if not page_refid:
        return ""

    page_path = xml_dir / f"{page_refid}.xml"
    if not page_path.exists():
        return ""

    tree = ET.parse(page_path)
    cdef = tree.getroot().find("compounddef")
    if cdef is None:
        return ""

    lines = [f"## Chapter {section_num}\n"]
    brief, detail = _get_description(cdef)
    # The detailed description contains all the page content
    desc_node = cdef.find("detaileddescription")
    if desc_node is None:
        return ""

    # Convert page content, handling headings encoded as sect1/sect2/sect3
    lines.append(_convert_page_sections(desc_node, section_num))
    return "\n".join(lines)


def _fmt_itemizedlist(node, indent=0):
    """Recursively format an itemizedlist with nested indentation.

    Top-level items use '- ', nested items use '    * '.
    """
    lines = []
    prefix = "    " * indent + ("- " if indent == 0 else "* ")
    for li in node.findall("listitem"):
        # A listitem may contain para children; para may contain nested lists
        for para in li.findall("para"):
            # Text part of this para (excluding nested lists)
            inner_text_parts = []
            if para.text:
                inner_text_parts.append(para.text.strip())
            nested_lists = []
            for child in para:
                if child.tag == "itemizedlist":
                    nested_lists.append(child)
                elif child.tag == "ref":
                    name = (child.text or "").strip()
                    anchor = _ref_anchor(child)
                    inner_text_parts.append(f"[{name} ](#{anchor})")
                    if child.tail:
                        inner_text_parts.append(child.tail.strip())
                else:
                    if child.text:
                        inner_text_parts.append(child.text.strip())
                    if child.tail:
                        inner_text_parts.append(child.tail.strip())
            item_text = " ".join(t for t in inner_text_parts if t)
            if item_text:
                lines.append("    " * indent + ("- " if indent == 0 else "* ") + item_text)
            for nested in nested_lists:
                lines.extend(_fmt_itemizedlist(nested, indent + 1))
    return lines


# Module-level image name map; populated by build_doc() when --image-map is used.
_IMAGE_MAP = {}


def _convert_para_with_image(para):
    """Convert a para that may contain image or list tags.

    Handles paras that have leading text followed by an itemizedlist, emitting
    the text paragraph first and then the list items separately.
    """
    lines = []
    # Check for image children
    images = para.findall("image")
    if images:
        for img in images:
            img_type = img.get("type", "")
            if img_type != "html":
                continue  # only emit the html variant; skip latex/rtf/docbook duplicates
            img_name = img.get("name", "")
            # Apply image name mapping if provided (e.g. mpp_hal_overview.png -> hal.png)
            img_name = _IMAGE_MAP.get(img_name, img_name)
            caption = (img.text or "").strip()
            if img_name:
                lines.append(f"![]({img_name})\n")
                if caption:
                    lines.append(f"**Figure 1.1 {caption}**\n")
        return lines
    # Check for itemizedlist children - emit any preceding text first
    item_lists = para.findall("itemizedlist")
    if item_lists:
        # Collect leading text (para.text + non-list inline children before first list)
        leading_parts = []
        if para.text and para.text.strip():
            leading_parts.append(para.text.strip())
        for child in para:
            if child.tag == "itemizedlist":
                break
            leading_parts.append(_node_to_md(child, inline=True))
            if child.tail and child.tail.strip():
                leading_parts.append(child.tail.strip())
        leading = " ".join(p for p in leading_parts if p)
        if leading:
            lines.append(leading + "\n")
        for il in item_lists:
            lines.extend(_fmt_itemizedlist(il, indent=0))
        lines.append("")  # blank line after list ensures paragraph separation
        return lines
    # plain text para
    txt = _node_to_md(para)
    if txt:
        lines.append(txt + "\n")
    return lines


def _convert_sect2_content(sc, sect2_num, chapter_num, sect1_num):
    """Convert a sect2 element: heading + body paragraphs + sect3 as numbered items."""
    lines = []
    h2 = _text(sc.find("title"))
    # Strip trailing colon from heading
    h2_clean = h2.rstrip(":")
    lines.append(f"#### {chapter_num}.{sect1_num}.{sect2_num} {h2_clean}\n")

    # Check for sect3 children (render as numbered list items)
    sect3_children = sc.findall("sect3")
    if sect3_children:
        num = 1
        for sc3 in sc:
            if sc3.tag == "para":
                for ln in _convert_para_with_image(sc3):
                    lines.append(ln)
            elif sc3.tag == "sect3":
                h3 = _text(sc3.find("title"))
                lines.append(f"{num}. **{h3}**")
                num += 1
                for sc3c in sc3:
                    if sc3c.tag == "para":
                        for il in sc3c.findall("itemizedlist"):
                            lines.extend(_fmt_itemizedlist(il, indent=0))
                        if not sc3c.findall("itemizedlist"):
                            txt = _node_to_md(sc3c)
                            if txt:
                                lines.append(txt)
    else:
        for sc2c in sc:
            if sc2c.tag == "para":
                for ln in _convert_para_with_image(sc2c):
                    lines.append(ln)
    lines.append("")
    return lines


def _convert_page_sections(desc_node, chapter_num):
    """Convert page detaileddescription with sect1/sect2/sect3 to markdown."""
    lines = []
    sect1_num = 0

    for child in desc_node:
        if child.tag == "para":
            for ln in _convert_para_with_image(child):
                lines.append(ln)

        elif child.tag == "sect1":
            sect1_num += 1
            heading = _text(child.find("title")).rstrip(":")

            if sect1_num == 1:
                # First sect1: render as ### heading with numbered #### sub-sections
                lines.append(f"### {chapter_num}.1 {heading}\n")
                sect2_num = 0
                for sc in child:
                    if sc.tag == "para":
                        for ln in _convert_para_with_image(sc):
                            lines.append(ln)
                    elif sc.tag == "sect2":
                        sect2_num += 1
                        lines.extend(_convert_sect2_content(sc, sect2_num, chapter_num, 1))
            else:
                # Subsequent sect1s: plain text title + body (no heading markup)
                lines.append(f"{heading}:")
                for sc in child:
                    if sc.tag == "para":
                        for ln in _convert_para_with_image(sc):
                            lines.append(ln)
                    elif sc.tag == "sect2":
                        h2 = _text(sc.find("title")).rstrip(":")
                        lines.append(f"\n{h2}:")
                        for sc2c in sc:
                            if sc2c.tag == "para":
                                for ln in _convert_para_with_image(sc2c):
                                    lines.append(ln)

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Main document builder
# ---------------------------------------------------------------------------

def build_doc(xml_dir, title, version, output_path, intro_from_page=False,
              group_order=None, ver_label=None):
    """Build the full markdown document from doxygen XML output.

    xml_dir        -- path to the doxygen xml/ output directory
    title          -- document title (e.g. 'eIQ MPP Hardware Abstraction Layer API')
    version        -- version string (e.g. '4.3')
    output_path    -- path to write the output .md file
    intro_from_page -- if True, extract chapter 1 from the doxygen page (mainpage md)
    group_order    -- optional list of group refids in display order; if None, auto-discover
    ver_label      -- version label prefix (e.g. 'MPP-HAL'). Auto-detected if None.
    """
    xml_dir = Path(xml_dir)
    index_path = xml_dir / "index.xml"
    if not index_path.exists():
        print(f"ERROR: index.xml not found in {xml_dir}", file=sys.stderr)
        sys.exit(1)

    index = ET.parse(index_path).getroot()

    # Discover groups if not provided
    if group_order is None:
        group_order = []
        for compound in index.findall("compound[@kind='group']"):
            group_order.append(compound.get("refid", ""))

    # Determine version label prefix
    if ver_label is None:
        ver_label = "MPP-HAL" if "HAL" in title.upper() or "HARDWARE ABSTRACTION LAYER" in title.upper() else "MPP"

    lines = []
    lines.append(f"# {title}\n")
    lines.append(f"{ver_label} VERSION {version}\n")

    section_num = 1
    has_intro = False

    # Optional intro chapter from mainpage
    if intro_from_page:
        intro = _intro_chapter_from_page(xml_dir, section_num, title)
        if intro:
            lines.append(intro)
            section_num += 1
            has_intro = True

    # One section per group
    for refid in group_order:
        section = _section_for_group(xml_dir, refid, section_num,
                                     use_chapter_heading=has_intro)
        if section:
            lines.append(section)
            section_num += 1

    content = "\n".join(lines)

    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"Generated {output_path}")


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Convert doxygen XML output to a Markdown API document.")
    parser.add_argument("--xml-dir", required=True,
                        help="Path to doxygen xml/ output directory")
    parser.add_argument("--title", required=True,
                        help="Document title")
    parser.add_argument("--version", required=True,
                        help="API version string (e.g. 4.3)")
    parser.add_argument("--output", required=True,
                        help="Output .md file path")
    parser.add_argument("--intro-from-page", action="store_true",
                        help="Extract chapter 1 from doxygen mainpage (USE_MDFILE_AS_MAINPAGE)")
    parser.add_argument("--groups", nargs="*",
                        help="Group refids in display order (auto-detected if omitted)")
    parser.add_argument("--ver-label", default=None,
                        help="Version label prefix (e.g. MPP-HAL). Auto-detected from title if omitted.")
    parser.add_argument("--image-map", nargs="*", default=None,
                        help="Image filename remappings as 'src:dst' pairs "
                             "(e.g. mpp_hal_overview.png:hal.png). "
                             "Replaces the image filename in the generated ![](...) links.")
    args = parser.parse_args()

    # Populate the module-level image map from --image-map pairs
    if args.image_map:
        for entry in args.image_map:
            if ":" in entry:
                src, dst = entry.split(":", 1)
                _IMAGE_MAP[src.strip()] = dst.strip()

    build_doc(
        xml_dir=args.xml_dir,
        title=args.title,
        version=args.version,
        output_path=args.output,
        intro_from_page=args.intro_from_page,
        group_order=args.groups if args.groups else None,
        ver_label=args.ver_label,
    )


if __name__ == "__main__":
    main()
