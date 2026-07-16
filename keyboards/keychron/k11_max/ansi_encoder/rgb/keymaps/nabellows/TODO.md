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

## Other
- GUI which-key helper of some kind would be wild, probably just a wrapper around launcher/via
- Should I just/also use kanata to make gaming keyboard usable? If so, why even use QMK for much stuff besides keyboard specific features like lights, etc? 


