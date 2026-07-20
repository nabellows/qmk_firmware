#!/bin/bash
keyboards=("${@:-keychron/k11_max}")
for i in "${!keyboards[@]}"; do
    if [[ "${keyboards[$i]}" != */* ]]; then
        keyboards[i]="prefix${keyboards[$i]}"
    fi
done
git sparse-checkout set builddefs/ data/ docs/ drivers/ keyboards/keychron/common "${keyboards[@]/#/keyboards/}" layouts/ lib/ modules/ platforms/ quantum/ tests/ tmk_core/ users/ util/ scripts/ templates/
