# Sound sources

Every file here is CC0 1.0 (public domain dedication). Each was converted with
`build/bin/sound_convert <source> <file> [options]` (mono, 16-bit, peak −1 dBFS unless `--rms` is given);
see `tool/sound_convert.c`. Effects are 44100 Hz (`--rate 44100`), the ambience beds 22050 Hz.
CMake refuses to configure if a `.wav` here is missing from this list.

Freesound sources were converted from the page's high-quality preview
(`https://cdn.freesound.org/previews/<id/1000>/<id>_<user id>-hq.mp3`, 128 kbps MP3), which is CC0 like the original.

| File | Source | Original file | Author | Licence | Options |
|---|---|---|---|---|---|
| `roll.wav` | https://freesound.org/s/766870/ | `SPRTInd-Int_BowlingBall_Zus_OwSfx_Loop` (preview) | ZusIsKing | CC0 1.0 | `--start 0.6 --length 3.1 --loop --xfade 0.3 --rate 44100` |
| `gutter_drop.wav` | https://kenney.nl/assets/impact-sounds | `impactPlank_medium_000.ogg` | Kenney (kenney.nl) | CC0 1.0 | |
| `pin_hit.wav` | https://freesound.org/s/151203/ | `bowling pins mono.aif` (preview), fourth knock | Bruce Burbank | CC0 1.0 | `--start 2.082 --length 0.26 --rate 44100` |
| `pin_clatter.wav` | https://freesound.org/s/151203/ | `bowling pins mono.aif` (preview), first knock | Bruce Burbank | CC0 1.0 | `--start 0.256 --length 0.24 --rate 44100` |
| `charge.wav` | https://kenney.nl/assets/sci-fi-sounds | `forceField_000.ogg` | Kenney (kenney.nl) | CC0 1.0 | `--loop` |
| `release.wav` | https://kenney.nl/assets/impact-sounds | `impactSoft_heavy_000.ogg` | Kenney (kenney.nl) | CC0 1.0 | |
| `strike.wav` | https://freesound.org/s/541985/ | `GASP_Chimes_Success_4.wav` (preview) | Rob_Marion | CC0 1.0 | `--rate 44100` |
| `spare.wav` | https://freesound.org/s/611113/ | `bell ding 1.wav` (preview) | 5ro4 | CC0 1.0 | `--start 0.04 --length 1.6 --rate 44100` |
| `new_game.wav` | https://kenney.nl/assets/interface-sounds | `confirmation_001.ogg` | Kenney (kenney.nl) | CC0 1.0 | |
| `theme_switch.wav` | https://kenney.nl/assets/interface-sounds | `switch_002.ogg` | Kenney (kenney.nl) | CC0 1.0 | |
| `ambient_neon.wav` | https://freesound.org/s/655942/ | `Arcade Ambience FX.wav` (preview) | Santi171 | CC0 1.0 | `--start 76 --length 32 --loop --xfade 2 --rms -20` |
| `ambient_classic.wav` | https://freesound.org/s/667757/ | `Bowling Alley Ambience 01b.wav` (preview) | dagray | CC0 1.0 | `--start 832 --length 42 --loop --xfade 2 --rms -20 --limit` |
| `ambient_outdoor.wav` | https://freesound.org/s/459489/ | `GARDEN - 2` (preview) | SamuelGremaud | CC0 1.0 | `--start 0 --length 35 --loop --xfade 2 --rms -20 --limit` (birds only; an ambulance starts at 35 s) |
