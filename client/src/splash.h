#ifndef SPLASH_H
#define SPLASH_H

/* show_splash_hires() — lo-res color splash screen on the Apple II.
   Uses the text page ($0400-$07FF) for 40x48 pixel / 16-color display.
   Switches to 40-col lo-res, draws the logo, waits for a key, then
   returns to 80-col text mode for normal program operation.

   Hi-res ($2000-$5FFF) cannot be used because the 24 KB program
   occupies those addresses.  Lo-res is the correct choice: it lives
   entirely below the program start at $0803.                          */
void show_splash_hires(void);

#endif /* SPLASH_H */
