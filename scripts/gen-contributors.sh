#!/usr/bin/env bash
# Prints every contributor, one per line, most recent first.
#   sh scripts/gen-contributors.sh > CONTRIBUTORS
cd "$(dirname "$0")/.." || exit 1
git log --format='%aN' |
  grep -v '\[bot\]' |
  sed \
    -e 's/^Vaishnav-Sabari-Girish$/Vaishnav Sabari Girish/' \
    -e 's/^Vaishnav-sabari-girish$/Vaishnav Sabari Girish/' \
    -e 's/^vaishnav sabari girish$/Vaishnav Sabari Girish/' |
  awk 'NF && !seen[$0]++'
