"""Link PlayStation programs with the Psy-Q SDK."""

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

from .sdk import dos_run, dos_text, tool_succeeded
from .sdk_compat import library_input


# Ordinary linker inputs. The native linker selects members from these archives.
# PSYLINK 1.29 visits the archives in this order and returns to the first one
# after every member it takes, so the `inclib` order is the order of the SDK
# member groups in the linked text, and a member's dependencies on an archive
# listed earlier follow it directly (GPU SYS -> LIBAPI GPU_cw). Within one
# archive the linker's own symbol-table order applies.
LIBRARIES = {
    'PSX.EXE': ('LIBSN', 'LIBAPI'),
    # Retail GAME text holds the groups CARD, CD, SPU (with the SND members
    # that need SPU interleaved), SND, GTE, ETC, API, C, GPU; no LIBPRESS
    # member is linked.
    'GAME.EXE': ('LIBSN', 'LIBCARD', 'LIBCD', 'LIBSPU', 'LIBSND', 'LIBGTE',
                 'LIBETC', 'LIBAPI', 'LIBC', 'LIBGPU'),
    # Retail overlay RODATA and first SDK text runs place these archives in
    # PRESS, GPU, GTE, CD, ETC, SND, SPU order after the API/C helpers.
    'OPEN.EXE': ('LIBSN', 'LIBAPI', 'LIBC', 'LIBPRESS', 'LIBGPU', 'LIBGTE',
                 'LIBCD', 'LIBETC', 'LIBSND', 'LIBSPU'),
    'END.EXE': ('LIBSN', 'LIBAPI', 'LIBC', 'LIBPRESS', 'LIBGPU', 'LIBGTE',
                'LIBCD', 'LIBETC', 'LIBSND', 'LIBSPU'),
}
ENTRY = '__SN_ENTRY_POINT'
BOOT_STARTUP = '2MBYTE.OBJ'
OVERLAY_STARTUP = 'NONE2.OBJ'
# The retail OPEN/END text puts NONE2 immediately before the movie units.
# GAME's startup placement is still unresolved and keeps the append order.
OVERLAY_STARTUP_BEFORE_UNIT = {'OPEN.EXE': 'open.movie', 'END.EXE': 'end.movie'}
MALLOC_OBJECT_SHA256 = '628e405fd0e3acfff2ce9d4a15d481f0aa36398c14e9eae0a82b7ff0a86a74c9'
# Every overlay links the Release 2.5 allocator object after the startup.
ALLOCATOR_IMAGES = ('GAME.EXE', 'OPEN.EXE', 'END.EXE')
# GAME also links the kit's memory-card CARD.OBJ explicitly: retail places its
# _card_clear directly after MALLOC although no GAME code calls it, and its
# _new_card/_card_write references pull those LIBCARD members.
CARD_OBJECT = 'CARD.OBJ'
CARD_OBJECT_SHA256 = '4ab0873cddbf26d99aded93f8b654c861f44409cee408b3bff7026fc59665387'
CARD_IMAGES = ('GAME.EXE',)
# The converter is the CPE2X 1.3 build from Sony's Runtime Library 3.0 CD,
# not the kit's: it zeroes the reserved words and the EXEC save area and sets
# s_addr to 801ffff0, as the retail PSX, OPEN and END headers record.
CONVERTER_SHA256 = '641d95ebe8131c3503407518cb6110ed311cb5f87943d866296660ab98938af2'
# CPE2X never writes the title field's tail (header 0x7c-0x87), which keeps
# its own stack there: a saved frame pointer, the far return address 158e and
# code segment of an earlier call, and that call's two arguments. The frame
# pointer moves with the length of the CPE argument, and the code segment is
# where DOS loaded the converter. The retail PSX/OPEN/END tails record a 15- or
# 16-byte argument (this 7-character directory prefix gives that for all
# three names) and these code segments. Retail GAME came from an older,
# unavailable converter and keeps DOSBox-X's default layout here.
CONVERTER_DIRECTORY = 'CPEDIR'
CONVERTER_CODE_SEGMENTS = {'PSX.EXE': 0x3b30, 'OPEN.EXE': 0x3b30, 'END.EXE': 0x36b0}
# Under the pinned DOSBox-X, `minimum mcb free = S` loads CPE2X's code at S + 0x122.
DOSBOX_CODE_SEGMENT_BIAS = 0x122


