#!/usr/bin/env python3
"""
Generate tutorial.texi from the tutorials/ directory.

The output is meant to be @include'd inside getdp.texi, under an existing
chapter declaration ``@chapter GetDP tutorial''. It contains:

  1. The body of tutorials/README.md (without its H1) as the chapter
     introduction.
  2. An @menu listing every top-level tutorial.
  3. One @node/@section per top-level tutorial, each made of:
       - the body of the tutorial's README.md (everything below the H1),
         converted from Markdown to Texinfo;
       - an @image showing a screenshot of the tutorial and a @url link to
         its source directory in the GetDP repository;
       - one @subheading File @file{NAME} followed by an @verbatiminclude
         directive for every .geo/.pro file of the tutorial.

A subdirectory of a tutorial that itself contains a README.md and one or
more .geo/.pro files (e.g. ``09-Template_library/bonus'') is picked up as
a sub-tutorial and emitted as an @subsection of its parent section, with
its own sub-menu under the parent and per-file @subsubheading headings.
Its image name is derived from the path by replacing slashes with
underscores (so ``09-Template_library/bonus'' uses image
``09-Template_library_bonus'').

Relative Markdown links between READMEs (e.g. a parent tutorial linking to
``bonus/README.md'') are resolved against the tutorial tree: if they point
at a directory that is itself emitted as a (sub)tutorial, they become
Texinfo cross-references to the corresponding node; otherwise they are
rewritten as absolute URLs into the source repository, since a relative
path is meaningless in the generated manual. Absolute links are left
alone.

The script is generic: drop in new ``NN-Name'' tutorial directories (or
sub-tutorial subdirectories with a README.md and .geo/.pro files) and
rerun -- node pointers, menus, cross-references and image/URL blocks are
recomputed automatically.

Default behavior assumes the script is run from the directory that
contains getdp.texi, with tutorials at ``../../tutorials'' and tutorial
screenshots at ``images/<basename>''. Run without arguments in that
layout, or use the options below.
"""

import argparse
import os
import posixpath
import re
import sys


# ---------------------------------------------------------------------------
# Texinfo escaping
# ---------------------------------------------------------------------------

def tex_escape(text):
    """Escape Texinfo special characters in inline text."""
    return text.replace('@', '@@').replace('{', '@{').replace('}', '@}')


# ---------------------------------------------------------------------------
# Minimal Markdown -> Texinfo conversion
#
# Handles the subset of Markdown actually used in the tutorial READMEs:
# headings, bullet/numbered lists (with continuation lines and blank lines
# between items), fenced code blocks, inline code, bold, italic, links.
# ---------------------------------------------------------------------------

INLINE_CODE_RE = re.compile(r'`([^`]+)`')
BOLD_RE = re.compile(r'\*\*([^*]+)\*\*')
ITALIC_RE = re.compile(r'(?<!\*)\*([^*\n]+)\*(?!\*)')
LINK_RE = re.compile(r'\[([^\]]+)\]\(([^)]+)\)')

# Anything with a scheme ("https:", "mailto:", ...) or a protocol-relative
# "//host/..." prefix is an external link and is emitted as-is.
ABSOLUTE_URL_RE = re.compile(r'^(?:[A-Za-z][A-Za-z0-9+.-]*:|//)')

BULLET_RE = re.compile(r'^\s*[-*+]\s+')
NUMBERED_RE = re.compile(r'^\s*\d+\.\s+')
HEADING_RE = re.compile(r'^(#+)\s+(.*?)\s*#*\s*$')
FENCE_RE = re.compile(r'^```')
CONT_RE = re.compile(r'^\s{2,}\S')


