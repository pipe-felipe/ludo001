## BLOCK 3 — Complete the OLED font with every letter, '-' and '%'

**Depends on:** BLOCK 2 committed
**Touches:** `components/framebuffer/framebuffer.cpp` (MODIFY), `components/framebuffer/include/framebuffer.hpp` (MODIFY), `host_tests/main/test_framebuffer.cpp` (MODIFY), `host_tests/main/test_main.cpp` (MODIFY)
4 files: the glyphs, the header comment that lists the supported characters, their 3 tests, and the test registration.

### Goal
`Framebuffer::draw_char` draws every letter A–Z plus `-` and `%`, which the OLED needs for `ATUALIZANDO 42%` and for firmware versions such as `514d68f-dirty`. 3 new host tests pass, for 95 in total.

### Context to read first
1. `components/framebuffer/framebuffer.cpp:1-60`: the segment font. Each glyph is a `constexpr Segment kGlyphX[]` list of `{x0, y0, x1, y1}` lines in a 5×7 cell. A glyph that wraps continues on a line indented to align after `= {`, like `kGlyphO`.
2. `components/framebuffer/framebuffer.cpp:136-190`: `draw_char`'s `switch`, with one `case` per glyph.
3. `host_tests/main/test_framebuffer.cpp:1-20` and `:167-185`: the `all_bytes_zero` helper and the `test_draw_char_*` style.

Read exactly these. Do not open other files unless a step below names one.

### Steps
1. In `components/framebuffer/framebuffer.cpp`, replace these 3 comment lines
   ```cpp
   // bitmap font table. Digits reuse a classic 7-segment layout; letters are
   // hand-drawn block strokes. Only the characters this project's status
   // messages need are defined; anything else is skipped by draw_char().
   ```
   with
   ```cpp
   // bitmap font table. Digits reuse a classic 7-segment layout; letters are
   // hand-drawn block strokes. Covers A-Z, 0-9 and the few symbols the status
   // screen and firmware versions use; anything else is skipped by
   // draw_char().
   ```
2. In the same file, replace every line from the line that starts with `constexpr Segment kGlyphA[]` through the line that starts with `constexpr Segment kGlyphW[]` (15 lines; `kGlyphO` takes two) with exactly this block. The double braces are C++ initializers: type them exactly as shown.
   ```cpp
   constexpr Segment kGlyphA[] = {{0, 6, 2, 0}, {2, 0, 4, 6}, {1, 3, 3, 3}};
   constexpr Segment kGlyphB[] = {{0, 0, 0, 6}, {0, 0, 3, 0}, {3, 0, 4, 1}, {4, 1, 4, 2}, {4, 2, 3, 3}, {0, 3, 3, 3},
                                   {3, 3, 4, 4}, {4, 4, 4, 5}, {4, 5, 3, 6}, {3, 6, 0, 6}};
   constexpr Segment kGlyphC[] = {{3, 0, 0, 0}, {0, 0, 0, 6}, {0, 6, 3, 6}};
   constexpr Segment kGlyphD[] = {{0, 0, 0, 6}, {0, 0, 2, 0}, {2, 0, 4, 2}, {4, 2, 4, 4}, {4, 4, 2, 6}, {2, 6, 0, 6}};
   constexpr Segment kGlyphE[] = {{0, 0, 0, 6}, {0, 0, 4, 0}, {0, 3, 3, 3}, {0, 6, 4, 6}};
   constexpr Segment kGlyphF[] = {{0, 0, 0, 6}, {0, 0, 4, 0}, {0, 3, 3, 3}};
   constexpr Segment kGlyphG[] = {{4, 0, 0, 0}, {0, 0, 0, 6}, {0, 6, 4, 6}, {4, 6, 4, 3}, {4, 3, 2, 3}};
   constexpr Segment kGlyphH[] = {{0, 0, 0, 6}, {4, 0, 4, 6}, {0, 3, 4, 3}};
   constexpr Segment kGlyphI[] = {{0, 0, 4, 0}, {2, 0, 2, 6}, {0, 6, 4, 6}};
   constexpr Segment kGlyphJ[] = {{4, 0, 4, 6}, {4, 6, 0, 6}, {0, 6, 0, 4}};
   constexpr Segment kGlyphK[] = {{0, 0, 0, 6}, {4, 0, 0, 3}, {0, 3, 4, 6}};
   constexpr Segment kGlyphL[] = {{0, 0, 0, 6}, {0, 6, 4, 6}};
   constexpr Segment kGlyphM[] = {{0, 6, 0, 0}, {0, 0, 2, 3}, {2, 3, 4, 0}, {4, 0, 4, 6}};
   constexpr Segment kGlyphN[] = {{0, 6, 0, 0}, {0, 0, 4, 6}, {4, 6, 4, 0}};
   constexpr Segment kGlyphO[] = {{1, 0, 3, 0}, {3, 0, 4, 1}, {4, 1, 4, 5}, {4, 5, 3, 6},
                                   {3, 6, 1, 6}, {1, 6, 0, 5}, {0, 5, 0, 1}, {0, 1, 1, 0}};
   constexpr Segment kGlyphP[] = {{0, 6, 0, 0}, {0, 0, 3, 0}, {3, 0, 3, 3}, {3, 3, 0, 3}};
   constexpr Segment kGlyphQ[] = {{1, 0, 3, 0}, {3, 0, 4, 1}, {4, 1, 4, 5}, {4, 5, 3, 6},
                                   {3, 6, 1, 6}, {1, 6, 0, 5}, {0, 5, 0, 1}, {0, 1, 1, 0}, {2, 4, 4, 6}};
   constexpr Segment kGlyphR[] = {{0, 6, 0, 0}, {0, 0, 3, 0}, {3, 0, 3, 3}, {3, 3, 0, 3}, {1, 3, 4, 6}};
   constexpr Segment kGlyphS[] = {{4, 0, 0, 0}, {0, 0, 0, 3}, {0, 3, 4, 3}, {4, 3, 4, 6}, {4, 6, 0, 6}};
   constexpr Segment kGlyphT[] = {{0, 0, 4, 0}, {2, 0, 2, 6}};
   constexpr Segment kGlyphU[] = {{0, 0, 0, 6}, {0, 6, 4, 6}, {4, 6, 4, 0}};
   constexpr Segment kGlyphV[] = {{0, 0, 2, 6}, {2, 6, 4, 0}};
   constexpr Segment kGlyphW[] = {{0, 0, 1, 6}, {1, 6, 2, 3}, {2, 3, 3, 6}, {3, 6, 4, 0}};
   constexpr Segment kGlyphX[] = {{0, 0, 4, 6}, {4, 0, 0, 6}};
   constexpr Segment kGlyphY[] = {{0, 0, 2, 3}, {4, 0, 2, 3}, {2, 3, 2, 6}};
   constexpr Segment kGlyphZ[] = {{0, 0, 4, 0}, {4, 0, 0, 6}, {0, 6, 4, 6}};

   constexpr Segment kGlyphMinus[] = {kSegMiddle};
   constexpr Segment kGlyphPercent[] = {{0, 6, 4, 0}, {0, 0, 1, 0}, {0, 1, 1, 1}, {3, 5, 4, 5}, {3, 6, 4, 6}};
   ```
