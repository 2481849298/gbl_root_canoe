/*
 * Software-rendered fastboot UI: a dark gradient scene with a cyan accent glow
 * and an anti-aliased "FASTBOOT" title, plus a time-based fade/slide-in so the
 * transition into fastboot looks smooth regardless of frame rate.
 *
 * Everything is integer-only (no floating point in the bootloader) and fully
 * defensive: without a usable Graphics Output Protocol every call is a no-op.
 */

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>

#include "fastboot_ui.h"
#include "fastboot_ui_font.h"

/* Millisecond clock provided by LinuxLoaderLib. */
extern UINT64 GetTimerCountms (VOID);

typedef EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel;

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL *mGop;
STATIC UINT32 mW;
STATIC UINT32 mH;
STATIC Pixel *mBack;     /* compositing back buffer */

/* Palette (R, G, B). */
#define BG_TOP_R 10
#define BG_TOP_G 14
#define BG_TOP_B 26
#define BG_BOT_R 4
#define BG_BOT_G 6
#define BG_BOT_B 12
#define ACCENT_R 34
#define ACCENT_G 211
#define ACCENT_B 238
#define TITLE_R 236
#define TITLE_G 241
#define TITLE_B 252
#define DIM_R 120
#define DIM_G 140
#define DIM_B 170

STATIC UINT8
Clamp8 (INT32 v)
{
  if (v < 0)   return 0;
  if (v > 255) return 255;
  return (UINT8)v;
}

/* Locate GOP and allocate the back buffer. Returns FALSE if unavailable. */
STATIC BOOLEAN
UiInit (VOID)
{
  EFI_STATUS Status;

  mGop  = NULL;
  mBack = NULL;
  Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL,
                                (VOID **)&mGop);
  if (EFI_ERROR (Status) || mGop == NULL || mGop->Mode == NULL ||
      mGop->Mode->Info == NULL) {
    mGop = NULL;
    return FALSE;
  }

  mW = mGop->Mode->Info->HorizontalResolution;
  mH = mGop->Mode->Info->VerticalResolution;
  if (mW == 0 || mH == 0 || mW > 8192 || mH > 8192) {
    mGop = NULL;
    return FALSE;
  }

  mBack = AllocatePool ((UINTN)mW * mH * sizeof (Pixel));
  if (mBack == NULL) {
    mGop = NULL;
    return FALSE;
  }
  return TRUE;
}

STATIC VOID
UiFree (VOID)
{
  if (mBack != NULL) {
    FreePool (mBack);
    mBack = NULL;
  }
  mGop = NULL;
}

STATIC VOID
Present (VOID)
{
  mGop->Blt (mGop, mBack, EfiBltBufferToVideo, 0, 0, 0, 0, mW, mH, 0);
}

/* alpha is 0..255; blends (r,g,b) over the existing pixel. */
STATIC VOID
BlendPixel (Pixel *p, INT32 r, INT32 g, INT32 b, INT32 alpha)
{
  INT32 ia = 255 - alpha;
  p->Red      = Clamp8 ((r * alpha + p->Red * ia)   / 255);
  p->Green    = Clamp8 ((g * alpha + p->Green * ia) / 255);
  p->Blue     = Clamp8 ((b * alpha + p->Blue * ia)  / 255);
  p->Reserved = 0;
}

STATIC VOID
FillBlack (VOID)
{
  ZeroMem (mBack, (UINTN)mW * mH * sizeof (Pixel));
}

/* Vertical gradient over the whole frame. */
STATIC VOID
DrawGradientBg (VOID)
{
  UINT32 x, y;
  for (y = 0; y < mH; y++) {
    /* interpolate top->bottom by row */
    INT32 r = BG_TOP_R + (INT32)(BG_BOT_R - BG_TOP_R) * (INT32)y / (INT32)mH;
    INT32 g = BG_TOP_G + (INT32)(BG_BOT_G - BG_TOP_G) * (INT32)y / (INT32)mH;
    INT32 b = BG_TOP_B + (INT32)(BG_BOT_B - BG_TOP_B) * (INT32)y / (INT32)mH;
    Pixel *row = &mBack[(UINTN)y * mW];
    Pixel px;
    px.Red = Clamp8 (r); px.Green = Clamp8 (g); px.Blue = Clamp8 (b);
    px.Reserved = 0;
    for (x = 0; x < mW; x++) {
      row[x] = px;
    }
  }
}