def convert_inline(text, link_handler=None):
    """Convert inline Markdown to Texinfo.

    Replace links, inline code, bold and italic with Texinfo placeholders
    BEFORE escaping the surrounding raw text, so the Texinfo specials we
    introduce are not double-escaped.

    ``link_handler'', if given, is called as ``link_handler(url, text)'' for
    every Markdown link and may return a ready-made (already escaped)
    Texinfo string; returning None falls back to a plain @uref.
    """
    placeholders = []

    def stash(s):
        placeholders.append(s)
        return '\x00{}\x00'.format(len(placeholders) - 1)

    def convert_link(m):
        label, url = m.group(1), m.group(2)
        if link_handler is not None:
            replacement = link_handler(url, label)
            if replacement is not None:
                return stash(replacement)
        return stash('@uref{' + tex_escape(url) + ', ' +
                     tex_escape(label) + '}')

    text = LINK_RE.sub(convert_link, text)
    text = INLINE_CODE_RE.sub(
        lambda m: stash('@code{' + tex_escape(m.group(1)) + '}'),
        text)
    text = BOLD_RE.sub(
        lambda m: stash('@strong{' + tex_escape(m.group(1)) + '}'),
        text)
    text = ITALIC_RE.sub(
        lambda m: stash('@emph{' + tex_escape(m.group(1)) + '}'),
        text)

    parts = re.split(r'\x00(\d+)\x00', text)
    out = []
    for i, part in enumerate(parts):
        if i % 2 == 0:
            out.append(tex_escape(part))
        else:
            out.append(placeholders[int(part)])
    return ''.join(out)


# Markdown heading levels -> Texinfo unnumbered heading commands.
# ``level_offset'' lets the caller shift everything down (e.g. for content
# embedded inside a subsection rather than a section).
HEADING_COMMANDS = ['@heading', '@subheading', '@subsubheading']


def heading_command(md_level, level_offset):
    idx = md_level - 1 + level_offset
    idx = max(0, min(idx, len(HEADING_COMMANDS) - 1))
    return HEADING_COMMANDS[idx]


def convert_markdown(md_text, level_offset=0, link_handler=None):
    """Convert a block of Markdown to Texinfo.

    ``level_offset'' shifts heading levels down: with offset 0, ``# X''
    maps to @heading, ``## X'' to @subheading; with offset 1, ``# X'' maps
    to @subheading, ``## X'' to @subsubheading.

    ``link_handler'' is passed through to convert_inline (see there).
    """
    lines = md_text.split('\n')
    out = []
    n = len(lines)
    i_ref = [0]

    def consume_list_items(marker_re, strip_re):
        items = []
        while i_ref[0] < n:
            line = lines[i_ref[0]]
            if marker_re.match(line):
                text = strip_re.sub('', line)
                i_ref[0] += 1
                cont = []
                while i_ref[0] < n and CONT_RE.match(lines[i_ref[0]]):
                    cont.append(lines[i_ref[0]].strip())
                    i_ref[0] += 1
                if cont:
                    text = text + ' ' + ' '.join(cont)
                items.append(text)
            elif line.strip() == '':
                j = i_ref[0] + 1
                while j < n and lines[j].strip() == '':
                    j += 1
                if j < n and marker_re.match(lines[j]):
                    i_ref[0] = j
                else:
                    break
            else:
                break
        return items

    while i_ref[0] < n:
        line = lines[i_ref[0]]

        if FENCE_RE.match(line):
            i_ref[0] += 1
            block = []
            while i_ref[0] < n and not FENCE_RE.match(lines[i_ref[0]]):
                block.append(lines[i_ref[0]])
                i_ref[0] += 1
            if i_ref[0] < n:
                i_ref[0] += 1  # skip closing ```
            out.append('@example')
            for bl in block:
                out.append(tex_escape(bl))
            out.append('@end example')
            continue

        m = HEADING_RE.match(line)
        if m:
            level = len(m.group(1))
            title = m.group(2).strip()
            out.append('{} {}'.format(
                heading_command(level, level_offset), tex_escape(title)))
            i_ref[0] += 1
            continue

        if BULLET_RE.match(line):
            items = consume_list_items(BULLET_RE,
                                       re.compile(r'^\s*[-*+]\s+'))
            out.append('@itemize @bullet')
            for it in items:
                out.append('@item')
                out.append(convert_inline(it, link_handler))
            out.append('@end itemize')
            continue

        if NUMBERED_RE.match(line):
            items = consume_list_items(NUMBERED_RE,
                                       re.compile(r'^\s*\d+\.\s+'))
            out.append('@enumerate')
            for it in items:
                out.append('@item')
                out.append(convert_inline(it, link_handler))
            out.append('@end enumerate')
            continue

        if line.strip() == '':
            out.append('')
            i_ref[0] += 1
            continue

        # Regular paragraph
        para = [line]
        i_ref[0] += 1
        while i_ref[0] < n:
            nxt = lines[i_ref[0]]
            if (nxt.strip() == ''
                    or FENCE_RE.match(nxt)
                    or HEADING_RE.match(nxt)
                    or BULLET_RE.match(nxt)
                    or NUMBERED_RE.match(nxt)):
                break
            para.append(nxt)
            i_ref[0] += 1
        out.append(convert_inline(
            ' '.join(l.strip() for l in para), link_handler))

    cleaned = []
    prev_blank = True
    for ln in out:
        if ln == '':
            if not prev_blank:
                cleaned.append('')
            prev_blank = True
        else:
            cleaned.append(ln)
            prev_blank = False
    while cleaned and cleaned[-1] == '':
        cleaned.pop()
    return '\n'.join(cleaned)


