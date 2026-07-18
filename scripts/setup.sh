#!/bin/bash

declare -A remotes=(
    [upstream]="https://github.com/Keychron/qmk_firmware.git"
    [signalrgb]="https://github.com/SRGBmods/KeychronQMK.git"
)

for name in "${!remotes[@]}"; do
    git remote get-url "$name" >/dev/null 2>&1 || git remote add "$name" "${remotes[$name]}"
done

git config --local alias.setup '!./scripts/setup.sh'
git config --local alias.sync '!./scripts/sync.sh'
git config --local alias.setup-sparse '!./scripts/sparse-checkout.sh'

qmk config user.keyboard=keychron/k11_max/ansi_encoder/rgb
qmk config user.keymap=nabellows