/* Soft radial glow centered at (cx,cy), squared falloff, no sqrt. */
STATIC VOID
DrawGlow (INT32 cx, INT32 cy, INT32 radius, INT32 r, INT32 g, INT32 b,
          INT32 maxAlpha)
{
  INT32 x, y;
  INT64 r2 = (INT64)radius * radius;
  if (r2 == 0) {
    return;
  }
  for (y = cy - radius; y < cy + radius; y++) {
    if (y < 0 || y >= (INT32)mH) {
      continue;
    }
    for (x = cx - radius; x < cx + radius; x++) {
      if (x < 0 || x >= (INT32)mW) {
        continue;
      }
      INT64 dx = x - cx;
      INT64 dy = y - cy;
      INT64 d2 = dx * dx + dy * dy;
      if (d2 >= r2) {
        continue;
      }
      /* factor falls off with the square of the normalized distance */
      INT64 f = (r2 - d2) * 255 / r2;     /* 0..255 */
      INT64 a = (INT64)maxAlpha * f / 255 * f / 255;
      BlendPixel (&mBack[(UINTN)y * mW + x], r, g, b, (INT32)a);
    }
  }
}

/* Filled rectangle with constant alpha. */
STATIC VOID
BlendRect (INT32 x0, INT32 y0, INT32 w, INT32 h, INT32 r, INT32 g, INT32 b,
           INT32 alpha)
{
  INT32 x, y;
  if (alpha <= 0) {
    return;
  }
  for (y = y0; y < y0 + h; y++) {
    if (y < 0 || y >= (INT32)mH) {
      continue;
    }
    for (x = x0; x < x0 + w; x++) {
      if (x < 0 || x >= (INT32)mW) {
        continue;
      }
      BlendPixel (&mBack[(UINTN)y * mW + x], r, g, b, alpha);
    }
  }
}

STATIC UINT32
TextWidth (CONST CHAR8 *s, UINT32 scale)
{
  UINT32 n = 0;
  while (s[n] != '\0') {
    n++;
  }
  return n * FONT_CELL_W * scale;
}

/* Draw an ASCII string, nearest-neighbour upscaled, alpha-blended with color.
 * globalAlpha 0..255 fades the whole string. */
STATIC VOID
DrawText (INT32 x, INT32 y, CONST CHAR8 *s, INT32 r, INT32 g, INT32 b,
          INT32 globalAlpha, UINT32 scale)
{
  if (globalAlpha <= 0 || scale == 0) {
    return;
  }
  for (; *s != '\0'; s++) {
    CHAR8 c = *s;
    if (c < FONT_FIRST_CHAR || c > FONT_LAST_CHAR) {
      c = ' ';
    }
    CONST UINT8 *glyph = &gFontAtlas[(c - FONT_FIRST_CHAR) *
                                     (FONT_CELL_W * FONT_CELL_H)];
    UINT32 gx, gy, sx, sy;
    for (gy = 0; gy < FONT_CELL_H; gy++) {
      for (gx = 0; gx < FONT_CELL_W; gx++) {
        UINT8 cov = glyph[gy * FONT_CELL_W + gx];
        if (cov == 0) {
          continue;
        }
        INT32 a = cov * globalAlpha / 255;
        if (a <= 0) {
          continue;
        }
        INT32 px = x + (INT32)(gx * scale);
        INT32 py = y + (INT32)(gy * scale);
        for (sy = 0; sy < scale; sy++) {
          INT32 yy = py + (INT32)sy;
          if (yy < 0 || yy >= (INT32)mH) {
            continue;
          }
          for (sx = 0; sx < scale; sx++) {
            INT32 xx = px + (INT32)sx;
            if (xx < 0 || xx >= (INT32)mW) {
              continue;
            }
            BlendPixel (&mBack[(UINTN)yy * mW + xx], r, g, b, a);
          }
        }
      }
    }
    x += (INT32)(FONT_CELL_W * scale);
  }
}

STATIC VOID
DrawTextCentered (INT32 cy, CONST CHAR8 *s, INT32 r, INT32 g, INT32 b,
                  INT32 alpha, UINT32 scale)
{
  INT32 w = (INT32)TextWidth (s, scale);
  DrawText (((INT32)mW - w) / 2, cy, s, r, g, b, alpha, scale);
}

/* easeOutCubic on a 0..1000 parameter, returns 0..1000. */
STATIC UINT32
EaseOutCubic (UINT32 t)
{
  UINT32 u = 1000 - t;
  return 1000 - (u * u / 1000 * u / 1000);
}