# ---------------------------------------------------------------------------
# README parsing
# ---------------------------------------------------------------------------

def parse_top_readme(text):
    """Return the body of the top-level README, dropping its H1."""
    lines = text.split('\n')
    start = 0
    for i, line in enumerate(lines):
        if re.match(r'^#\s+', line):
            start = i + 1
            break
    return '\n'.join(lines[start:]).strip('\n')


def parse_tutorial_readme(text):
    """Parse a per-tutorial README. Returns (title, body_markdown).

    ``title'' is the H1 heading (with leading ``#'' stripped).
    ``body_markdown'' is the rest of the README, verbatim.
    """
    lines = text.split('\n')
    title = None
    start = 0
    for i, line in enumerate(lines):
        m = re.match(r'^#\s+(.*?)\s*#*\s*$', line)
        if m:
            title = m.group(1).strip()
            start = i + 1
            break
    return title, '\n'.join(lines[start:]).strip('\n')


# ---------------------------------------------------------------------------
# Tutorial discovery
# ---------------------------------------------------------------------------

DIR_RE = re.compile(r'^(\d+)[-_](.+)$')


def _has_tutorial_files(path):
    """True iff path contains a README.md and at least one .geo or .pro file."""
    if not os.path.isfile(os.path.join(path, 'README.md')):
        return False
    for f in os.listdir(path):
        if (os.path.isfile(os.path.join(path, f))
                and f.lower().endswith(('.geo', '.pro'))):
            return True
    return False


def collect_tutorial_paths(tutorials_dir):
    """Yield (rel_path, full_path, level) tuples in emission order.

    Top-level numbered directories (NN-Name) are returned in numeric order
    as ``section''. Any direct subdirectory of such a tutorial that itself
    has a README.md plus at least one .geo or .pro file is emitted as a
    ``subsection'' immediately after its parent, in alphabetical order.
    """
    top_dirs = []
    for entry in os.listdir(tutorials_dir):
        full = os.path.join(tutorials_dir, entry)
        if not os.path.isdir(full):
            continue
        m = DIR_RE.match(entry)
        if not m:
            continue
        top_dirs.append((int(m.group(1)), entry, full))
    top_dirs.sort()
    for _, basename, full in top_dirs:
        yield (basename, full, 'section')
        subs = []
        for sub in os.listdir(full):
            sub_full = os.path.join(full, sub)
            if not os.path.isdir(sub_full):
                continue
            if not _has_tutorial_files(sub_full):
                continue
            subs.append((sub, sub_full))
        subs.sort(key=lambda x: x[0].lower())
        for sub_name, sub_full in subs:
            yield (basename + '/' + sub_name, sub_full, 'subsection')


