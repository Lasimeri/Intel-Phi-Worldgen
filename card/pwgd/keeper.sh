#!/usr/bin/env bash
# keeper.sh: keeps pwgd running on the card. Every 30 s, when the card answers and pwgd is not running there (a card
# reboot empties its tmpfs), puts out/pwgd-card on the card and starts it. See keeper.md.
set -u
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
bin="$here/out/pwgd-card"
card=${PHI_CARD:-0}
say() { printf '%s keeper: %s\n' "$(date '+%F %T')" "$*"; }
while :; do
    if [ "$(phictl --card "$card" exec -- /bin/echo up 2>/dev/null)" = up ]; then
        if [ -z "$(phictl --card "$card" exec -- /bin/pidof pwgd 2>/dev/null)" ]; then
            if phictl --card "$card" exec -- /bin/mkdir -p /tmp/pwg > /dev/null 2>&1 \
                && phictl --card "$card" put "$bin" /tmp/pwg/pwgd --mode 755 > /dev/null 2>&1 \
                && phictl --card "$card" exec -- /bin/sh -c \
                    'cd /tmp/pwg && setsid nohup ./pwgd -p 25600 -w 120 -s 1024 > /tmp/pwg/pwgd.log 2>&1 < /dev/null &' \
                    > /dev/null 2>&1; then
                say "pwgd started on card $card: $(phictl --card "$card" exec -- /bin/pidof pwgd 2>/dev/null)"
            else
                say "could not start pwgd on card $card"
            fi
        fi
    fi
    sleep 30
done
