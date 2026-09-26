#!/bin/zsh
# Nonstop Mac campaign. Alternates tight near-miss reruns with diversified wide reruns (gap <= 15, record < 80),
# rotating scoring settings so splicing can combine different paths.
cd ~/acc/acsolver
while pgrep -f "campaign.sh" >/dev/null; do sleep 60; done     # let a running round finish
N=${1:-20}
SETTINGS=("--junk 4 --look-pen 16" "--junk 2 --look-pen 8" "--junk 8 --look-pen 32" "--junk 4 --look-pen 16 --power 0" "--junk 4 --look 0")
while true; do
  for S in $SETTINGS; do
    GMAX=3 EXTRA="$S" ./campaign.sh $N 8000 $((1000 + N)) || print "campaign $N failed" >> results/campaign.log; N=$((N + 1))
    GMAX=15 RMAX=80 EXTRA="$S" ./campaign.sh $N 12000 $((1000 + N)) || print "campaign $N failed" >> results/campaign.log; N=$((N + 1))
  done
done