def collect_files(tutorial_path):
    """Return an ordered list of .geo and .pro filenames (top level only)."""
    geo_files = []
    pro_files = []
    for entry in os.listdir(tutorial_path):
        full = os.path.join(tutorial_path, entry)
        if not os.path.isfile(full):
            continue
        low = entry.lower()
        if low.endswith('.geo'):
            geo_files.append(entry)
        elif low.endswith('.pro'):
            pro_files.append(entry)
    geo_files.sort(key=str.lower)
    geo_stems = {os.path.splitext(g)[0].lower() for g in geo_files}

    def pro_key(name):
        stem = os.path.splitext(name)[0].lower()
        bucket = 3
        for gs in geo_stems:
            if stem == gs:
                bucket = 0
                break
            if stem.startswith(gs + '_'):
                bucket = 1
                break
            if stem.startswith(gs):
                bucket = 2
                break
        return (bucket, name.lower())

    pro_files.sort(key=pro_key)
    return geo_files + pro_files


def node_name_from_relpath(rel_path):
    """Make a Texinfo node name from a path relative to tutorials/.

    Strips a leading ``NN-'' from each path component, replaces underscores
    with spaces, drops characters that confuse Texinfo node syntax, joins
    components with spaces, and prefixes with ``Tutorial ''.

    Examples:
      '04-Magneto-quasistatics'         -> 'Tutorial Magneto-quasistatics'
      '09-Template_library/bonus'       -> 'Tutorial Template library bonus'
    """
    parts = []
    for component in rel_path.replace(os.sep, '/').split('/'):
        m = DIR_RE.match(component)
        name = m.group(2) if m else component
        name = name.replace('_', ' ')
        parts.append(name)
    name = ' '.join(parts)
    name = re.sub(r'[,:.()]', '', name)
    return 'Tutorial ' + name


def image_name_from_relpath(rel_path):
    """Return the image basename for a tutorial.

    For top-level tutorials this is the directory basename; for sub-tutorials
    the slash is replaced with an underscore so a ``bonus'' subdirectory of
    ``09-Template_library'' uses the image ``09-Template_library_bonus''.
    """
    return rel_path.replace('/', '_')


# ---------------------------------------------------------------------------
# Cross-references between READMEs
#
# READMEs may link to each other with relative paths that make sense when
# browsing the repository (e.g. ``[bonus tutorial](bonus/README.md)'' in a
# parent tutorial). Such a path is meaningless in the generated manual, so
# it is turned into a Texinfo cross-reference when it designates a tutorial
# that has a node of its own, and into an absolute repository URL
# otherwise.
# ---------------------------------------------------------------------------

def resolve_local_link(url, base_rel):
    """Return the path a relative link points to, relative to tutorials/.

    ``base_rel'' is the path (relative to tutorials/) of the directory
    holding the README that contains the link; it is empty for the
    top-level README. Returns None if the link is absolute, is a bare
    fragment, or escapes the tutorials tree.
    """
    if ABSOLUTE_URL_RE.match(url) or url.startswith('#'):
        return None
    path = url.split('#', 1)[0].split('?', 1)[0]
    if not path:
        return None
    joined = posixpath.normpath(posixpath.join(base_rel, path))
    if joined == '.' or joined == '..' or joined.startswith('../'):
        return None
    return joined


