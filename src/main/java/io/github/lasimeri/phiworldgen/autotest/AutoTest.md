# AutoTest.java

Singleplayer test runs with nobody at the keyboard, client side, for the
integrated server a player's game runs. It is off unless `[autotest] runs`
is above 0.

**Each run:**
1. From the title screen (vanilla's or Custom Main Menu's), make a creative world of `worldType` with a seed.
   - With `cardModes=alternate`, runs come in pairs on one seed, card off and on, the order alternating by pair.
2. Once the player is in, on the server thread:
   1. log the census;
   2. optionally capture RWG's height stage (`capture`);
   3. card runs wait up to 30 s for the card;
   4. reset the card statistics;
   5. run `phiwg pregen radius` (with `dump`, also the world dump);
   6. append one line to `~/.cache/phiworldgen/autotest-summary.log`;
   7. log the card status.
3. Save and quit to the title screen.

The game quits after the last run.

**It never touches a world it did not make.** Only the world whose folder it
launched is tested. This came after it once took over a world the user had
opened by hand.

**The first world in a fresh JVM runs with a cold JIT.** It was 44 % slower
in provideChunk and 62 % in populate. Its numbers are a warm-up, not a
measurement.
