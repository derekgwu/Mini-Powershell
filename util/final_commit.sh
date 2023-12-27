#!/bin/sh

echo "By committing with this script, you're agreeing that you've\n1. Tested your code on the class' server.\n2. You've done 'git status' and 'git diff --stat' to make sure that all necessary changes are in git.\n3. You've tested your code after a 'git pull' to ensure you're submitting up-to-date code.\n4. You'll immediately do a 'git push' after this command."
echo "You can run this command multiple times, but it should *not* be a replacement for normal commits with descriptive commit messages."
echo "Remember: you must still do a final 'git push' after running this script."

git commit --allow-empty -a -m "Final commit:" -m "Who: `git config --get user.name`, `git config --get user.email`" -m "Where: `hostname`" -m "When: `date`"