def make_link_handler(base_rel, node_map, gitlab_base_url, tutorials_dir):
    """Build the ``link_handler'' used to convert one README's links.

    ``node_map'' maps a tutorial path (relative to tutorials/) to its
    Texinfo node name. ``tutorials_dir'' is only used to warn about links
    that point nowhere.
    """
    source = posixpath.join(base_rel, 'README.md') if base_rel else 'README.md'

    def handler(url, label):
        if ABSOLUTE_URL_RE.match(url) or url.startswith('#'):
            return None  # external link or bare anchor: leave it alone
        target = resolve_local_link(url, base_rel)
        if target is None:
            sys.stderr.write(
                'Warning: {}: relative link {!r} points outside the '
                'tutorials tree; left as is\n'.format(source, url))
            return None

        # A link to a tutorial directory, or to the README.md inside it,
        # becomes a cross-reference to that tutorial's node. @pxref is used
        # inside parentheses, which is its intended form and avoids the
        # trailing-punctuation requirement of @ref in a running sentence.
        candidates = [target]
        if posixpath.basename(target).lower() == 'readme.md':
            candidates.insert(0, posixpath.dirname(target))
        for candidate in candidates:
            candidate = candidate.strip('/')
            if candidate in node_map:
                return '{} (@pxref{{{}}})'.format(tex_escape(label),
                                                  node_map[candidate])

        # Anything else in the tree (a .pro file, a directory without a
        # node of its own) is linked to in the repository instead.
        if not os.path.exists(os.path.join(tutorials_dir, *target.split('/'))):
            sys.stderr.write(
                'Warning: {}: link target {!r} does not exist\n'
                .format(source, url))
        return '@uref{' + tex_escape(gitlab_base_url.rstrip('/') + '/'
                                     + target) + ', ' + tex_escape(label) + '}'

    return handler


# ---------------------------------------------------------------------------
# Output assembly
# ---------------------------------------------------------------------------

HEADER_COMMENT = (
    '@c -------------------------------------------------------------------------\n'
    '@c {section_name}\n'
    '@c -------------------------------------------------------------------------'
)


def _build_section_records(paths, tutorials_dir, include_prefix, node_map,
                           gitlab_base_url):
    """Parse every tutorial directory and return a list of section records."""
    sections = []
    for rel, full, level in paths:
        readme_path = os.path.join(full, 'README.md')
        with open(readme_path, encoding='utf-8') as fh:
            title, intro_md = parse_tutorial_readme(fh.read())
        if title is None:
            sys.stderr.write(
                'Warning: skipping {} (no H1 in README)\n'.format(rel))
            continue
        # Drop the ``GetDP '' prefix from per-tutorial titles. The enclosing
        # chapter is already titled ``GetDP tutorial'', so repeating it in
        # every section header is redundant. Capitalize the first letter of
        # the remainder so that, e.g., ``GetDP tutorial 1: ...'' becomes
        # ``Tutorial 1: ...''.
        title = re.sub(r'^GetDP\s+', '', title)
        if title:
            title = title[0].upper() + title[1:]
        # Inside a subsection, shift heading levels in the converted intro
        # so an in-body ``## Features'' becomes @subsubheading rather than
        # @subheading -- matching the per-file @subsubheading used below.
        offset = 1 if level == 'subsection' else 0
        intro_texi = convert_markdown(
            intro_md, level_offset=offset,
            link_handler=make_link_handler(rel, node_map, gitlab_base_url,
                                           tutorials_dir))
        filenames = collect_files(full)
        include_paths = [
            '/'.join([include_prefix.rstrip('/'), rel, fname])
            for fname in filenames
        ]
        sections.append({
            'rel_path': rel,
            'level': level,
            'node': node_name_from_relpath(rel),
            'title': title,
            'intro': intro_texi,
            'files': list(zip(filenames, include_paths)),
        })
    return sections


def _wire_pointers(sections, chapter_node):
    """Populate prev/next/up node pointers and child lists on each record."""
    last_section = None
    for s in sections:
        if s['level'] == 'section':
            s['children'] = []
            last_section = s
        else:  # subsection
            if last_section is None:
                raise SystemExit(
                    'Internal error: subsection {!r} has no parent section'
                    .format(s['rel_path']))
            s['parent'] = last_section
            last_section['children'].append(s)

    # Section-level chain (skips over subsections)
    section_records = [s for s in sections if s['level'] == 'section']
    for i, s in enumerate(section_records):
        s['prev_node'] = section_records[i - 1]['node'] if i > 0 else ''
        s['next_node'] = (section_records[i + 1]['node']
                          if i + 1 < len(section_records) else '')
        s['up_node'] = chapter_node

    # Subsection-level chain (siblings only)
    for parent in section_records:
        children = parent['children']
        for i, c in enumerate(children):
            c['prev_node'] = children[i - 1]['node'] if i > 0 else ''
            c['next_node'] = (children[i + 1]['node']
                              if i + 1 < len(children) else '')
            c['up_node'] = parent['node']


