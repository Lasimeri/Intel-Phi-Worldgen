# keeper.sh

Keeps pwgd running on the card. Every 30 s it asks the card to echo `up`
through phictl. When the card answers and `pidof pwgd` finds nothing, it:
1. creates `/tmp/pwg`;
2. puts `out/pwgd-card` there;
3. starts pwgd detached with the defaults.

A card reboot empties the card's tmpfs, and the card's init has no hook for
extra services; this way pwgd lives on the card without changes to the
Intel Phi 7120A tree's boot image.

It runs as the user unit `phiwg-pwgd`, restarted by systemd when it exits:

```
systemd-run --user --unit=phiwg-pwgd --collect --property=Restart=always --property=RestartSec=10 \
    bash ~/Intel\ Phi\ Worldgen/card/pwgd/keeper.sh
```

`PHI_CARD` picks the card (default 0).
