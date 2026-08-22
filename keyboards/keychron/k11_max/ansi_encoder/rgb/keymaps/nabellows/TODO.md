# TODO:
## Firmware
- Fix left space not treated as cmd with mouse click til after the TAP_TERM feature expires (tap vs hold duration). This makes sense because though a *keypress* will immediately trigger as a combo, mouse will not. Make the key treated as holding cmd, but also treated as space if quickly released 
    - Idea works great on mac, but as win key, win tap DOES have destructive effects so my naive solution of registering key down as cmd/win down may not work. Similarly, the space lag is not ideal... how often do I use left-hand space? Happens on occasion but I rarely notice
- Move F keys to fn1 because I am much more likely to use f1/etc left-hand only such as even games, meanwhile OS/GUI-level maps i typically have both hands on keys
    - Works even better for media control on right hand anyway
- True CAPS_LOCK key somewhere? Used in games for example, plus its a key and useful on occasion
    - Should probably be on fn1 and make a new bind for rgb toggle
    - OR, fn1->caps should be the smart, and double shift is the regular? Righty shift is such a weak muscle mem
- CAPS_LOCK light for smart lock? Different color? one for both?
- Arrow keys HJKL
- A layer to act like numpad? jkl->123, uio->456? (or shifted down 1)

- Could implement a which-key help button which lights up layer sections which are mapped like in below
    - [link](https://github.com/qmk/qmk_firmware/blob/master/docs/features/rgb_matrix.md#indicator-examples-indicator-examples)
    - Could light keys by which layer

- Custom function to restore certain things as firmware-default eprom settings like snap-tap a/d? 
- Use magic or other to actually implement toggle for invasive keymaps and/or a normie layer like for gaming (make caps lock caps lock again, no interruption to shift and others?)
- A key to put caps word into super sticky mode where stuff like comma does not interrupt. I guess thats not caps word no more, but shift space...
- Move lots of constexpr to inline constexpr (for globals that arent meant to actually extern)

- Make a "weak combo" or whatever we end up calling it for gui-space and ctl-esc to be nav layer? Am I really such a baby that reaching thumb to fn1 isnt the same muscle memory as ctrl hjkl
    - Insane idea, map ctrl hjkl to arrows completely on keyboard? Nah that would kinda break some stuff in nvim and other places like valid ctrl-hjkl differences, lazygit, shell, etc

- 'Strict' mode in the util is kinda shit/overloaded, would be better to pass orthoganal/vararg enum policy flags or a policy struct (NTTP or function arg?)

- factory reset + inits (snap click)
    - Make snap-action set for normie/gaming keyboard layer and maybe not others? tbh i think no issue with a/d in moooost applications

- figure out how to use lights that aren't "indicator" flag even when rgb off for caps word.... i guess declare them as indicator

## Other
- GUI which-key helper of some kind would be wild, probably just a wrapper around launcher/via
- Should I just/also use kanata to make gaming keyboard usable? If so, why even use QMK for much stuff besides keyboard specific features like lights, etc? 
    - Well, software-level versions are kinda iffy in the experience so far...