def _emit_image_and_link(s, pieces, images_dir, gitlab_base_url):
    """Append the @image and source-link block for one tutorial."""
    rel = s['rel_path']
    image_path = images_dir.rstrip('/') + '/' + image_name_from_relpath(rel)
    url = gitlab_base_url.rstrip('/') + '/' + rel
    desc = 'tutorials/' + rel
    pieces.append('@center@image{' + image_path
                  + ',14cm,,Screenshot of tutorial ' + rel + '}')
    pieces.append('@center See @url{' + url + ',' + desc + '}.')


def _emit_section(s, pieces, images_dir, gitlab_base_url):
    """Append the texinfo block for one section/subsection to ``pieces''."""
    pieces.append(HEADER_COMMENT.format(section_name=s['title']))
    pieces.append('')
    # Force a page break before each tutorial in printed output. @page is
    # a no-op for info and HTML, so this only affects the TeX/PDF rendering.
    pieces.append('@page')
    pieces.append('@node {n}, {nx}, {p}, {up}'.format(
        n=s['node'], nx=s['next_node'], p=s['prev_node'], up=s['up_node']))
    section_cmd = '@section' if s['level'] == 'section' else '@subsection'
    pieces.append('{} {}'.format(section_cmd, tex_escape(s['title'])))
    pieces.append('')

    if s['intro']:
        pieces.append(s['intro'])
        pieces.append('')

    _emit_image_and_link(s, pieces, images_dir, gitlab_base_url)
    pieces.append('')

    # A section with subsections needs a sub-menu listing them so that
    # makeinfo can validate the node graph.
    if s['level'] == 'section' and s.get('children'):
        pieces.append('@menu')
        for c in s['children']:
            pieces.append('* {}::'.format(c['node']))
        pieces.append('@end menu')
        pieces.append('')

    # Per-file headings: one level below the section/subsection itself so
    # readers see a clear hierarchy (section > file) regardless of where
    # the tutorial sits in the tree.
    file_heading_cmd = ('@subheading' if s['level'] == 'section'
                        else '@subsubheading')
    for fname, inc_path in s['files']:
        pieces.append('{} File @file{{{}}}'.format(file_heading_cmd,
                                                   tex_escape(fname)))
        pieces.append('@verbatiminclude {}'.format(inc_path))
        pieces.append('')


def build_texi(tutorials_dir, include_prefix, parent_node,
               images_dir, gitlab_base_url, script_name):
    """Assemble the full tutorial.texi content as a string."""
    top_readme_path = os.path.join(tutorials_dir, 'README.md')
    if not os.path.isfile(top_readme_path):
        raise SystemExit(
            'Error: missing top-level README at {}'.format(top_readme_path))

    # Discover the tutorials first: the node names they will get are needed
    # to resolve cross-references, including those made from the top-level
    # README.
    paths = list(collect_tutorial_paths(tutorials_dir))
    if not any(level == 'section' for _, _, level in paths):
        raise SystemExit(
            'Error: no numbered tutorial subdirectories under {}'
            .format(tutorials_dir))
    node_map = {rel: node_name_from_relpath(rel) for rel, _, _ in paths}

    with open(top_readme_path, encoding='utf-8') as fh:
        top_intro_md = parse_top_readme(fh.read())
    top_intro_texi = convert_markdown(
        top_intro_md,
        link_handler=make_link_handler('', node_map, gitlab_base_url,
                                       tutorials_dir))

    sections = _build_section_records(paths, tutorials_dir, include_prefix,
                                      node_map, gitlab_base_url)
    _wire_pointers(sections, parent_node)

    pieces = []
    pieces.append('@c This file is auto-generated by {}.'.format(script_name))
    pieces.append('@c Do not edit by hand; regenerate after adding or '
                  'modifying tutorials.')
    pieces.append('')

    if top_intro_texi:
        pieces.append(top_intro_texi)
        pieces.append('')

    # Chapter menu lists only top-level tutorial sections.
    pieces.append('@menu')
    for s in sections:
        if s['level'] == 'section':
            pieces.append('* {}::'.format(s['node']))
    pieces.append('@end menu')
    pieces.append('')

    for s in sections:
        _emit_section(s, pieces, images_dir, gitlab_base_url)

    return '\n'.join(pieces).rstrip() + '\n'


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

