# GameBase Sprite Converter (browser)

Open `index.html` in a browser (local file is fine). Choose one or more PNG files,
pick 4/8/16-bit depth, preview the quantized result, and download a `.h` header.

Features:

- 4-bit, 8-bit, or 16-bit RGB565 palette sprites
- 1-bit transparency mask (same model as the firmware renderer)
- Multiple PNG frames packed into one sheet with shared palette
- Optional legacy 16-bit output matching older `png_to_spritesheet.py` headers

Generated unified headers include:

- `SpriteAsset` descriptor (`sprite_foo`)
- `sprite_fooPalette[]` for 4/8-bit images
- `sprite_fooPixels[]` indices or RGB565 values
- `sprite_fooMask[]`
- `sprite_fooBitmaps[]` sub-rectangle metadata

Use in scenes:

```cpp
#include "sprite_foo.h"

SpriteSheet sheet(&sprite_foo);
Avatar *avatar = sheet.createAvatar(
    x, y, SpriteSheet::readBitmapRegion(sprite_fooBitmaps, 0));
scene->appendAvatar(avatar);
```

Existing legacy headers continue to work unchanged.
