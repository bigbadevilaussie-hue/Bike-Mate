#!/bin/bash
# Bike-Mate handoff file generator
# Run this before starting a new chat.
# Then: cat HANDOFF.md

cd ~/Documents/Arduino/bike_mate

{
  echo "# Bike-Mate — Chat Handoff"
  echo ""
  echo "Generated: $(date '+%Y-%m-%d %H:%M:%S')"
  echo ""
  echo "---"
  echo ""
  echo "## GIT STATE"
  echo ""
  echo '```'
  echo "\$ git log --oneline -5"
  git log --oneline -5
  echo ""
  echo "\$ git status --short"
  git status --short
  echo ""
  echo "\$ git tag -l | tail -10"
  git tag -l | tail -10
  echo '```'
  echo ""
  echo "---"
  echo ""
  echo "## PROJECT_STATE.md"
  echo ""
  cat PROJECT_STATE.md
  echo ""
  echo "---"
  echo ""
  echo "## CURRENT FILE LIST"
  echo ""
  echo '```'
  ls -la *.h *.cpp *.ino *.py 2>/dev/null
  echo '```'
  echo ""
  echo "---"
  echo ""
  echo "## LATEST SERIAL SNAPSHOT (optional — paste manually)"
  echo ""
  echo "_Paste last serial output here if needed._"
  echo ""
} > HANDOFF.md

echo "HANDOFF.md updated ($(wc -l < HANDOFF.md) lines)"
echo ""
echo "To paste into new chat:"
echo "  cat ~/Documents/Arduino/bike_mate/HANDOFF.md | pbcopy"
