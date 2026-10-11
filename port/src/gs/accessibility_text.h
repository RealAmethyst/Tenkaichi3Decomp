#ifndef ACCESSIBILITY_TEXT_H
#define ACCESSIBILITY_TEXT_H

/* English artwork transcriptions, verified from the USA disc by research/inspect_startup.py.
   No underlying text table is used by these picture/texture consumers. Keep translations here,
   separate from handlers. Dialog body/heading text comes directly from the game's own tables. */
static const char *const kA11yLogo[] = {
    NULL, NULL, NULL,
    "BANDAI NAMCO Games",
    "Spike",
    "CRIWARE. Technology by ADX, Sofdec.",
    "ATARI",
    "FUNimation Entertainment. A Navarre Corporation Company.",
    "OKRATRON 5000. Game, vox, music, SFX."
};
static const char kA11yLegal[] =
    "Copyright 2007 Atari, Inc. All rights reserved. "
    "Copyright 2007 BIRD STUDIO/SHUEISHA, TOEI ANIMATION. Licensed by FUNimation Productions, Ltd. All Rights Reserved. "
    "Dragon Ball, Dragon Ball Z, Dragon Ball GT, and all logos, character names and distinctive likenesses thereof are trademarks of TOEI ANIMATION. "
    "Developed by NAMCO BANDAI Games Inc. GAME: Copyright 2007 NAMCO BANDAI Games Inc. "
    "Marketed and distributed by Atari, Inc., New York, NY. Atari and the Atari logo are trademarks owned by Atari Interactive, Inc. "
    "All other trademarks are the property of their respective owners. PlayStation and the PS Family logo are registered trademarks of Sony Computer Entertainment Inc. "
    "The ESRB rating icons are registered trademarks of the Entertainment Software Association.";
static const char kA11yTitle[] = "Dragon Ball Z: Budokai Tenkaichi 3";
static const char kA11yPressStart[] = "Press START button";
static const char *const kA11yTitleItems[] = {"New Game", "Continue"};
static const char *const kA11yDialogItems[] = {"Yes", "No"};
static const char kA11yPosition[] = "%s, %d of %d";

#endif