DEFAULT_TUTORIALS_DIR = '../../tutorials'
DEFAULT_OUTPUT = 'tutorial.texi'
DEFAULT_INCLUDE_PREFIX = '../../tutorials'
DEFAULT_PARENT_NODE = 'GetDP tutorial'
DEFAULT_IMAGES_DIR = 'images'
DEFAULT_GITLAB_BASE_URL = (
    'https://gitlab.onelab.info/getdp/getdp/-/tree/master/tutorials')


def main(argv=None):
    parser = argparse.ArgumentParser(
        description='Generate tutorial.texi from the tutorials/ directory. '
                    'With no arguments, assumes it is being run from the '
                    'directory containing getdp.texi, with tutorials at '
                    '../../tutorials and screenshots at images/.')
    parser.add_argument(
        '--tutorials-dir', default=DEFAULT_TUTORIALS_DIR,
        help='Path to the tutorials directory on disk, used to read the '
             'README.md files (default: %(default)s).')
    parser.add_argument(
        '--output', default=DEFAULT_OUTPUT,
        help='Output texinfo file (default: %(default)s).')
    parser.add_argument(
        '--include-prefix', default=DEFAULT_INCLUDE_PREFIX,
        help='Path prefix prepended to each @verbatiminclude directive, '
             'relative to the directory in which tutorial.texi is processed '
             '(default: %(default)s).')
    parser.add_argument(
        '--parent-node', default=DEFAULT_PARENT_NODE,
        help='Texinfo node name of the parent chapter that @include\'s this '
             'file; used as the "up" pointer of every top-level section node '
             '(default: %(default)r).')
    parser.add_argument(
        '--images-dir', default=DEFAULT_IMAGES_DIR,
        help='Directory containing tutorial screenshots, relative to the '
             'directory in which tutorial.texi is processed. The image used '
             'for tutorial NN-Name is <images-dir>/NN-Name; for a '
             'sub-tutorial NN-Name/sub it is <images-dir>/NN-Name_sub '
             '(default: %(default)s).')
    parser.add_argument(
        '--gitlab-base-url', default=DEFAULT_GITLAB_BASE_URL,
        help='Base URL of the tutorials directory in the source repository; '
             'the relative tutorial path is appended to form the per-tutorial '
             'link (default: %(default)s).')
    args = parser.parse_args(argv)

    if not os.path.isdir(args.tutorials_dir):
        parser.error('{} is not a directory'.format(args.tutorials_dir))

    content = build_texi(
        tutorials_dir=args.tutorials_dir,
        include_prefix=args.include_prefix,
        parent_node=args.parent_node,
        images_dir=args.images_dir,
        gitlab_base_url=args.gitlab_base_url,
        script_name=os.path.basename(sys.argv[0] if sys.argv[0] else
                                     'make_tutorial_texi.py'),
    )

    with open(args.output, 'w', encoding='utf-8') as fh:
        fh.write(content)

    sys.stderr.write('Wrote {}\n'.format(args.output))


if __name__ == '__main__':
    main()