3. In the same file, inside `Framebuffer::draw_char`, replace every line from `        case 'A': select_glyph(segs, count, kGlyphA); break;` through `        case 'W': select_glyph(segs, count, kGlyphW); break;` (14 lines) with exactly these 28 lines:
   ```cpp
           case 'A': select_glyph(segs, count, kGlyphA); break;
           case 'B': select_glyph(segs, count, kGlyphB); break;
           case 'C': select_glyph(segs, count, kGlyphC); break;
           case 'D': select_glyph(segs, count, kGlyphD); break;
           case 'E': select_glyph(segs, count, kGlyphE); break;
           case 'F': select_glyph(segs, count, kGlyphF); break;
           case 'G': select_glyph(segs, count, kGlyphG); break;
           case 'H': select_glyph(segs, count, kGlyphH); break;
           case 'I': select_glyph(segs, count, kGlyphI); break;
           case 'J': select_glyph(segs, count, kGlyphJ); break;
           case 'K': select_glyph(segs, count, kGlyphK); break;
           case 'L': select_glyph(segs, count, kGlyphL); break;
           case 'M': select_glyph(segs, count, kGlyphM); break;
           case 'N': select_glyph(segs, count, kGlyphN); break;
           case 'O': select_glyph(segs, count, kGlyphO); break;
           case 'P': select_glyph(segs, count, kGlyphP); break;
           case 'Q': select_glyph(segs, count, kGlyphQ); break;
           case 'R': select_glyph(segs, count, kGlyphR); break;
           case 'S': select_glyph(segs, count, kGlyphS); break;
           case 'T': select_glyph(segs, count, kGlyphT); break;
           case 'U': select_glyph(segs, count, kGlyphU); break;
           case 'V': select_glyph(segs, count, kGlyphV); break;
           case 'W': select_glyph(segs, count, kGlyphW); break;
           case 'X': select_glyph(segs, count, kGlyphX); break;
           case 'Y': select_glyph(segs, count, kGlyphY); break;
           case 'Z': select_glyph(segs, count, kGlyphZ); break;
           case '-': select_glyph(segs, count, kGlyphMinus); break;
           case '%': select_glyph(segs, count, kGlyphPercent); break;
   ```
