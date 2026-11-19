# Eightfold

Four 8-channel modules for VCV Rack 2, inspired by Make Noise's New Universal Skiff System:

- **EFG** Eightfold Function Generator (after PoliMATHS)
- **ETS** Eightfold Time Smear (after MultiMod)
- **EGX** Eightfold Gesture Extractor (after GTE)
- **JumbleSale** 8 in, 8 out routing network (after Jumbler)

Independent behavioural re-imaginings written from the public manuals; not affiliated with Make Noise.

- Manual: [docs/MANUAL.md](docs/MANUAL.md), also as [docs/Eightfold-Manual.pdf](docs/Eightfold-Manual.pdf)
- Build spec: [docs/BUILD-INSTRUCTIONS.md](docs/BUILD-INSTRUCTIONS.md); research: [docs/RESEARCH.md](docs/RESEARCH.md); choices made while building: [docs/DECISIONS.md](docs/DECISIONS.md)

## Install (Mac, Apple silicon)

1. Download `Eightfold-2.1.0-mac-arm64.vcvplugin` from the latest [release](https://github.com/hardyfinn/NUSS/releases) (or from the artifacts of the latest Actions run).
2. Put it in Rack's user plugins folder: `~/Library/Application Support/Rack2/plugins-mac-arm64/` (in Rack: Help > Open user folder).
3. Restart Rack. Search the module browser for `EFG`, `ETS`, `EGX`, `JumbleSale` or `Eightfold`.

## Build from source

    export RACK_DIR=/path/to/Rack-SDK
    make dist          # dist/Eightfold-*.vcvplugin
    make test          # engine tests, no Rack SDK needed
    make panels        # regenerate res/*.svg and src/generated/Layout.hpp (needs fonttools)

GitHub Actions builds the Mac arm64 plugin on every push and attaches it to a release for tags `v*`.
