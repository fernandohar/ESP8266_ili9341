#ifndef _SPRITETEXT_H_
#define _SPRITETEXT_H_

#include "Avatar.h"
#include "GameScene.h"
#include "SpriteSheet.h"
#include "sprite_letters.h"

#define SPRITE_LETTERS_CELL_W 24
#define SPRITE_LETTERS_CELL_H 20
#define SPRITE_TEXT_MAX_GLYPHS 32

class SpriteText {
  public:
    static int letterRegionIndex(char ch) {
      if (ch >= 'A' && ch <= 'Z') {
        return ch - 'A';
      }
      if (ch >= 'a' && ch <= 'z') {
        return ch - 'a';
      }
      if (ch == '?') {
        return 26;
      }
      if (ch == '!') {
        return 27;
      }
      if (ch == '#') {
        return 28;
      }
      if (ch == '.') {
        return 29;
      }
      if (ch == '-') {
        return 30;
      }
      return -1;
    }

    static int measureWidth(const char *text, int gap = 2) {
      int width = 0;
      for (const char *p = text; *p != '\0'; ++p) {
        if (*p == ' ') {
          width += SPRITE_LETTERS_CELL_W / 2;
          continue;
        }
        if (letterRegionIndex(*p) < 0) {
          continue;
        }
        width += SPRITE_LETTERS_CELL_W + gap;
      }
      if (width > 0) {
        width -= gap;
      }
      return width;
    }

    static SpriteSheet letterSheet() {
      return SpriteSheet(sprite_letters, sprite_lettersMask, SPRITE_LETTERS_WIDTH, SPRITE_LETTERS_HEIGHT);
    }

    static int buildLine(GameScene *scene, const char *text, int x, int y, Avatar *out[], int maxOut, int gap = 2) {
      int count = 0;
      int cursor = x;
      SpriteSheet sheet = letterSheet();

      for (const char *p = text; *p != '\0' && count < maxOut; ++p) {
        if (*p == ' ') {
          cursor += SPRITE_LETTERS_CELL_W / 2;
          continue;
        }
        int index = letterRegionIndex(*p);
        if (index < 0) {
          continue;
        }
        SpriteSheetRegion region = SpriteSheet::readRegion(sprite_lettersRegions, index);
        Avatar *glyph = sheet.createAvatar((float)cursor, (float)y, region);
        scene->appendAvatar(glyph);
        out[count++] = glyph;
        cursor += SPRITE_LETTERS_CELL_W + gap;
      }
      return count;
    }

    static int buildCenteredLine(GameScene *scene, const char *text, int y, Avatar *out[], int maxOut, int gap = 2) {
      int width = measureWidth(text, gap);
      int x = (SCREENWIDTH - width) / 2;
      return buildLine(scene, text, x, y, out, maxOut, gap);
    }
};

#endif
