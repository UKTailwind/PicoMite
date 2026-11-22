"""make.py: builds apps/doom/out/doom.app, Doom for the PicoMite (the PC3), from the Colour Maximite 2's port
(doomgeneric_pico.c, from its doomgeneric_cmm2.c). doomgeneric is fetched the first time into apps/doom/doomgeneric (git
ignores it), at the commit below, and built unaltered with the files its own Makefile names, its X11 layer replaced by
doomgeneric_pico.c, with FEATURE_SOUND (stub/ holds the SDL_mixer.h it then includes, empty). The program is GPL-2.0, as
doomgeneric is: its source must go with any copy given out (doomgeneric's, at that commit, and this directory).

    python apps/doom/make.py
then put out/doom.app beside freedoom1.wad (B:/Doom: the WAD is too big for A:) and, at the prompt:
    CHDIR "B:/Doom"
    RUN "doom.app"                          or RUN "doom.app", "-timedemo demo1" to measure it
"""
import os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(HERE, 'doomgeneric')
URL = 'https://github.com/ozkl/doomgeneric.git'
COMMIT = 'e257a09527062bfb986b6e5e0780839b6cbd5b71'   # 11 Oct 2026, as the CMM2's
FILES = '''dummy am_map doomdef doomstat dstrings d_event d_items d_iwad d_loop d_main d_mode d_net f_finale f_wipe g_game
hu_lib hu_stuff info i_cdmus i_endoom i_joystick i_scale i_sound i_system i_timer memio m_argv m_bbox m_cheat m_config
m_controls m_fixed m_menu m_misc m_random p_ceilng p_doors p_enemy p_floor p_inter p_lights p_map p_maputl p_mobj
p_plats p_pspr p_saveg p_setup p_sight p_spec p_switch p_telept p_tick p_user r_bsp r_data r_draw r_main r_plane
r_segs r_sky r_things sha1 sounds statdump st_lib st_stuff s_sound tables v_video wi_stuff w_checksum w_file w_main
w_wad z_zone w_file_stdc i_input i_video doomgeneric'''.split()


def main():
    if not os.path.isdir(SRC):
        subprocess.run(['git', 'clone', '-q', URL, SRC], check=True)
        subprocess.run(['git', '-C', SRC, 'checkout', '-q', COMMIT], check=True)
    head = subprocess.run(['git', '-C', SRC, 'rev-parse', 'HEAD'], capture_output=True, text=True).stdout.strip()
    if head != COMMIT:
        sys.exit('%s is at %s, not %s' % (SRC, head, COMMIT))
    os.makedirs(os.path.join(HERE, 'out'), exist_ok=True)
    dg = os.path.join(SRC, 'doomgeneric')
    srcs = [os.path.join(dg, f + '.c') for f in FILES] + [os.path.join(HERE, 'doomgeneric_pico.c')]
    flags = ['-DCMAP256', '-DDOOMGENERIC_RESX=320', '-DDOOMGENERIC_RESY=200', '-DFEATURE_SOUND', '-w']
    cmd = [sys.executable, '-I', os.path.join(REPO, 'tools', 'sdk', 'build.py'), os.path.join(HERE, 'out', 'doom.app')]
    cmd += srcs + ['--psram', '--stack', '0x10000', '--heap', str(2560 << 10), '-I', dg, '-I', os.path.join(HERE, 'stub')]
    cmd += ['--cflag=' + f for f in flags]
    sys.exit(subprocess.run(cmd).returncode)


if __name__ == '__main__':
    main()