STATIC INT32
Lerp (INT32 a, INT32 b, UINT32 t /*0..1000*/)
{
  return a + (b - a) * (INT32)t / 1000;
}

/* Compose the settled scene (gradient + glow + accent + title) into mBack.
 * sceneAlpha fades the whole scene in; titleY/titleAlpha animate the title. */
STATIC VOID
ComposeScene (INT32 sceneAlpha, INT32 titleY, INT32 titleAlpha)
{
  UINT32 titleScale = (mW >= 1000) ? 3 : 2;
  UINT32 subScale   = (mW >= 1000) ? 2 : 1;
  INT32  glowCx     = (INT32)mW / 2;
  INT32  glowCy     = (INT32)mH * 38 / 100;
  INT32  glowR      = (INT32)((mW < mH ? mW : mH) * 45 / 100);
  INT32  accentW    = (INT32)mW * 22 / 100;
  INT32  accentY    = titleY + (INT32)(FONT_CELL_H * titleScale) + 24;

  FillBlack ();

  /* The whole scene fades in via sceneAlpha by drawing onto black with that
   * alpha; gradient first (opaque base), then glow/accent on top. */
  DrawGradientBg ();
  DrawGlow (glowCx, glowCy, glowR, ACCENT_R, ACCENT_G, ACCENT_B, 70);

  if (sceneAlpha < 255) {
    /* darken everything toward black by (255-sceneAlpha) */
    BlendRect (0, 0, (INT32)mW, (INT32)mH, 0, 0, 0, 255 - sceneAlpha);
  }

  /* Cyan halo behind the title for a soft glow. */
  DrawGlow (glowCx, titleY + (INT32)(FONT_CELL_H * titleScale) / 2,
            (INT32)(FONT_CELL_H * titleScale), ACCENT_R, ACCENT_G, ACCENT_B,
            titleAlpha * 60 / 255);

  DrawTextCentered (titleY, "FASTBOOT", TITLE_R, TITLE_G, TITLE_B, titleAlpha,
                    titleScale);

  /* Accent underline that grows with the title fade. */
  BlendRect (glowCx - accentW * titleAlpha / 255 / 2, accentY,
             accentW * titleAlpha / 255, 4 * (INT32)subScale,
             ACCENT_R, ACCENT_G, ACCENT_B, titleAlpha);

  DrawTextCentered (accentY + 24, "superfastboot", DIM_R, DIM_G, DIM_B,
                    titleAlpha * 200 / 255, subScale);
}

VOID
FastbootUiEntryTransition (VOID)
{
  UINT64 start, elapsed;
  CONST UINT64 dur = 750;   /* ms */
  /* Hard frame cap so the loop always terminates even if the ms timer never
   * advances (otherwise we would hang before entering fastboot). */
  CONST UINT32 maxFrames = 180;
  UINT32 frames = 0;
  INT32  titleEndY = (INT32)mH * 38 / 100;
  INT32  titleStartY;

  if (!UiInit ()) {
    return;
  }
  titleEndY = (INT32)mH * 38 / 100;
  titleStartY = titleEndY + (INT32)mH / 12;

  start = GetTimerCountms ();
  for (;;) {
    elapsed = GetTimerCountms () - start;
    if (elapsed >= dur || frames >= maxFrames) {
      break;
    }
    frames++;
    UINT32 p = EaseOutCubic ((UINT32)(elapsed * 1000 / dur));
    INT32  sceneA = Lerp (0, 255, p);
    INT32  titleA = Lerp (0, 255, EaseOutCubic ((UINT32)((p > 200) ?
                          (p - 200) * 1000 / 800 : 0)));
    INT32  titleY = Lerp (titleStartY, titleEndY, p);

    ComposeScene (sceneA, titleY, titleA);
    Present ();
    gBS->Stall (4000);
  }

  /* settle on the final frame */
  ComposeScene (255, titleEndY, 255);
  Present ();
  gBS->Stall (120000);   /* brief hold so the reveal reads as deliberate */

  UiFree ();
}

VOID
FastbootUiDrawMain (VOID)
{
  INT32 titleY;

  if (!UiInit ()) {
    return;
  }
  titleY = (INT32)mH * 38 / 100;
  ComposeScene (255, titleY, 255);

  /* status footer */
  DrawTextCentered ((INT32)mH - 70, "device connected . waiting for host",
                    DIM_R, DIM_G, DIM_B, 220, 1);

  Present ();
  UiFree ();
}
