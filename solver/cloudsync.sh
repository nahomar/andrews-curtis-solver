#!/bin/zsh
# Every 30 min: keep the spot VM alive (restart + resume after preemption) and pull its results home.
cd ~/acc/acsolver
Z=$(cat .gcp_zone); P=$(cat .gcp_project)
log() { print -r -- "[$(date '+%m-%d %H:%M')] $*" >> results/cloudsync.log; }
while true; do
  FOCUS_GMAX=8 python3 tools.py focus results/cloud_targets.txt > /dev/null 2>&1   # fresh targets for the perpetual cloud workers
  python3 -c "
import json;d=json.load(open('best_ac.json'))
ids=[l.split()[0] for l in open('results/cloud_targets.txt')]
open('results/cloud_paths.jsonl','w').writelines(json.dumps({'id':i,'solved':True,'moves':d[i]['moves']})+chr(10) for i in ids if i in d)"   # our best paths, for cloud-side polishing
  for VM in acc1 acc2; do
    gcloud compute scp results/cloud_targets.txt "${VM}:acs/targets.txt" --zone=$Z --project=$P --quiet >> results/cloudsync.log 2>&1
    gcloud compute scp results/cloud_paths.jsonl "${VM}:acs/paths.jsonl" --zone=$Z --project=$P --quiet >> results/cloudsync.log 2>&1
    st=$(gcloud compute instances describe $VM --zone=$Z --project=$P --format='value(status)' 2>&1)
    if [ "$st" != "RUNNING" ]; then
      log "$VM status $st -> starting"
      gcloud compute instances start $VM --zone=$Z --project=$P --quiet >> results/cloudsync.log 2>&1
      sleep 60
      gcloud compute ssh $VM --zone=$Z --project=$P --quiet --command="acs/resume.sh" -- -T -n -o ConnectTimeout=20 -o ServerAliveInterval=15 -o ServerAliveCountMax=4 >> results/cloudsync.log 2>&1
    fi
    # bundle only result files changed since the last sync, one transfer per VM
    gcloud compute ssh $VM --zone=$Z --project=$P --quiet --command="cd acs/results && touch -a .synced && find . -maxdepth 1 \\( -name '*_ac.jsonl' -o -name '*_sac.jsonl' \\) -newer .synced > /tmp/chg.txt; [ -s /tmp/chg.txt ] && tar czf /tmp/chg.tgz -T /tmp/chg.txt; touch .synced.new; cat /tmp/chg.txt | wc -l" -- -T -n -o ConnectTimeout=20 >> results/cloudsync.log 2>&1
    rm -rf /tmp/acc_sync_$VM && mkdir -p /tmp/acc_sync_$VM
    if gcloud compute scp "${VM}:/tmp/chg.tgz" /tmp/acc_sync_$VM/ --zone=$Z --project=$P --quiet >> results/cloudsync.log 2>&1; then
      tar xzf /tmp/acc_sync_$VM/chg.tgz -C /tmp/acc_sync_$VM && for f in /tmp/acc_sync_$VM/*.jsonl(N); do cp "$f" "results/${VM}_$(basename $f)"; done
      gcloud compute ssh $VM --zone=$Z --project=$P --quiet --command="cd acs/results && mv .synced.new .synced && rm -f /tmp/chg.tgz" -- -T -n -o ConnectTimeout=20 >> results/cloudsync.log 2>&1
    fi
    log "$VM synced"
  done
  sleep 1800
done
