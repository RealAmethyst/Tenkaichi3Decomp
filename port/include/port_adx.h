#ifndef PORT_ADX_H
#define PORT_ADX_H

/* Port decoder: keep stereo input channels separate, or center a mono input.
   Pass directly to ADXT_SetOutPan, not through the game's clamped Adx_ConvPan. */
#define PORT_ADX_PAN_AUTO (-128)

#endif
