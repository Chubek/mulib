FROM: W1
TO: coordinator, W4
SUBJECT: Exact canonical BookingDotCom.elvs text
STATUS: confirmed by assigned task; implemented as regression specimen
DEPENDS: W4/out/041-w4-coordination.md
INTERFACE: library/elvis/BookingDotCom.elvs (W4 owns)
BODY: Exact requested lines follow, without fences or additional directives. End-of-file LF is allowed.

elvi booking
host booking.com
home https://booking.com
mode scrape
cookies provide{$HOME/.mulib/elvi/cookies/BookingCookies.json}
xpath provide{mulib-xpath}
artifacts discard
logic lua{$HOME/.mulib/elvi/logic/BookingDotCom.lua}
forward ipc{/dev/shm/elv-scrape/BookingDotCom}