4. In `components/framebuffer/include/framebuffer.hpp`, replace these 4 comment lines
   ```cpp
       // Stroke-font text. Supports 'A'-'Z'/'a'-'z' (rendered as uppercase),
       // '0'-'9', ' ', '.', '/' and ':'; unsupported characters are skipped
       // (cursor still advances). Each glyph is kGlyphWidth x kGlyphHeight,
       // advancing by kGlyphAdvance per character.
   ```
   with
   ```cpp
       // Stroke-font text. Supports 'A'-'Z'/'a'-'z' (rendered as uppercase),
       // '0'-'9', ' ', '.', '/', ':', '-' and '%'; unsupported characters are
       // skipped (cursor still advances). Each glyph is kGlyphWidth x
       // kGlyphHeight, advancing by kGlyphAdvance per character.
   ```
5. At the end of `host_tests/main/test_framebuffer.cpp`, after the closing `}` of `test_draw_char_slash_is_diagonal`, append a blank line and these 3 tests:
   ```cpp
   void test_draw_char_every_letter_lights_some_pixels() {
       for (char c = 'A'; c <= 'Z'; ++c) {
           Framebuffer fb;
           fb.draw_char(0, 0, c);
           const char letter[] = {c, '\0'};
           TEST_ASSERT_FALSE_MESSAGE(all_bytes_zero(fb), letter);
       }
   }

   void test_draw_char_minus_is_middle_line() {
       Framebuffer fb;
       fb.draw_char(0, 0, '-');
       TEST_ASSERT_TRUE(fb.get_pixel(0, 3));
       TEST_ASSERT_TRUE(fb.get_pixel(Framebuffer::kGlyphWidth - 1, 3));
       TEST_ASSERT_FALSE(fb.get_pixel(2, 0));
   }

   void test_draw_char_percent_has_slash_and_two_dots() {
       Framebuffer fb;
       fb.draw_char(0, 0, '%');
       TEST_ASSERT_TRUE(fb.get_pixel(2, 3)); // middle of the slash
       TEST_ASSERT_TRUE(fb.get_pixel(0, 0)); // top-left dot
       TEST_ASSERT_TRUE(fb.get_pixel(Framebuffer::kGlyphWidth - 1, Framebuffer::kGlyphHeight - 1)); // bottom-right dot
   }
   ```
   The file must end with a single newline.
6. In `host_tests/main/test_main.cpp`, immediately after the line `void test_draw_char_slash_is_diagonal();`, insert:
   ```cpp
   void test_draw_char_every_letter_lights_some_pixels();
   void test_draw_char_minus_is_middle_line();
   void test_draw_char_percent_has_slash_and_two_dots();
   ```
7. In `host_tests/main/test_main.cpp`, immediately after the line `    RUN_TEST(test_draw_char_slash_is_diagonal);`, insert:
   ```cpp
       RUN_TEST(test_draw_char_every_letter_lights_some_pixels);
       RUN_TEST(test_draw_char_minus_is_middle_line);
       RUN_TEST(test_draw_char_percent_has_slash_and_two_dots);
   ```

### Do not
- Do not replace the segment lists with a bitmap table (§9).
- Do not change the existing glyphs' segments, `kGlyphWidth`, `kGlyphHeight` or `kGlyphAdvance`. The block in step 2 repeats the existing glyphs unchanged.
- Do not add glyphs beyond the ones in step 2. `'?'` must stay unsupported, because `test_draw_char_unsupported_is_noop` depends on it.
- Do not touch `main/ludo001.cpp`; the OLED lines are BLOCK 5.

### Verify
Run from the repository root, in this order:
```
source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; export PATH="/usr/bin:$PATH"; idf.py -C host_tests build > build/host-tests-build.log 2>&1; echo "host build exit=$?"; ./host_tests/build/ludo001_host_tests.elf | tail -3
grep -c "select_glyph(segs, count, kGlyph" components/framebuffer/framebuffer.cpp
source ~/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1; idf.py build > build/fw-build.log 2>&1; echo "fw build exit=$?"; grep -E 'warning:|binary size' build/fw-build.log
```
Expected:
1. `host build exit=0`, `95 Tests 0 Failures 0 Ignored`, `OK`.
2. `38` (10 digits + 26 letters + `-` + `%`).
3. `fw build exit=0`, no `warning:` line, and a `ludo001.bin binary size ...` line that shows partition `0x1a9000` with at least `0x90000` bytes free.

### If verification fails
1. Read the failing output in full.
2. Fix only files listed in **Touches**.
3. Re-run every command in **Verify**.
4. After 2 failed attempts, follow §11 R12: mark this block `BLOCKED` in §12, append the exact failing output under `## Blocked` in `PLAN.md`, commit only `PLAN.md`, and stop.

### Commit
1. Set BLOCK 3's row in §12 Status to `DONE`.
2. Run:
   ```
   git add components/framebuffer/framebuffer.cpp components/framebuffer/include/framebuffer.hpp host_tests/main/test_framebuffer.cpp host_tests/main/test_main.cpp PLAN.md
   git commit -m "Complete the OLED font with every letter, '-' and '%'"
   ```

### Next
Go to §12 Status, take the first row still marked `TODO`, re-read §11 EXECUTOR CONTRACT,
then start that block. If no row is `TODO`, stop and report to the human.