def file_hash(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_image(name, root, units, compile_one, *, repo, load_address, bounds_source) -> dict:
    """Compile and link a program using its source units and SDK libraries."""
    root = root.resolve()
    if root.exists():
        shutil.rmtree(root)
    root.mkdir(parents=True)
    report = {'image': name, 'linked': False, 'phase': 'compile', 'units': [],
              'libraries': [], 'startup': None, 'boot_startup': None, 'tools': {}}
    try:
        tools = {'ASPSX.EXE': Path(os.environ['PSYQ_ASPSX']),
                 'PSYLINK.EXE': Path(os.environ['PSYQ_BIN']) / 'PSYLINK.EXE',
                 'CPE2X.EXE': Path(os.environ['PSYQ_CPE2X'])}
        for tool, source in tools.items():
            shutil.copyfile(source, root / tool)
            report['tools'][tool] = {'path': str(source), 'sha256': file_hash(source)}
        if report['tools']['CPE2X.EXE']['sha256'] != CONVERTER_SHA256:
            raise ValueError(f'{tools["CPE2X.EXE"]}: expected the Runtime Library 3.0 CPE2X')
        if not units:
            raise ValueError(f'{name}: no C source units')
        for index, unit in enumerate(units):
            report['units'].append(compile_one(unit, root, index))
        assembler_commands = [u['assembler_command'] for u in report['units']
                              if 'assembler_command' in u]
        if name == 'PSX.EXE':
            source = Path(os.environ['PSYQ_LIB']) / BOOT_STARTUP
            shutil.copyfile(source, root / BOOT_STARTUP)
            report['boot_startup'] = {
                'file': BOOT_STARTUP, 'path': str(source), 'sha256': file_hash(source),
                'provenance': 'retail stup1/stup0 offsets match the pinned 2MBYTE object',
            }
        else:
            assembler = Path(os.environ['PSYQ_ASMPSX'])
            shutil.copyfile(assembler, root / 'ASMPSX.EXE')
            report['tools']['ASMPSX.EXE'] = {
                'path': str(assembler), 'sha256': file_hash(assembler)}
            source = repo / bounds_source
            shutil.copyfile(source, root / 'BOUNDS.ASM')
            dos_text(root / 'BOUNDS.ASM')
            report['boundaries'] = {
                'source': str(source.relative_to(repo)), 'source_sha256': file_hash(source),
                'object': 'BOUNDS.OBJ',
                'assembler_command': 'asmpsx /l /oc+ BOUNDS.ASM,BOUNDS.OBJ > BOUNDS.TXT'}
            assembler_commands.append(report['boundaries']['assembler_command'])
            source = Path(os.environ['PSYQ_LIB']) / OVERLAY_STARTUP
            shutil.copyfile(source, root / OVERLAY_STARTUP)
            report['startup'] = {
                'file': OVERLAY_STARTUP,
                'path': str(source),
                'sha256': file_hash(source),
                'provenance': 'exact Psy-Q 3.0 LIB object (identical to Release 2.5 H2000)',
            }
        report['phase'] = 'assemble'
        if assembler_commands:
            dos_run(root, assembler_commands, 'asm')
        for unit in report['units']:
            if 'assembler_command' in unit:
                tool_succeeded(root, unit['log'], unit['object'], b'LNK\x02')
            elif not (root / unit['object']).read_bytes().startswith(b'LNK\x02'):
                raise ValueError('compiler produced no native object')
            unit['object_sha256'] = file_hash(root / unit['object'])
        if report['startup'] and not (root / OVERLAY_STARTUP).read_bytes().startswith(b'LNK\x02'):
            raise ValueError(f'{OVERLAY_STARTUP}: expected a Psy-Q LNK object')
        if report['boot_startup'] and not (root / BOOT_STARTUP).read_bytes().startswith(b'LNK\x02'):
            raise ValueError(f'{BOOT_STARTUP}: expected a Psy-Q LNK object')
        if report['startup']:
            tool_succeeded(root, 'BOUNDS.TXT', 'BOUNDS.OBJ', b'LNK\x02')
            report['boundaries']['object_sha256'] = file_hash(root / 'BOUNDS.OBJ')
        if name in ALLOCATOR_IMAGES:
            source = Path(os.environ['PSYQ_MALLOC_OBJ'])
            if file_hash(source) != MALLOC_OBJECT_SHA256:
                raise ValueError(f'{source}: expected the retail-matching Sony MALLOC.OBJ')
            shutil.copyfile(source, root / 'MALLOC.OBJ')
            report['allocator'] = {
                'file': 'MALLOC.OBJ', 'path': str(source),
                'sha256': MALLOC_OBJECT_SHA256,
                'provenance': 'hash-pinned Psy-Q Release 2.5 object; retail fixed text bytes exact',
            }
        if name in CARD_IMAGES:
            source = Path(os.environ['PSYQ_LIB']) / CARD_OBJECT
            if file_hash(source) != CARD_OBJECT_SHA256:
                raise ValueError(f'{source}: expected the Psy-Q 3.0 CARD.OBJ')
            shutil.copyfile(source, root / CARD_OBJECT)
            report['card'] = {
                'file': CARD_OBJECT, 'path': str(source), 'sha256': CARD_OBJECT_SHA256,
                'provenance': 'Psy-Q 3.0 LIB object; retail _card_clear text follows MALLOC',
            }
        for library in LIBRARIES[name]:
            filename = library + '.LIB'
            source = Path(os.environ['PSYQ_LIB']) / filename
            data, corrections = library_input(filename, source.read_bytes())
            (root / filename).write_bytes(data)
            report['libraries'].append({
                'file': filename, 'path': str(source), 'sha256': file_hash(source),
                'link_input_sha256': hashlib.sha256(data).hexdigest(),
                'corrections': corrections,
            })
        report['phase'] = 'link'
        stem = name.removesuffix('.EXE')
        object_inputs = [f'\tinclude "{u["object"]}"' for u in report['units']]
        if report['boot_startup']:
            object_inputs.append(f'\tinclude "{BOOT_STARTUP}"')
        if report['startup']:
            units = [u['unit'] for u in report['units']]
            before = OVERLAY_STARTUP_BEFORE_UNIT.get(name)
            startup_after = units.index(before) if before in units else len(object_inputs)
            object_inputs.insert(startup_after, f'\tinclude "{OVERLAY_STARTUP}"')
            report['startup']['after_source_units'] = startup_after
        commands = [f'\torg ${load_address:08x}',
                    *object_inputs,
                    *(['\tinclude "BOUNDS.OBJ"'] if report['startup'] else []),
                    *(['\tinclude "MALLOC.OBJ"'] if report.get('allocator') else []),
                    *([f'\tinclude "{CARD_OBJECT}"'] if report.get('card') else []),
                    *(f'\tinclib "{library["file"]}"' for library in report['libraries']),
                    'bssdata group bss', '\tsection .sbss,bssdata',
                    '\tsection .bss,bssdata',
                    *(['\tsection .bss_end,bssdata'] if report['startup'] else []),
                    f'\tregs pc={ENTRY}']
        (root / 'LINK.LNK').write_bytes(('\r\n'.join(commands) + '\r\n').encode('ascii'))
        # The overlay source units plus SDK members exceed PSYLINK 1.29's
        # default object-module table. /n raises only that table limit.
        module_limit = '' if name == 'PSX.EXE' else ' /n1024'
        report['linker_command'] = (
            f'psylink /c{module_limit} @LINK.LNK,{stem}.CPE,{stem}.SYM,{stem}.MAP > LINK.TXT')
        dos_run(root, [report['linker_command']], 'link')
        tool_succeeded(root, 'LINK.TXT', stem + '.CPE', b'CPE\x01')
        report['phase'] = 'convert'
        (root / CONVERTER_DIRECTORY).mkdir()
        shutil.copyfile(root / (stem + '.CPE'), root / CONVERTER_DIRECTORY / (stem + '.CPE'))
        segment = CONVERTER_CODE_SEGMENTS.get(name)
        report['converter_command'] = f'cpe2x {CONVERTER_DIRECTORY}\\{stem}.CPE > CONVERT.TXT'
        report['converter_dosbox_settings'] = (
            [f'minimum mcb free={segment - DOSBOX_CODE_SEGMENT_BIAS:x}'] if segment else [])
        dos_run(root, [report['converter_command']], 'convert',
                tuple(report['converter_dosbox_settings']))
        executable = root / name
        converted = root / CONVERTER_DIRECTORY / name
        if not converted.is_file():
            raise RuntimeError('CPE2X produced no executable; see CONVERT.TXT')
        converted.replace(executable)
        actual = executable.read_bytes()
        if len(actual) < 2048 or actual[:8] != b'PS-X EXE':
            raise ValueError('CPE2X produced an invalid executable')
        report.update(linked=True, phase='complete', executable=str(executable),
                      cpe=str(root / (stem + '.CPE')))
    except (KeyError, OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        (root / name).unlink(missing_ok=True)
        report['error'] = str(error)
    (root / 'build.json').write_text(json.dumps(report, indent=2) + '\n')
    return report
